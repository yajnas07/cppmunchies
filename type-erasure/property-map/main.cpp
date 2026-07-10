// =============================================================================
// type_erasure_property_map.cpp
//
// Demonstrates: Type Erasure via a heterogeneous property map — a key-value
// store where values can be of *any* type, retrieved type-safely by key.
//
// Concepts shown:
//   - std::any as the type erasure vehicle for values
//   - A typed PropertyMap wrapper with get<T> / set<T> / has<T>
//   - A stricter TypedPropertyMap with compile-time registered schema
//   - A ComponentConfig use case (SystemC-style component configuration)
//   - Observer pattern on top of a property map (change notification)
//
// Build (C++17):
//   g++ -std=c++17 -O2 type_erasure_property_map.cpp -o property_map_demo
//
// Author: CodePuz  |  codepuz.com
// =============================================================================
#include <iostream>
#include <unordered_map>
#include <string>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <optional>
#include <sstream>
// =============================================================================
// PART 1: PropertyMap — the core type-erased key/value store
//
// Values are stored as std::any. The concrete type is erased at set() and
// recovered at get<T>(). Any copy-constructible type can be a value.
// =============================================================================
class PropertyMap {
public:
// ── Write ────────────────────────────────────────────────────────────────

// set<T>: type T is erased into std::any here.
// After this call, the map holds no compile-time knowledge of T.
template<typename T>
void set(const std::string& key, T value) {
   props_[key] = std::any(std::move(value));
}
// ── Read ─────────────────────────────────────────────────────────────────
// get<T>: recovers T from std::any.
// Throws std::bad_any_cast if the stored type doesn't match T.
template<typename T>
T get(const std::string& key) const {
   auto it = props_.find(key);
   if (it == props_.end())
       throw std::out_of_range("PropertyMap: key not found: " + key);
   return std::any_cast<T>(it->second);
}
// get_or: returns a default value if the key is absent or type mismatches
template<typename T>
T get_or(const std::string& key, T default_val) const noexcept {
   auto it = props_.find(key);
   if (it == props_.end()) return default_val;
   try {
       return std::any_cast<T>(it->second);
   } catch (const std::bad_any_cast&) {
       return default_val;
   }
}
// try_get: returns std::optional<T> — no exception, no default needed
template<typename T>
std::optional<T> try_get(const std::string& key) const noexcept {
   auto it = props_.find(key);
   if (it == props_.end()) return std::nullopt;
   try {
       return std::any_cast<T>(it->second);
   } catch (const std::bad_any_cast&) {
       return std::nullopt;
   }
}
// ── Query ─────────────────────────────────────────────────────────────────
bool has(const std::string& key) const {
   return props_.count(key) > 0;
}
// has<T>: true only if the key exists AND holds exactly type T
template<typename T>
bool has(const std::string& key) const {
   auto it = props_.find(key);
   if (it == props_.end()) return false;
   return it->second.type() == typeid(T);
}
void remove(const std::string& key) { props_.erase(key); }
size_t size() const { return props_.size(); }
// type_name_of: returns the stored type's name (for debugging)
std::string type_name_of(const std::string& key) const {
   auto it = props_.find(key);
   if (it == props_.end()) return "<not found>";
   return it->second.type().name();
}
// iterate all keys
template<typename F>
void for_each_key(F&& fn) const {
   for (const auto& [k, _] : props_) fn(k);
}

private:
std::unordered_map<std::string, std::any> props_;
};
// =============================================================================
// PART 2: ObservablePropertyMap — change notification on top of PropertyMap
//
// Demonstrates that type erasure composes: the observer callbacks are
// themselves type-erased via std::function.
// =============================================================================
class ObservablePropertyMap : public PropertyMap {
public:
using ChangeCallback = std::function<void(const std::string& key)>;

void on_change(ChangeCallback cb) {
   observers_.push_back(std::move(cb));
}
template<typename T>
void set(const std::string& key, T value) {
   PropertyMap::set(key, std::move(value));
   notify(key);
}
void remove(const std::string& key) {
   PropertyMap::remove(key);
   notify(key);
}

private:
void notify(const std::string& key) {
for (auto& cb : observers_) cb(key);
}

std::vector<ChangeCallback> observers_;

};
// =============================================================================
// PART 3: ComponentConfig — a SystemC-style component configuration object
//
// Uses PropertyMap to hold mixed-type configuration for a hardware component.
// The component itself is templated on nothing — it accepts any config value
// type through the map.
// =============================================================================
struct ComponentConfig {
PropertyMap params;

void apply_defaults() {
   if (!params.has("name"))           params.set("name",           std::string("unnamed"));
   if (!params.has("clock_period_ns")) params.set("clock_period_ns", 10);
   if (!params.has("enable_trace"))   params.set("enable_trace",   false);
   if (!params.has("bus_width"))      params.set("bus_width",      32);
   if (!params.has("timeout_us"))     params.set("timeout_us",     1000.0);
}
void print() const {
   std::cout << "  ComponentConfig:\n";
   params.for_each_key([&](const std::string& k) {
       std::cout << "    " << k << " [" << params.type_name_of(k) << "]\n";
   });
}

};
// Simulated hardware component that reads its config from a PropertyMap
class BusInitiator {
ComponentConfig cfg_;
public:
explicit BusInitiator(ComponentConfig cfg) : cfg_(std::move(cfg)) {
cfg_.apply_defaults();
}

void init() {
   auto name     = cfg_.params.get<std::string>("name");
   auto clk_ns   = cfg_.params.get<int>("clock_period_ns");
   auto trace    = cfg_.params.get<bool>("enable_trace");
   auto width    = cfg_.params.get<int>("bus_width");
   auto timeout  = cfg_.params.get<double>("timeout_us");
   std::cout << "  BusInitiator '" << name << "' initialised:\n"
<< "    clock=" << clk_ns << "ns  width=" << width
<< "bit  trace=" << (trace ? "ON" : "OFF")
<< "  timeout=" << timeout << "us\n";
}

};
// =============================================================================
// PART 4: SchemaMap — a property map with a registered type schema
//
// Catches type mismatches at set() time rather than at get() time.
// The schema maps key names to expected std::type_index values.
// =============================================================================
class SchemaMap {
public:
// Register a key with its expected type at setup time
template<typename T>
void register_key(const std::string& key) {
schema_.insert_or_assign(key, std::type_index(typeid(T)));
}

template<typename T>
void set(const std::string& key, T value) {
   auto it = schema_.find(key);
   if (it != schema_.end()) {
       if (it->second != std::type_index(typeid(T))) {
           throw std::invalid_argument(
               "SchemaMap: type mismatch for key '" + key +
               "': expected " + it->second.name() +
               ", got " + typeid(T).name()
           );
       }
   }
   props_[key] = std::any(std::move(value));
}
template<typename T>
T get(const std::string& key) const {
   auto it = props_.find(key);
   if (it == props_.end())
       throw std::out_of_range("SchemaMap: key not found: " + key);
   return std::any_cast<T>(it->second);
}
bool has(const std::string& key) const { return props_.count(key) > 0; }

private:
std::unordered_map<std::string, std::type_index> schema_;
std::unordered_map<std::string, std::any>        props_;
};
// =============================================================================
// PART 5: Test Bench
// =============================================================================
// ── Test 1: Basic set / get / type safety ────────────────────────────────────
void test_basic_operations() {
std::cout << "\n=== Test 1: Basic set / get / type safety ===\n";

PropertyMap map;
map.set("timeout_ns",      100);
map.set("name",            std::string("initiator_0"));
map.set("enable_coverage", true);
map.set("threshold",       3.14);
map.set("tag",             std::string("v2.1"));
assert(map.get<int>("timeout_ns")          == 100);
assert(map.get<std::string>("name")        == "initiator_0");
assert(map.get<bool>("enable_coverage")    == true);
assert(map.get<double>("threshold")        == 3.14);
assert(map.size()                          == 5);
std::cout << "  PASS: get<T> recovered all values correctly\n";
// has<T> — type-aware existence check
assert( map.has<int>("timeout_ns"));
assert(!map.has<double>("timeout_ns"));   // stored as int, not double
assert( map.has<std::string>("name"));
std::cout << "  PASS: has<T> correctly distinguishes types\n";
// Wrong type throws bad_any_cast
try {
   auto val = map.get<double>("timeout_ns");  // stored as int!
   (void)val;
   std::cout << "  FAIL: expected bad_any_cast\n";
} catch (const std::bad_any_cast&) {
   std::cout << "  PASS: bad_any_cast thrown on type mismatch\n";
}
// Missing key throws out_of_range
try {
   map.get<int>("nonexistent");
   std::cout << "  FAIL: expected out_of_range\n";
} catch (const std::out_of_range&) {
   std::cout << "  PASS: out_of_range thrown on missing key\n";
}

}
// ── Test 2: get_or and try_get ───────────────────────────────────────────────
void test_safe_accessors() {
std::cout << "\n=== Test 2: Safe accessors (get_or / try_get) ===\n";

PropertyMap map;
map.set("retries", 3);
// get_or: default on missing key
int r1 = map.get_or<int>("retries",   99);   // exists
int r2 = map.get_or<int>("missing",   99);   // missing
assert(r1 == 3);
assert(r2 == 99);
std::cout << "  PASS: get_or returned " << r1 << " (exists) and " << r2 << " (missing)\n";
// try_get: optional
auto o1 = map.try_get<int>("retries");
auto o2 = map.try_get<int>("missing");
assert( o1.has_value() && *o1 == 3);
assert(!o2.has_value());
std::cout << "  PASS: try_get returned optional correctly\n";
// type mismatch: get_or returns default, try_get returns nullopt
int r3   = map.get_or<double>("retries", 7.7);  // wrong type
auto o3  = map.try_get<std::string>("retries");  // wrong type
assert(r3 == 7);         // double 7.7 -> int default
assert(!o3.has_value());
std::cout << "  PASS: type-mismatch handled gracefully by safe accessors\n";

}
// ── Test 3: Heterogeneous value types including user-defined structs ─────────
void test_user_defined_types() {
std::cout << "\n=== Test 3: User-defined struct as value type ===\n";

struct Waveform {
   std::string signal_name;
   double      frequency_mhz;
   int         samples;
   bool operator==(const Waveform& o) const {
       return signal_name == o.signal_name &&
              frequency_mhz == o.frequency_mhz &&
              samples == o.samples;
   }
};
PropertyMap map;
map.set("clk",  Waveform{"CLK",  100.0, 1024});
map.set("data", Waveform{"DATA",  50.0,  512});
map.set("desc", std::string("AXI bus capture"));
auto clk = map.get<Waveform>("clk");
assert(clk == (Waveform{"CLK", 100.0, 1024}));
std::cout << "  PASS: Waveform struct stored and recovered via std::any\n";
// The map doesn't care what the type is — it stores and returns faithfully
auto data = map.get<Waveform>("data");
assert(data.frequency_mhz == 50.0);
std::cout << "  PASS: second Waveform (" << data.signal_name
<< " @ " << data.frequency_mhz << " MHz) recovered correctly\n";

}
// ── Test 4: ComponentConfig + BusInitiator ───────────────────────────────────
void test_component_config() {
std::cout << "\n=== Test 4: ComponentConfig (SystemC-style) ===\n";

ComponentConfig cfg;
cfg.params.set("name",            std::string("axi_initiator_0"));
cfg.params.set("clock_period_ns", 5);
cfg.params.set("enable_trace",    true);
cfg.params.set("bus_width",       64);
cfg.params.set("timeout_us",      500.0);
cfg.print();
BusInitiator initiator(cfg);
initiator.init();
std::cout << "  PASS: BusInitiator configured from heterogeneous PropertyMap\n";

}
// ── Test 5: ObservablePropertyMap ────────────────────────────────────────────
void test_observable_map() {
std::cout << "\n=== Test 5: Observable property map (change notification) ===\n";

ObservablePropertyMap map;
std::vector<std::string> changed_keys;
// Register a type-erased observer callback
map.on_change([&](const std::string& key) {
   changed_keys.push_back(key);
   std::cout << "  [observer] key changed: " << key << "\n";
});
map.set("x", 10);
map.set("y", 20);
map.set("label", std::string("origin"));
map.remove("label");
assert(changed_keys.size() == 4);
assert(changed_keys[0] == "x");
assert(changed_keys[1] == "y");
assert(changed_keys[2] == "label");
assert(changed_keys[3] == "label");  // remove also notifies
std::cout << "  PASS: " << changed_keys.size() << " change notifications received\n";

}
// ── Test 6: SchemaMap — type-checked at set() time ───────────────────────────
void test_schema_map() {
std::cout << "\n=== Test 6: SchemaMap — schema-enforced types ===\n";

SchemaMap map;
map.register_key<int>("port");
map.register_key<std::string>("host");
map.register_key<bool>("secure");
map.set("port",   8080);
map.set("host",   std::string("localhost"));
map.set("secure", true);
assert(map.get<int>("port")         == 8080);
assert(map.get<std::string>("host") == "localhost");
assert(map.get<bool>("secure")      == true);
std::cout << "  PASS: schema-correct values accepted\n";
// Type mismatch caught at set() — not at get()
try {
   map.set("port", std::string("not-an-int"));  // wrong type!
   std::cout << "  FAIL: expected invalid_argument\n";
} catch (const std::invalid_argument& e) {
   std::cout << "  PASS: schema violation caught at set(): " << e.what() << "\n";
}
// Unregistered keys are accepted with any type (open schema)
map.set("extra", 3.14);
assert(map.get<double>("extra") == 3.14);
std::cout << "  PASS: unregistered key accepted with any type\n";

}
// ── Test 7: Overwrite and remove ─────────────────────────────────────────────
void test_overwrite_and_remove() {
std::cout << "\n=== Test 7: Overwrite and remove ===\n";

PropertyMap map;
map.set("mode", std::string("fast"));
assert(map.get<std::string>("mode") == "fast");
// Overwrite with same type
map.set("mode", std::string("slow"));
assert(map.get<std::string>("mode") == "slow");
std::cout << "  PASS: overwrite same type\n";
// Overwrite with different type — std::any just replaces
map.set("mode", 42);
assert(map.get<int>("mode") == 42);
assert(!map.has<std::string>("mode"));  // old type is gone
std::cout << "  PASS: overwrite with different type — old type erased\n";
// Remove
map.remove("mode");
assert(!map.has("mode"));
std::cout << "  PASS: remove works correctly\n";

}
// =============================================================================
// PART 6: Examples — practical usage walkthrough
//
// Run by default (no flags).  Shows how each class is meant to be used in
// realistic scenarios, including graceful error handling.
// =============================================================================
void run_examples() {
    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 1: Basic heterogeneous config store\n";
    std::cout << "--------------------------------------------------\n";
    {
        // A PropertyMap can hold any mix of types under plain string keys.
        // The concrete type is erased into std::any at set() and recovered
        // type-safely at get<T>().
        PropertyMap cfg;
        cfg.set("bus_width",      64);
        cfg.set("clock_period_ns", 5);
        cfg.set("model_name",      std::string("axi_lite_v2"));
        cfg.set("enable_trace",    true);
        cfg.set("voltage",         1.8);

        std::cout << "  bus_width      = " << cfg.get<int>("bus_width")         << " bits\n";
        std::cout << "  clock_period   = " << cfg.get<int>("clock_period_ns")   << " ns\n";
        std::cout << "  model_name     = " << cfg.get<std::string>("model_name") << "\n";
        std::cout << "  enable_trace   = " << std::boolalpha
                                           << cfg.get<bool>("enable_trace")     << "\n";
        std::cout << "  voltage        = " << cfg.get<double>("voltage")        << " V\n";
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 2: Safe accessors - no exceptions needed\n";
    std::cout << "--------------------------------------------------\n";
    {
        PropertyMap cfg;
        cfg.set("retries", 3);

        // get_or: returns a default when key is absent or type mismatches
        int  r1 = cfg.get_or<int>("retries",  0);    // key exists  → 3
        int  r2 = cfg.get_or<int>("timeout",  500);  // key absent  → 500
        auto o1 = cfg.try_get<int>("retries");        // Some(3)
        auto o2 = cfg.try_get<int>("missing");        // None

        std::cout << "  get_or(retries, 0)   = " << r1 << "\n";
        std::cout << "  get_or(timeout, 500) = " << r2 << "  (key absent - default used)\n";
        std::cout << "  try_get(retries)     = " << (o1 ? std::to_string(*o1) : "nullopt") << "\n";
        std::cout << "  try_get(missing)     = " << (o2 ? std::to_string(*o2) : "nullopt") << "\n";

        // Requesting the wrong type also uses the default gracefully
        double wrong = cfg.get_or<double>("retries", -1.0);
        std::cout << "  get_or<double>(retries, -1) = " << wrong
                  << "  (stored as int -> default returned)\n";
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 3: Type mismatch - what happens\n";
    std::cout << "--------------------------------------------------\n";
    {
        PropertyMap cfg;
        cfg.set("port", 8080);  // stored as int

        // Correct retrieval
        std::cout << "  get<int>(\"port\")    = " << cfg.get<int>("port") << "\n";

        // Wrong type → bad_any_cast
        try {
            std::string s = cfg.get<std::string>("port");  // wrong!
            (void)s;
        } catch (const std::bad_any_cast&) {
            std::cout << "  get<string>(\"port\") -> caught std::bad_any_cast  [ok]\n";
        }

        // Missing key → out_of_range
        try {
            cfg.get<int>("host");
        } catch (const std::out_of_range& e) {
            std::cout << "  get<int>(\"host\")    -> caught std::out_of_range: "
                      << e.what() << "  [ok]\n";
        }
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 4: Storing a user-defined struct\n";
    std::cout << "--------------------------------------------------\n";
    {
        // std::any works with any copy-constructible type — not just primitives.
        struct ClockSpec {
            std::string name;
            double      freq_mhz;
            int         duty_cycle_pct;
        };

        PropertyMap cfg;
        cfg.set("sys_clk",  ClockSpec{"SYS_CLK",  200.0, 50});
        cfg.set("pcie_clk", ClockSpec{"PCIE_CLK", 100.0, 50});
        cfg.set("label",    std::string("platform_clocks"));

        auto sys = cfg.get<ClockSpec>("sys_clk");
        std::cout << "  sys_clk:  " << sys.name
                  << "  @ " << sys.freq_mhz << " MHz"
                  << "  duty=" << sys.duty_cycle_pct << "%\n";

        auto pcie = cfg.get<ClockSpec>("pcie_clk");
        std::cout << "  pcie_clk: " << pcie.name
                  << " @ " << pcie.freq_mhz << " MHz"
                  << "  duty=" << pcie.duty_cycle_pct << "%\n";

        std::cout << "  label:    " << cfg.get<std::string>("label") << "\n";
        std::cout << "  (ClockSpec, ClockSpec, and string all stored in the same map)\n";
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 5: Observable map - reacting to changes\n";
    std::cout << "--------------------------------------------------\n";
    {
        ObservablePropertyMap live;

        // Register an observer — its type is erased via std::function
        live.on_change([](const std::string& key) {
            std::cout << "  [observer] property changed: \"" << key << "\"\n";
        });

        std::cout << "  Setting properties...\n";
        live.set("voltage",    1.0);
        live.set("frequency",  500);
        live.set("state",      std::string("idle"));
        live.set("state",      std::string("running"));  // update triggers again
        live.remove("voltage");                           // remove also notifies
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 6: SchemaMap - enforce types at set() time\n";
    std::cout << "--------------------------------------------------\n";
    {
        // SchemaMap lets you register expected types up-front.
        // A type mismatch at set() throws immediately — not later at get().
        SchemaMap schema;
        schema.register_key<int>("port");
        schema.register_key<std::string>("host");
        schema.register_key<bool>("tls");

        schema.set("port", 443);
        schema.set("host", std::string("localhost"));
        schema.set("tls",  true);

        std::cout << "  port = " << schema.get<int>("port")         << "\n";
        std::cout << "  host = " << schema.get<std::string>("host") << "\n";
        std::cout << "  tls  = " << std::boolalpha
                                 << schema.get<bool>("tls")         << "\n";

        // Attempt to set the wrong type for a registered key
        try {
            schema.set("port", std::string("not-a-number"));
        } catch (const std::invalid_argument& e) {
            std::cout << "  set(\"port\", string) -> " << e.what() << "  [ok]\n";
        }
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " EXAMPLE 7: Overwrite and dynamic type change\n";
    std::cout << "--------------------------------------------------\n";
    {
        PropertyMap cfg;
        cfg.set("mode", std::string("fast"));
        std::cout << "  mode (string) = " << cfg.get<std::string>("mode") << "\n";

        // Overwrite with a different type — the old type is fully replaced
        cfg.set("mode", 42);
        std::cout << "  mode (int)    = " << cfg.get<int>("mode")
                  << "  (string is gone - std::any replaced)\n";
        std::cout << "  has<string>(mode) = " << std::boolalpha
                                              << cfg.has<std::string>("mode") << "\n";

        cfg.remove("mode");
        std::cout << "  After remove: has(mode) = " << cfg.has("mode") << "\n";
    }

    std::cout << "\n";
}

// =============================================================================
// PART 7: main
// =============================================================================
static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [-test]\n\n"
              << "  (no flags)  Run usage examples that show the API in action\n"
              << "  -test       Run the full test suite with assertions\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=================================================\n";
    std::cout << " Type Erasure - Heterogeneous Property Map\n";
    std::cout << " codepuz.com\n";
    std::cout << "=================================================\n";

    // ── parse flags ───────────────────────────────────────────────────────
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
        // ── full test suite ───────────────────────────────────────────────
        std::cout << "\nRunning test suite...\n";
        test_basic_operations();
        test_safe_accessors();
        test_user_defined_types();
        test_component_config();
        test_observable_map();
        test_schema_map();
        test_overwrite_and_remove();
        std::cout << "\nAll tests passed.\n";
    } else {
        // ── examples (default) ────────────────────────────────────────────
        run_examples();
    }

    return 0;
}