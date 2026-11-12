#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) {
#if defined(__ANDROID__)
    detectSensorsAndroid(ctx);
#else
    ctx.sensorLine = std::unexpected(Error{"termux-api only (Android)"});
#endif
}
static const char* const kFmt[] = {"sensors", nullptr};
extern const FFModuleBaseInfo kSensorsModuleInfo{ "sensors", "Sensors", "sensors (accelerometer, light, proximity, gyroscope)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.sensorLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
