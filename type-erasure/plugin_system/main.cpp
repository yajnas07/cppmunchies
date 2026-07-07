// =============================================================================
// type_erasure_plugin_system.cpp
//
// Demonstrates: Type Erasure via a plugin system — dynamically registered
// plugins of completely different concrete types, all invoked through a
// single uniform interface without the host knowing the concrete types.
//
// Concepts shown:
//   - IPlugin: the type-erased interface (the "concept")
//   - PluginRegistry: owns and dispatches to type-erased plugins
//   - PluginHandle<T>: RAII wrapper for plugin lifetime
//   - Plugin pipeline: chain plugins, short-circuit on failure
//   - Versioned plugin interface with capability discovery
//   - Four concrete plugins: Logger, Validator, Transformer, Profiler
//
// In a real application plugins would live in shared libraries (.so / .dll)
// loaded via dlopen/LoadLibrary. Here we simulate that with in-process
// registration so the example is self-contained and portable.
//
// Build (C++17):
//   g++ -std=c++17 -O2 -Wall type_erasure_plugin_system.cpp -o plugin_demo
//
// Author: CodePuz  |  codepuz.com
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <optional>
#include <typeindex>

// =============================================================================
// PART 1: The Plugin Interface — the "erased concept"
//
// IPlugin defines the only operations the host ever calls.
// Concrete plugin types are completely hidden behind this interface.
// =============================================================================

// A generic message passed between plugins and the host.
// Real systems would use a domain-specific payload.
struct Message {
std::string topic;
std::string payload;
bool        valid  = true;
int         status = 0;


// Metadata bag — heterogeneous, also type-erased (reuse from prev example)
std::unordered_map<std::string, std::string> meta;


};

// Plugin capability flags — lets the host discover what a plugin can do
enum class Capability : uint32_t {
None        = 0,
Logging     = 1 << 0,
Validation  = 1 << 1,
Transform   = 1 << 2,
Profiling   = 1 << 3,
Filtering   = 1 << 4,
};

