#pragma once
// Module registry — FFModuleBaseInfo and the A–Z table.
// Modules are registered alphabetically in src/registry.cpp;
// lookup uses toupper(name[0]) - 'A'.
#include <string>
#include <string_view>
#include <vector>
#include "../detection/context.hpp"
#include "../../third_party/nlohmann/json.hpp"

namespace plf {

// Every FFModuleBaseInfo* must be valid while the program lives (static storage).
struct FFModuleBaseInfo {
    const char* name        = nullptr;   // registry key (lowercase, e.g. "os")
    const char* displayName = nullptr;   // rendered key, e.g. "OS" (defaults to name if null)
    const char* description = nullptr;   // human-facing help text

    // Options lifecycle (stateless modules use the shared no-ops from registry.cpp)
    void (*initOptions)(void* options)       = nullptr;
    void (*destroyOptions)(void* options)    = nullptr;
    void (*parseJsonObject)(void* options, const nlohmann::json& object) = nullptr;

    // Detection is done BEFORE print, in parallel. Writes only this module's
    // disjoint FFContext fields. Never throws.
    void (*detect)(FFContext& ctx)           = nullptr;

    // Renders/returns the module's value from FFContext.
    Result<std::string> (*moduleValue)(const FFContext& ctx) = nullptr;

    // Emits the rendered line into ctx.frame (default: "Key: value\n" with color).
    void (*printModule)(void* options)       = nullptr;

    // JSON render. Implementations replace ctx.jsonResults[name] with the value;
    // leaving it as nullptr marks Unsupported in --json output.
    bool (*generateJsonResult)(void* options, nlohmann::json& out) = nullptr;

    void (*generateJsonConfig)(void* options, nlohmann::json& out) = nullptr;

    // NULL-terminated list of {placeholder} names this module supports
    // for --format rendering (e.g. {"os"}).
    const char* const* formatArgs = nullptr;
};

// A–Z registry of NULL-terminated module arrays (defined in registry.cpp).
extern const FFModuleBaseInfo* const* ffModuleInfos[26];

// All registered modules, in registry order.
const std::vector<const FFModuleBaseInfo*>& allModules();

// Case-insensitive exact match on name; nullptr when absent.
const FFModuleBaseInfo* findModule(std::string_view name);

// Emit one rendered line ("Key: value") for a module into ctx.frame.
void renderModuleLine(FFContext& ctx, const FFModuleBaseInfo& info);

// Render the --format template; placeholders {name} and {~start,len} supported.
std::string renderFormatTemplate(FFContext& ctx, std::string_view tmpl);

} // namespace plf
