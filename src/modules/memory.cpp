#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) { detectMemory(ctx); }
static const char* const kFmt[] = {"memory", nullptr};
extern const FFModuleBaseInfo kMemoryModuleInfo{ "memory", "Memory", "memory usage (used / total)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.memLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
