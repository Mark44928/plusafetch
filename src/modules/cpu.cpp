#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) { detectCpu(ctx); }
static const char* const kFmt[] = {"cpu", nullptr};
extern const FFModuleBaseInfo kCpuModuleInfo{ "cpu", "CPU", "the processor (model and core count)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.cpuLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
