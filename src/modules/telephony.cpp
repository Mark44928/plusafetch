#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) {
#if defined(__ANDROID__)
    detectTelephonyAndroid(ctx);
#else
    ctx.telephonyLine = std::unexpected(Error{"termux-api only (Android)"});
#endif
}
static const char* const kFmt[] = {"telephony", nullptr};
extern const FFModuleBaseInfo kTelephonyModuleInfo{ "telephony", "Telephony", "telephony carrier and network info",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.telephonyLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