inline Capability operator|(Capability a, Capability b) {
return static_cast<Capability>(
static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool has_capability(Capability set, Capability flag) {
return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

// The type-erased plugin interface
class IPlugin {
public:
virtual ~IPlugin() = default;


// Lifecycle
virtual bool        initialize(const std::string& config) = 0;
virtual void        shutdown() = 0;

// Identity
virtual std::string name()    const = 0;
virtual std::string version() const = 0;
virtual Capability  capabilities() const = 0;

// Core operation — process a message, return false to halt the pipeline
virtual bool process(Message& msg) = 0;

// Optional: human-readable status for diagnostics
virtual std::string status_report() const { return "no report"; }


};

// =============================================================================
// PART 2: PluginRegistry — owns and dispatches type-erased plugins
//
// The registry stores std::unique_ptr<IPlugin>. It never knows the concrete
// type of any plugin after registration. The concrete type is erased at the
// register() call boundary.
// =============================================================================

class PluginRegistry {
public:
// register_plugin: the type-erasure point.
//
// T can be any type that derives from IPlugin. After this call the
// registry holds only IPlugin* — T is invisible. The caller can pass
// a unique_ptr to any concrete plugin type.
void register_plugin(std::unique_ptr<IPlugin> plugin) {
if (!plugin)
throw std::invalid_argument("PluginRegistry: null plugin");


    const std::string key = plugin->name();
    if (plugins_.count(key))
        throw std::runtime_error("PluginRegistry: plugin already registered: " + key);

    std::cout << "  [registry] registered '" << key
              << "' v" << plugin->version() << "\n";
    plugins_[key] = std::move(plugin);
    order_.push_back(key);
}

// Initialize all registered plugins with a config string
void initialize_all(const std::string& config = "") {
    for (auto& key : order_) {
        auto& p = plugins_.at(key);
        bool ok = p->initialize(config);
        std::cout << "  [registry] init '" << key
                  << "' -> " << (ok ? "OK" : "FAILED") << "\n";
        if (!ok)
            throw std::runtime_error("Plugin init failed: " + key);
    }
}

// Run a message through all plugins in registration order.
// Returns false (and stops) if any plugin signals halt.
bool process(Message& msg) const {
    for (auto& key : order_) {
        auto& p = plugins_.at(key);
        if (!p->process(msg)) {
            std::cout << "  [pipeline] halted by '" << key << "'\n";
            return false;
        }
    }
    return true;
}

// Run only plugins that have a specific capability
bool process_capable(Message& msg, Capability cap) const {
    for (auto& key : order_) {
        auto& p = plugins_.at(key);
        if (has_capability(p->capabilities(), cap)) {
            if (!p->process(msg)) return false;
        }
    }
    return true;
}

// Lookup by name — returns nullptr if not found
IPlugin* find(const std::string& name) const {
    auto it = plugins_.find(name);
    return it == plugins_.end() ? nullptr : it->second.get();
}

void shutdown_all() {
    // Shutdown in reverse registration order
    for (auto it = order_.rbegin(); it != order_.rend(); ++it)
        plugins_.at(*it)->shutdown();
}

void print_status() const {
    std::cout << "  [registry] " << plugins_.size() << " plugin(s):\n";
    for (auto& key : order_) {
        auto& p = plugins_.at(key);
        std::cout << "    - " << key << " v" << p->version()
                  << " | " << p->status_report() << "\n";
    }
}

size_t size() const { return plugins_.size(); }


private:
std::unordered_map<std::string, std::unique_ptr<IPlugin>> plugins_;
std::vector<std::string> order_;   // preserves registration order
};

// =============================================================================
// PART 3: Concrete Plugin Implementations
//
// These are the real types — completely unknown to the registry after
// registration. Each has its own state, logic, and member variables.
// The registry only ever calls IPlugin virtual methods.
// =============================================================================

// ── Plugin 1: Logger ─────────────────────────────────────────────────────────
class LoggerPlugin : public IPlugin {
std::vector<std::string> log_;
bool                     verbose_ = false;
int                      count_   = 0;

public:
bool initialize(const std::string& config) override {
verbose_ = (config.find("verbose") != std::string::npos);
std::cout << "    LoggerPlugin: init (verbose=" << verbose_ << ")\n";
return true;
}
void shutdown() override {
std::cout << "    LoggerPlugin: shutdown — logged " << count_ << " message(s)\n";
}


std::string name()    const override { return "Logger"; }
std::string version() const override { return "1.0"; }
Capability  capabilities() const override { return Capability::Logging; }

bool process(Message& msg) override {
    ++count_;
    std::string entry = "[" + std::to_string(count_) + "] "
                      + msg.topic + ": " + msg.payload;
    log_.push_back(entry);
    if (verbose_)
        std::cout << "    LoggerPlugin: " << entry << "\n";
    return true;   // never halts the pipeline
}

std::string status_report() const override {
    return "logged=" + std::to_string(count_) + " entries";
}

// Plugin-specific method (not on IPlugin) — accessible if you know the type
const std::vector<std::string>& get_log() const { return log_; }


};

// ── Plugin 2: Validator ───────────────────────────────────────────────────────
class ValidatorPlugin : public IPlugin {
size_t max_payload_len_ = 256;
int    rejected_        = 0;
int    accepted_        = 0;

public:
bool initialize(const std::string&) override {
std::cout << "    ValidatorPlugin: init (max_len=" << max_payload_len_ << ")\n";
return true;
}
void shutdown() override {
std::cout << "    ValidatorPlugin: shutdown — accepted=" << accepted_
<< " rejected=" << rejected_ << "\n";
}


std::string name()    const override { return "Validator"; }
std::string version() const override { return "2.1"; }
Capability  capabilities() const override { return Capability::Validation; }

bool process(Message& msg) override {
    // Rule 1: topic must not be empty
    if (msg.topic.empty()) {
        msg.valid  = false;
        msg.status = 400;
        ++rejected_;
        return false;  // halt pipeline
    }
    // Rule 2: payload length limit
    if (msg.payload.size() > max_payload_len_) {
        msg.valid  = false;
        msg.status = 413;
        ++rejected_;
        return false;
    }
    // Rule 3: no null bytes in payload
    if (msg.payload.find('\0') != std::string::npos) {
        msg.valid  = false;
        msg.status = 422;
        ++rejected_;
        return false;
    }
    ++accepted_;
    return true;
}

std::string status_report() const override {
    return "accepted=" + std::to_string(accepted_)
         + " rejected=" + std::to_string(rejected_);
}


};

// ── Plugin 3: Transformer ─────────────────────────────────────────────────────
class TransformerPlugin : public IPlugin {
bool  uppercase_topic_   = true;
bool  trim_payload_      = true;
int   transforms_applied_ = 0;


static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}


public:
bool initialize(const std::string& config) override {
uppercase_topic_ = (config.find("no_upper") == std::string::npos);
std::cout << "    TransformerPlugin: init (uppercase_topic="
<< uppercase_topic_ << ")\n";
return true;
}
void shutdown() override {
std::cout << "    TransformerPlugin: shutdown — applied "
<< transforms_applied_ << " transform(s)\n";
}


std::string name()    const override { return "Transformer"; }
std::string version() const override { return "1.3"; }
Capability  capabilities() const override { return Capability::Transform; }

bool process(Message& msg) override {
    if (uppercase_topic_) msg.topic   = to_upper(msg.topic);
    if (trim_payload_)    msg.payload = trim(msg.payload);
    msg.meta["transformed"] = "true";
    ++transforms_applied_;
    return true;
}

std::string status_report() const override {
    return "transforms=" + std::to_string(transforms_applied_);
}


};

// ── Plugin 4: Profiler ────────────────────────────────────────────────────────
// Note: Profiler has rich internal state (timing histogram) invisible to host
class ProfilerPlugin : public IPlugin {
using Clock    = std::chrono::steady_clock;
using Duration = std::chrono::microseconds;


struct Record {
    std::string topic;
    long        elapsed_us;
};

std::vector<Record>      records_;
Clock::time_point        start_;
bool                     timing_active_ = false;


public:
bool initialize(const std::string&) override {
std::cout << "    ProfilerPlugin: init\n";
return true;
}
void shutdown() override {
std::cout << "    ProfilerPlugin: shutdown — " << records_.size()
<< " record(s)\n";
}


std::string name()    const override { return "Profiler"; }
std::string version() const override { return "3.0"; }
Capability  capabilities() const override { return Capability::Profiling; }

bool process(Message& msg) override {
    auto t0  = Clock::now();
    // Simulate some measurement work
    volatile int dummy = 0;
    for (int i = 0; i < 1000; ++i) dummy += i;
    auto t1  = Clock::now();
    auto us  = std::chrono::duration_cast<Duration>(t1 - t0).count();

    records_.push_back({msg.topic, us});
    msg.meta["profiler_us"] = std::to_string(us);
    return true;
}

std::string status_report() const override {
    if (records_.empty()) return "no samples";
    long total = 0;
    for (auto& r : records_) total += r.elapsed_us;
    return "samples=" + std::to_string(records_.size())
         + " avg_us=" + std::to_string(total / (long)records_.size());
}

// Plugin-specific method — only accessible if caller knows concrete type
const std::vector<Record>& records() const { return records_; }


};

// =============================================================================
// PART 4: Test Bench
// =============================================================================

void separator(const std::string& title) {
std::cout << "\n=== " << title << " ===\n";
}

// ── Test 1: Basic pipeline — all four plugins, valid message ─────────────────
void test_basic_pipeline() {
separator("Test 1: Basic pipeline");


PluginRegistry registry;
registry.register_plugin(std::make_unique<LoggerPlugin>());
registry.register_plugin(std::make_unique<ValidatorPlugin>());
registry.register_plugin(std::make_unique<TransformerPlugin>());
registry.register_plugin(std::make_unique<ProfilerPlugin>());

registry.initialize_all("verbose");

Message msg;
msg.topic   = "sensor_data";
msg.payload = "  temperature=42.5  ";   // leading/trailing spaces

bool ok = registry.process(msg);

assert(ok);
assert(msg.valid);
assert(msg.topic   == "SENSOR_DATA");   // Transformer uppercased
assert(msg.payload == "temperature=42.5");  // Transformer trimmed
assert(msg.meta.count("transformed"));
assert(msg.meta.count("profiler_us"));

std::cout << "  topic   : " << msg.topic   << "\n";
std::cout << "  payload : " << msg.payload << "\n";
std::cout << "  profiler: " << msg.meta["profiler_us"] << " us\n";

registry.print_status();
registry.shutdown_all();

std::cout << "  PASS: valid message processed through all 4 plugins\n";


}

// ── Test 2: Validator halts pipeline on empty topic ───────────────────────────
void test_validation_halt() {
separator("Test 2: Validator halts pipeline on invalid message");


PluginRegistry registry;
registry.register_plugin(std::make_unique<LoggerPlugin>());
registry.register_plugin(std::make_unique<ValidatorPlugin>());
registry.register_plugin(std::make_unique<TransformerPlugin>());

registry.initialize_all();

Message msg;
msg.topic   = "";                // invalid — empty topic
msg.payload = "some data";

bool ok = registry.process(msg);

assert(!ok);
assert(!msg.valid);
assert(msg.status == 400);
// Transformer must NOT have run (pipeline was halted)
assert(msg.meta.find("transformed") == msg.meta.end());

std::cout << "  status : " << msg.status << " (expected 400)\n";
std::cout << "  PASS: pipeline halted correctly, Transformer did not run\n";

registry.shutdown_all();


}

// ── Test 3: Payload too long ──────────────────────────────────────────────────
void test_payload_too_long() {
separator("Test 3: Validator rejects oversized payload");


PluginRegistry registry;
registry.register_plugin(std::make_unique<ValidatorPlugin>());
registry.initialize_all();

Message msg;
msg.topic   = "upload";
msg.payload = std::string(512, 'x');   // 512 bytes > 256 limit

bool ok = registry.process(msg);
assert(!ok);
assert(msg.status == 413);
std::cout << "  status : " << msg.status << " (expected 413 Payload Too Large)\n";
std::cout << "  PASS\n";

registry.shutdown_all();


}

// ── Test 4: Multiple messages through same registry ───────────────────────────
void test_multiple_messages() {
separator("Test 4: Multiple messages — logger accumulates state");


auto logger_raw = std::make_unique<LoggerPlugin>();
LoggerPlugin* logger_ptr = logger_raw.get();   // keep raw ptr before move

PluginRegistry registry;
registry.register_plugin(std::move(logger_raw));
registry.register_plugin(std::make_unique<ValidatorPlugin>());
registry.initialize_all();

std::vector<std::pair<std::string,std::string>> inputs = {
    {"ping",    "hello"},
    {"status",  "ok"},
    {"metrics", "cpu=12 mem=44"},
    {"",        "bad — empty topic"},   // will be rejected
    {"data",    "payload_five"},
};

int passed = 0, rejected = 0;
for (auto& [topic, payload] : inputs) {
    Message msg;
    msg.topic   = topic;
    msg.payload = payload;
    if (registry.process(msg)) ++passed;
    else                       ++rejected;
}

assert(passed   == 4);
assert(rejected == 1);
// Logger is registered before Validator so it sees all 5 messages
assert(logger_ptr->get_log().size() == 5);

std::cout << "  passed=" << passed << " rejected=" << rejected << "\n";
std::cout << "  Logger accumulated " << logger_ptr->get_log().size()
          << " log entries (runs before Validator, sees all)\n";
std::cout << "  PASS\n";

registry.shutdown_all();


}

// ── Test 5: Capability-filtered dispatch ─────────────────────────────────────
void test_capability_filter() {
separator("Test 5: process_capable — only run Profiler");


PluginRegistry registry;
registry.register_plugin(std::make_unique<LoggerPlugin>());
registry.register_plugin(std::make_unique<ValidatorPlugin>());
registry.register_plugin(std::make_unique<ProfilerPlugin>());
registry.initialize_all();

Message msg;
msg.topic   = "perf_test";
msg.payload = "data";

// Only profiler runs — validator and logger are skipped
bool ok = registry.process_capable(msg, Capability::Profiling);

assert(ok);
// meta["profiler_us"] set by profiler
assert(msg.meta.count("profiler_us"));
// meta["transformed"] NOT set — transformer not in registry anyway
// topic NOT uppercased — transformer didn't run
assert(msg.topic == "perf_test");

std::cout << "  profiler_us: " << msg.meta["profiler_us"] << "\n";
std::cout << "  PASS: only Profiler ran, others skipped\n";

registry.shutdown_all();


}

// ── Test 6: Duplicate registration is rejected ────────────────────────────────
void test_duplicate_registration() {
separator("Test 6: Duplicate plugin registration rejected");


PluginRegistry registry;
registry.register_plugin(std::make_unique<LoggerPlugin>());
registry.initialize_all();

try {
    registry.register_plugin(std::make_unique<LoggerPlugin>());  // same name
    std::cout << "  FAIL: expected runtime_error\n";
} catch (const std::runtime_error& e) {
    std::cout << "  PASS: caught: " << e.what() << "\n";
}

registry.shutdown_all();


}

// ── Test 7: Plugin lookup and downcast ────────────────────────────────────────
void test_plugin_lookup() {
separator("Test 7: Plugin lookup by name + downcast to concrete type");


auto profiler_owned = std::make_unique<ProfilerPlugin>();

PluginRegistry registry;
registry.register_plugin(std::make_unique<LoggerPlugin>());
registry.register_plugin(std::move(profiler_owned));
registry.initialize_all();

// Process a few messages to build up profiler state
for (int i = 0; i < 3; ++i) {
    Message msg{"topic", "payload"};
    registry.process(msg);
}

// Look up the profiler by name — returns IPlugin*
IPlugin* p = registry.find("Profiler");
assert(p != nullptr);
std::cout << "  found via IPlugin*: " << p->name() << "\n";

// Downcast to access ProfilerPlugin-specific API
// This is one of the rare cases where you intentionally "unerase" the type
auto* profiler = dynamic_cast<ProfilerPlugin*>(p);
assert(profiler != nullptr);
assert(profiler->records().size() == 3);
std::cout << "  downcast OK — " << profiler->records().size()
          << " profiler records accessible\n";
std::cout << "  PASS\n";

registry.shutdown_all();


}

// =============================================================================
// PART 5: main
// =============================================================================

int main() {
std::cout << "=================================================\n";
std::cout << " Type Erasure — Plugin System Demo\n";
std::cout << " codepuz.com\n";
std::cout << "=================================================\n";


test_basic_pipeline();
test_validation_halt();
test_payload_too_long();
test_multiple_messages();
test_capability_filter();
test_duplicate_registration();
test_plugin_lookup();

std::cout << "\nAll tests passed.\n";
return 0;


}
