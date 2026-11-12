// Registry: A–Z module table with NULL-terminated arrays (fastfetch-style,
// lookup toupper(name[0]) - 'A'), the default renderers, the stateless
// option lifecycle shared by every module, and the --format template engine.
#include "core/module_registry.hpp"
#include "detection/platform.hpp"
#include <cctype>
#include <cstring>
#include <algorithm>

namespace plf {

// ---- Shared stateless option lifecycle ------------------------------------
void initOptionsDefault(void*) noexcept {}
void destroyOptionsDefault(void*) noexcept {}

// In this registry, `options` IS the module's own FFModuleBaseInfo* (set by main).
void printModuleDefault(void* options) noexcept {
    const auto* self = static_cast<const FFModuleBaseInfo*>(options);
    FFContext& ctx = *g_ctx;
    if (!self || !self->moduleValue) {
        ffStrbufAppendS(ctx.frame.b, "Unknown: ?\n");
        return;
    }
    auto v = self->moduleValue(ctx);
    std::string value = v ? std::move(*v) : "N/A";
    std::string key{self->displayName ? self->displayName : self->name};
    if (!self->displayName && !key.empty()) key[0] = (char)std::toupper((unsigned char)key[0]);
    const char* keyCol = "";
    const char* reset  = "";
    if (ctx.color) {
        keyCol = ctx.truecolor ? "\033[38;2;120;180;255m" : "\033[36m";
        reset  = "\033[0m";
    }
    ffStrbufAppendS(ctx.frame.b, keyCol);
    ffStrbufAppendS(ctx.frame.b, key.c_str());
    ffStrbufAppendS(ctx.frame.b, reset);
    ffStrbufAppendS(ctx.frame.b, ": ");
    ffStrbufAppendS(ctx.frame.b, value.c_str());
    ffStrbufAppendC(ctx.frame.b, '\n');
}

bool jsonResultDefault(void* options, nlohmann::json& out) noexcept {
    const auto* self = static_cast<const FFModuleBaseInfo*>(options);
    if (!self || !self->moduleValue) return false;
    auto v = self->moduleValue(*g_ctx);
    if (!v) return false; // Unsupported: main keeps `out` as null
    out = std::move(*v);
    return true;
}

void jsonConfigDefault(void*, nlohmann::json& out) noexcept {
    out = nlohmann::json::object();
}

} // namespace plf

