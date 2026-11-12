#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"

namespace plf {
static void detect(FFContext& ctx) {
#if defined(__ANDROID__)
    detectOsAndroid(ctx);
#else
    detectOsLinux(ctx);
#endif
}
static const char* const kFmt[] = {"os", nullptr};
extern const FFModuleBaseInfo kOsModuleInfo{ "os", "OS", "the operating system",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detect,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.osLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kFmt };
} // namespace plf
