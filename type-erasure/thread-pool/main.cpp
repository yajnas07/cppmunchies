// =============================================================================
// type_erasure_thread_pool.cpp
//
// Demonstrates: Type Erasure via a thread pool that accepts any callable —
// lambdas, free functions, functors, std::bind — without knowing their types.
//
// Concepts shown:
//   - CallableBase / CallableModel<T> — the type erasure engine
//   - ThreadPool — owns a queue of type-erased tasks
//   - Workers that drain the queue on background threads
//   - Return values via std::future + std::promise
//
// Build (C++17):
//   g++ -std=c++17 -O2 -pthread type_erasure_thread_pool.cpp -o thread_pool_demo
//
// Author: CodePuz  |  codepuz.com
// =============================================================================
#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <chrono>
#include <cassert>
#include <sstream>
#include <numeric>
#include <utility>
// =============================================================================
// PART 1: Type Erasure Engine
//
// We build our own simplified type-erased callable so you can see exactly
// what std::function does under the hood.
// =============================================================================
// — The abstract interface (the "erased" view) —
struct CallableBase {
virtual void call() = 0;
virtual ~CallableBase() = default;
};
// — The template wrapper (where the compiler stamps out concrete types) —
//
// For each T you pass into the pool, the compiler instantiates a fresh
// CallableModel<T> with:
//   - A stored copy of T
//   - An overridden call() that invokes T::operator()()
//
// After construction, nobody outside needs to know T anymore.
template<typename T>
struct CallableModel : CallableBase {
T callable_;
explicit CallableModel(T t) : callable_(std::move(t)) {}
void call() override { callable_(); }
};
// — The owning wrapper (the public API) —
//
// TypeErasedTask holds a CallableBase* and exposes operator().
// It is move-only because we want unique ownership of the callable.
class TypeErasedTask {
CallableBase* ptr_ = nullptr;
public:
TypeErasedTask() = default;

// Type-erasure point: any callable T becomes a CallableModel<T>
template<typename T>
explicit TypeErasedTask(T callable)
   : ptr_(new CallableModel<T>(std::move(callable))) {}
// Move-only — no copying
TypeErasedTask(const TypeErasedTask&) = delete;
TypeErasedTask& operator=(const TypeErasedTask&) = delete;
TypeErasedTask(TypeErasedTask&& other) noexcept
   : ptr_(std::exchange(other.ptr_, nullptr)) {}
TypeErasedTask& operator=(TypeErasedTask&& other) noexcept {
   delete ptr_;
   ptr_ = std::exchange(other.ptr_, nullptr);
   return *this;
}
void operator()() { if (ptr_) ptr_->call(); }
~TypeErasedTask() { delete ptr_; }

};
// =============================================================================
// PART 2: Thread Pool
//
// Owns N worker threads. Accepts any callable via enqueue().
// The callable's type is erased at the enqueue boundary — workers
// only see TypeErasedTask; they never know what the real callable is.
// =============================================================================
class ThreadPool {
public:
explicit ThreadPool(size_t num_threads) : stop_(false) {
for (size_t i = 0; i < num_threads; ++i) {
workers_.emplace_back([this] { worker_loop(); });
}
}

// enqueue(callable) — accepts ANY callable with signature void()
//
// The template T is deduced from the argument. A CallableModel<T> is
// created here, inside this template. After this function returns,
// the pool's queue holds only TypeErasedTask objects — T is gone.
template<typename F>
void enqueue(F&& task) {
   {
       std::unique_lock<std::mutex> lock(mtx_);
       queue_.push(TypeErasedTask(std::forward<F>(task)));
   }
   cv_.notify_one();
}
// enqueue_with_result: returns a future so the caller can get a value back.
// The callable F must return a value of type R.
//
// We wrap F in a lambda that sets a promise — the lambda is what gets
// type-erased into a TypeErasedTask. R and F are both invisible to the pool.
template<typename F, typename R = std::invoke_result_t<F>>
std::future<R> enqueue_with_result(F&& task) {
   auto promise = std::make_shared<std::promise<R>>();
   std::future<R> future = promise->get_future();
   enqueue([p = promise, t = std::forward<F>(task)]() mutable {
       try {
           if constexpr (std::is_void_v<R>) {
               t();
               p->set_value();
           } else {
               p->set_value(t());
           }
       } catch (...) {
           p->set_exception(std::current_exception());
       }
   });
   return future;
}
// Drain the queue and stop all workers
~ThreadPool() {
   {
       std::unique_lock<std::mutex> lock(mtx_);
       stop_ = true;
   }
   cv_.notify_all();
   for (auto& w : workers_) w.join();
}
size_t queue_size() const {
   std::unique_lock<std::mutex> lock(mtx_);
   return queue_.size();
}

private:
void worker_loop() {
while (true) {
TypeErasedTask task;
{
std::unique_lock<std::mutex> lock(mtx_);
cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
if (stop_ && queue_.empty()) return;
task = std::move(queue_.front());
queue_.pop();
}
task(); // concrete type was erased; we just call it
}
}

std::vector<std::thread>       workers_;
std::queue<TypeErasedTask>     queue_;
mutable std::mutex             mtx_;
std::condition_variable        cv_;
bool                           stop_;

};
// =============================================================================
// PART 3: Test Bench
//
// Five test scenarios that exercise the type erasure from different angles.
// =============================================================================
// Utility: thread-safe logger for test output
class SafeLogger {
std::mutex mtx_;
public:
template<typename... Args>
void log(Args&&... args) {
std::unique_lock<std::mutex> lock(mtx_);
(std::cout << ... << std::forward<Args>(args)) << '\n';
}
};
SafeLogger g_log;
// A named functor — a callable object with state, a distinct type
struct NumberCruncher {
int multiplier;
int input;
std::atomic<int>* result;

void operator()() const {
   int val = multiplier * input;
   result->store(val, std::memory_order_relaxed);
   g_log.log("  [NumberCruncher] ", input, " x ", multiplier, " = ", val);
}

};
// A plain free function — another distinct type
void log_separator() {
g_log.log("  [FreeFunction] - separator task ran -");
}
// — Test 1: Mix of lambdas, free functions, and functors —
void test_heterogeneous_callables() {
g_log.log("\n=== Test 1: Heterogeneous callables ===");
ThreadPool pool(2);
std::atomic<int> result{0};

// Lambda — type: unique compiler-generated closure type
pool.enqueue([&] {
   g_log.log("  [Lambda] hello from a lambda");
});
// Free function pointer — type: void(*)()
pool.enqueue(&log_separator);
// Functor — type: NumberCruncher
pool.enqueue(NumberCruncher{7, 6, &result});
// Another lambda — a *different* closure type from the first
pool.enqueue([&] {
   g_log.log("  [Lambda2] I am a different closure type from Lambda1");
});
// Give workers time to drain
std::this_thread::sleep_for(std::chrono::milliseconds(100));
assert(result.load() == 42);
g_log.log("  PASS: NumberCruncher result == 42");

}
// — Test 2: Return values via future —
void test_return_values() {
g_log.log("\n=== Test 2: Return values via std::future ===");
ThreadPool pool(2);

// Enqueue tasks that return values — types are fully erased inside the pool
auto f1 = pool.enqueue_with_result([] { return 100; });
auto f2 = pool.enqueue_with_result([] { return std::string("hello from future"); });
auto f3 = pool.enqueue_with_result([] {
   std::this_thread::sleep_for(std::chrono::milliseconds(10));
   return 3.14159;
});
g_log.log("  f1 (int)    = ", f1.get());
g_log.log("  f2 (string) = ", f2.get());
g_log.log("  f3 (double) = ", f3.get());

}
// — Test 3: Exception propagation through type erasure —
void test_exception_propagation() {
g_log.log("\n=== Test 3: Exception propagation ===");
ThreadPool pool(1);

auto f = pool.enqueue_with_result([]  {
   throw std::runtime_error("intentional error from inside type-erased task");
   return 0;
});
try {
   int val = f.get();  // rethrows the stored exception
   (void)val;
   g_log.log("  FAIL: expected exception was not thrown");
} catch (const std::runtime_error& e) {
   g_log.log("  PASS: caught exception: ", e.what());
}

}
// — Test 4: High-volume throughput —
void test_throughput() {
g_log.log("\n=== Test 4: Throughput - 10000 tasks, 4 workers ===");
ThreadPool pool(4);

const int N = 10000;
std::atomic<int> counter{0};
std::vector<std::future<void>> futures;
auto t_start = std::chrono::steady_clock::now();
for (int i = 0; i < N; ++i) {
   futures.push_back(pool.enqueue_with_result([&counter] {
       counter.fetch_add(1, std::memory_order_relaxed);
   }));
}
for (auto& f : futures) f.get();
auto t_end = std::chrono::steady_clock::now();
auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
assert(counter.load() == N);
g_log.log("  PASS: ", N, " tasks completed in ", ms, " ms - counter = ", counter.load());

}
// — Test 5: Capturing large state (forces heap alloc beyond SBO) —
void test_large_capture() {
g_log.log("\n=== Test 5: Large captured state ===");
ThreadPool pool(2);

// A vector with 1000 ints — far too large for any SBO buffer.
// The type erasure still works correctly; it just heap-allocates the model.
std::vector<int> big_data(1000);
std::iota(big_data.begin(), big_data.end(), 1);  // 1..1000
auto f = pool.enqueue_with_result([big_data] {  // captured by value
   return std::accumulate(big_data.begin(), big_data.end(), 0);
});
int sum = f.get();
assert(sum == 500500);  // sum of 1..1000
g_log.log("  PASS: sum of 1..1000 = ", sum);

}
// =============================================================================
// PART 4: Examples — practical usage walkthrough
//
// Run by default (no flags). Shows how each class is meant to be used in
// realistic scenarios.
// =============================================================================
void run_examples() {
    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 1: Enqueue a mix of callable types\n";
    std::cout << "--------------------------------------------------\n";
    {
        // A ThreadPool accepts ANY callable — lambda, free function, functor —
        // because each is wrapped into a TypeErasedTask at enqueue().
        // Workers only ever see TypeErasedTask; the concrete type is gone.
        ThreadPool pool(2);
        std::atomic<int> shared{0};

        // Lambda — unique compiler-generated closure type
        pool.enqueue([&shared] {
            shared.fetch_add(1, std::memory_order_relaxed);
            g_log.log("  [lambda]   ran, shared = ", shared.load());
        });

        // Free function pointer — type void(*)()
        pool.enqueue(&log_separator);

        // Functor with state — type NumberCruncher
        std::atomic<int> product{0};
        pool.enqueue(NumberCruncher{6, 7, &product});

        // Another lambda — a DIFFERENT closure type from the first
        pool.enqueue([&shared] {
            shared.fetch_add(10, std::memory_order_relaxed);
            g_log.log("  [lambda2]  ran, shared = ", shared.load());
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        g_log.log("  product (6x7) = ", product.load());
        g_log.log("  shared counter = ", shared.load(), "  (two lambdas added 1 + 10)");
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 2: Get return values via std::future\n";
    std::cout << "--------------------------------------------------\n";
    {
        // enqueue_with_result() wraps any callable that returns a value.
        // The return type R is erased from the pool — only the future survives.
        ThreadPool pool(2);

        auto f_int    = pool.enqueue_with_result([] { return 42; });
        auto f_string = pool.enqueue_with_result([] {
            return std::string("computed on a worker thread");
        });
        auto f_double = pool.enqueue_with_result([] {
            double acc = 0.0;
            for (int i = 1; i <= 100; ++i) acc += 1.0 / i;
            return acc;  // harmonic series partial sum
        });

        g_log.log("  int result    = ", f_int.get());
        g_log.log("  string result = ", f_string.get());
        g_log.log("  double result = ", f_double.get(), "  (harmonic sum 1..100)");
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 3: Exception propagation through the pool\n";
    std::cout << "--------------------------------------------------\n";
    {
        // Exceptions thrown inside a type-erased task are captured by the
        // promise and rethrown when the caller calls future.get().
        ThreadPool pool(1);

        auto f = pool.enqueue_with_result([] {
            throw std::runtime_error("error from inside a worker thread");
            return 0;
        });

        try {
            int v = f.get();  // rethrows here
            (void)v;
        } catch (const std::runtime_error& e) {
            g_log.log("  caught on main thread: ", e.what(), "  [ok]");
        }
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 4: Parallel map - process a vector concurrently\n";
    std::cout << "--------------------------------------------------\n";
    {
        // Use the pool to square each element of a vector in parallel.
        // Each task captures its index by value — a different closure each time.
        ThreadPool pool(4);
        const int N = 8;
        std::vector<int> input(N);
        std::iota(input.begin(), input.end(), 1);   // 1..8
        std::vector<std::future<int>> results;

        for (int x : input) {
            results.push_back(pool.enqueue_with_result([x] { return x * x; }));
        }

        std::cout << "  squares: ";
        for (auto& f : results) std::cout << f.get() << " ";
        std::cout << "\n";
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 5: Large captured state (heap-allocated model)\n";
    std::cout << "--------------------------------------------------\n";
    {
        // When a lambda captures a large object, the TypeErasedTask heap-
        // allocates the CallableModel<T>. The API is identical — type erasure
        // abstracts away the allocation strategy completely.
        ThreadPool pool(2);

        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 1);  // 1..1000

        auto f = pool.enqueue_with_result([data] {  // captured by value
            return std::accumulate(data.begin(), data.end(), 0LL);
        });

        g_log.log("  sum of 1..1000 = ", f.get(), "  (expected 500500)");
    }

    std::cout << "\n";
}

// =============================================================================
// PART 5: main
// =============================================================================
static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [-test]\n\n"
              << "  (no flags)  Run usage examples that show the API in action\n"
              << "  -test       Run the full test suite with assertions\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================\n";
    std::cout << " Type Erasure - Thread Pool Demo\n";
    std::cout << " codepuz.com\n";
    std::cout << "=================================================\n";

    // -- parse flags --
    bool run_tests = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-test" || arg == "--test") {
            run_tests = true;
        } else {
            std::cerr << "Unknown flag: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (run_tests) {
        std::cout << "\nRunning test suite...\n";
        test_heterogeneous_callables();
        test_return_values();
        test_exception_propagation();
        test_throughput();
        test_large_capture();
        std::cout << "\nAll tests passed.\n";
    } else {
        run_examples();
    }

    return 0;
}