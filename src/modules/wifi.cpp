#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) {
#if defined(__ANDROID__)
    detectWifiAndroid(ctx);
#else
    ctx.wifiLine = std::unexpected(Error{"termux-api only (Android)"});
#endif
}
static const char* const kFmt[] = {"wifi", nullptr};
extern const FFModuleBaseInfo kWifiModuleInfo{ "wifi", "WiFi", "wifi connection info",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.wifiLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