namespace plf {

extern const FFModuleBaseInfo kOsModuleInfo;
extern const FFModuleBaseInfo kKernelModuleInfo;
extern const FFModuleBaseInfo kCpuModuleInfo;
extern const FFModuleBaseInfo kMemoryModuleInfo;
extern const FFModuleBaseInfo kDiskModuleInfo;
extern const FFModuleBaseInfo kUptimeModuleInfo;
extern const FFModuleBaseInfo kPackagesModuleInfo;
extern const FFModuleBaseInfo kShellModuleInfo;
extern const FFModuleBaseInfo kTerminalModuleInfo;
extern const FFModuleBaseInfo kLocaleModuleInfo;
extern const FFModuleBaseInfo kDateTimeModuleInfo;
extern const FFModuleBaseInfo kHostModuleInfo;
extern const FFModuleBaseInfo kBatteryModuleInfo;
extern const FFModuleBaseInfo kBrightnessModuleInfo;
extern const FFModuleBaseInfo kSensorsModuleInfo;
extern const FFModuleBaseInfo kTelephonyModuleInfo;
extern const FFModuleBaseInfo kWifiModuleInfo;
extern const FFModuleBaseInfo kCommandModuleInfo;
extern const FFModuleBaseInfo kClipboardModuleInfo;
extern const FFModuleBaseInfo kTitleModuleInfo;
extern const FFModuleBaseInfo kSeparatorModuleInfo;
extern const FFModuleBaseInfo kColorsModuleInfo;
extern const FFModuleBaseInfo kGpuModuleInfo;
static const FFModuleBaseInfo* const A[] = { nullptr };
static const FFModuleBaseInfo* const B[] = { &kBatteryModuleInfo, &kBrightnessModuleInfo, nullptr };
static const FFModuleBaseInfo* const C[] = { &kClipboardModuleInfo, &kColorsModuleInfo, &kCommandModuleInfo, &kCpuModuleInfo, nullptr };
static const FFModuleBaseInfo* const D[] = { &kDateTimeModuleInfo, &kDiskModuleInfo, nullptr };
static const FFModuleBaseInfo* const E[] = { nullptr };
static const FFModuleBaseInfo* const F[] = { nullptr };
static const FFModuleBaseInfo* const G[] = { &kGpuModuleInfo, nullptr };
static const FFModuleBaseInfo* const H[] = { &kHostModuleInfo, nullptr };
static const FFModuleBaseInfo* const I[] = { nullptr };
static const FFModuleBaseInfo* const J[] = { nullptr };
static const FFModuleBaseInfo* const K[] = { &kKernelModuleInfo, nullptr };
static const FFModuleBaseInfo* const L[] = { &kLocaleModuleInfo, nullptr };
static const FFModuleBaseInfo* const M[] = { &kMemoryModuleInfo, nullptr };
static const FFModuleBaseInfo* const N[] = { nullptr };
static const FFModuleBaseInfo* const O[] = { &kOsModuleInfo, nullptr };
static const FFModuleBaseInfo* const P[] = { &kPackagesModuleInfo, nullptr };
static const FFModuleBaseInfo* const Q[] = { nullptr };
static const FFModuleBaseInfo* const R[] = { nullptr };
static const FFModuleBaseInfo* const S[] = { &kSensorsModuleInfo, &kSeparatorModuleInfo, &kShellModuleInfo, nullptr };
static const FFModuleBaseInfo* const T[] = { &kTelephonyModuleInfo, &kTerminalModuleInfo, &kTitleModuleInfo, nullptr };
static const FFModuleBaseInfo* const U[] = { &kUptimeModuleInfo, nullptr };
static const FFModuleBaseInfo* const V[] = { nullptr };
static const FFModuleBaseInfo* const W[] = { &kWifiModuleInfo, nullptr };
static const FFModuleBaseInfo* const X[] = { nullptr };
static const FFModuleBaseInfo* const Y[] = { nullptr };
static const FFModuleBaseInfo* const Z[] = { nullptr };

const FFModuleBaseInfo* const* ffModuleInfos[26] = {
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
};

const std::vector<const FFModuleBaseInfo*>& allModules() {
    static const std::vector<const FFModuleBaseInfo*> cached = [] {
        std::vector<const FFModuleBaseInfo*> out;
        for (int i = 0; i < 26; ++i)
            for (const FFModuleBaseInfo* const* it = ffModuleInfos[i]; *it != nullptr; ++it)
                out.push_back(*it);
        return out;
    }();
    return cached;
}

const FFModuleBaseInfo* findModule(std::string_view name) {
    if (name.empty()) return nullptr;
    unsigned char c = (unsigned char)std::toupper((unsigned char)name.front());
    if (c < 'A' || c > 'Z') return nullptr;
    for (const FFModuleBaseInfo* const* it = ffModuleInfos[c - 'A']; *it != nullptr; ++it) {
        const char* n = (*it)->name;
        if (std::strlen(n) == name.size()) {
            bool eq = true;
            for (std::size_t i = 0; i < name.size(); ++i) {
                if ((char)std::tolower((unsigned char)n[i]) != (char)std::tolower((unsigned char)name[i])) { eq = false; break; }
            }
            if (eq) return *it;
        }
    }
    return nullptr;
}

// Re-exports used by the modules themselves.
void renderModuleLine(FFContext& ctx, const FFModuleBaseInfo& info) {
    (void)ctx;
    printModuleDefault(const_cast<FFModuleBaseInfo*>(&info));
}

// --format renderer: {module} and {module~start,len} placeholders.
// Unknown or failed placeholders vanish. `{cpu}` (exact name) resolves via findModule.
std::string renderFormatTemplate(FFContext& ctx, std::string_view tmpl) {
    (void)ctx;
    std::string out;
    out.reserve(tmpl.size() + 64);
    std::size_t i = 0;
    while (i < tmpl.size()) {
        const char c = tmpl[i];
        if (c != '{') { out.push_back(c); ++i; continue; }
        const std::size_t end = tmpl.find('}', i);
        if (end == std::string_view::npos) { out.push_back(c); ++i; continue; }
        const std::string_view spec{tmpl.data() + i + 1, end - (i + 1)};
        const std::size_t tilde = spec.find('~');
        std::string_view nm = spec;
        int begin = 0, len = -1;
        if (tilde != std::string_view::npos) {
            nm = spec.substr(0, tilde);
            std::string_view rest = spec.substr(tilde + 1);
            const std::size_t comma = rest.find(',');
            begin = std::atoi(std::string{rest.substr(0, comma)}.c_str());
            len = (comma == std::string_view::npos) ? -1 : std::atoi(std::string{rest.substr(comma + 1)}.c_str());
        }
        if (!nm.empty()) {
            if (const FFModuleBaseInfo* m = findModule(nm)) {
                if (m->moduleValue) {
                    if (auto v = m->moduleValue(*g_ctx)) {
                        std::string_view val{*v};
                        if (begin > 0) val = val.substr(std::min(static_cast<std::size_t>(begin), val.size()));
                        if (len >= 0 && static_cast<std::size_t>(len) < val.size()) val = val.substr(0, static_cast<std::size_t>(len));
                        out.append(val);
                    }
                }
            }
        }
        i = end + 1;
    }
    return out;
}

} // namespace plf
