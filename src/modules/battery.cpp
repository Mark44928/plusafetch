#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) {
#if defined(__ANDROID__)
    detectBatteryAndroid(ctx);
#else
    detectBatteryLinux(ctx);
#endif
}
static const char* const kFmt[] = {"battery", nullptr};
extern const FFModuleBaseInfo kBatteryModuleInfo{ "battery", "Battery", "battery level and status",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.batteryLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
