// Core/system modules: kernel, host, uptime, packages, shell, terminal,
// locale, datetime, disk, title, separator, colors, command, clipboard, brightness.
#include "../core/module_registry.hpp"
#include "../core/default_impls.hpp"
#include "../detection/platform.hpp"
#include <cstring>
#include <cctype>
#include <cstdio>

namespace plf {

// ---- Kernel ----
static void detectKernelModule(FFContext& ctx) { detectKernel(ctx); }
static const char* const kKernelFormat[] = {"kernel", nullptr};
extern const FFModuleBaseInfo kKernelModuleInfo{ "kernel", "Kernel", "kernel name and release",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectKernelModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.kernelLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kKernelFormat };

// ---- Disk ----
static void detectDiskModule(FFContext& ctx) { detectDisk(ctx); }
static const char* const kDiskFormat[] = {"disk", nullptr};
extern const FFModuleBaseInfo kDiskModuleInfo{ "disk", "Disk", "root filesystem usage (used / total)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectDiskModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.diskLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kDiskFormat };

// ---- Host ----
static void detectHostModule(FFContext& ctx) {
#if defined(__ANDROID__)
    detectHostAndroid(ctx);
#else
    detectHostLinux(ctx);
#endif
}
static const char* const kHostFormat[] = {"host", nullptr};
extern const FFModuleBaseInfo kHostModuleInfo{ "host", "Host", "hostname / device model",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectHostModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.hostLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kHostFormat };

// ---- Uptime ----
static void detectUptimeModule(FFContext& ctx) { detectUptime(ctx); }
static const char* const kUptimeFormat[] = {"uptime", nullptr};
extern const FFModuleBaseInfo kUptimeModuleInfo{ "uptime", "Uptime", "system uptime",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectUptimeModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.uptimeLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kUptimeFormat };

// ---- Packages ----
static void detectPackagesModule(FFContext& ctx) { detectPackages(ctx); }
static const char* const kPackagesFormat[] = {"packages", nullptr};
extern const FFModuleBaseInfo kPackagesModuleInfo{ "packages", "Packages", "number of installed packages (dpkg / rpm)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectPackagesModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.pkgLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kPackagesFormat };

// ---- Shell ----
static void detectShellModule(FFContext& ctx) { detectShell(ctx); }
static const char* const kShellFormat[] = {"shell", nullptr};
extern const FFModuleBaseInfo kShellModuleInfo{ "shell", "Shell", "the user's shell",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectShellModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.shellLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kShellFormat };

// ---- Terminal ----
static void detectTerminalModule(FFContext& ctx) { detectTerminal(ctx); }
static const char* const kTerminalFormat[] = {"terminal", nullptr};
extern const FFModuleBaseInfo kTerminalModuleInfo{ "terminal", "Terminal", "TERM program capability",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectTerminalModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.terminalLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kTerminalFormat };

// ---- Locale ----
static void detectLocaleModule(FFContext& ctx) { detectLocale(ctx); }
static const char* const kLocaleFormat[] = {"locale", nullptr};
extern const FFModuleBaseInfo kLocaleModuleInfo{ "locale", "Locale", "the active locale",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectLocaleModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.localeLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kLocaleFormat };

// ---- DateTime ----
static void detectDateTimeModule(FFContext& ctx) { detectDateTime(ctx); }
static const char* const kDateTimeFormat[] = {"datetime", nullptr};
extern const FFModuleBaseInfo kDateTimeModuleInfo{ "datetime", "DateTime", "current date and time",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectDateTimeModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.dateTimeLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kDateTimeFormat };

// ---- Title (user@host) ----
static void detectTitleModule(FFContext& ctx) { (void)ctx; }
static const char* const kTitleFormat[] = {"title", nullptr};
static std::string titleText(const FFContext& ctx) {
    std::string host = ctx.hostName;
    if (host.empty()) {
        char buf[256]{};
        if (gethostname(buf, sizeof buf) == 0 && buf[0] != '\0') host = buf;
        if (host.empty() || host == "localhost") {
#if defined(__ANDROID__)
            std::string m = androidProp("ro.product.model");
            if (!m.empty()) host = std::move(m);
#endif
        }
        if (host.empty()) host = "localhost";
    }
    return ctx.userName + "@" + host;
}
static void printTitleModule(void*) noexcept {
    FFContext& ctx = *g_ctx;
    std::string t = titleText(ctx);
    if (ctx.color) ffStrbufAppendS(ctx.frame.b, ctx.truecolor ? "\033[1;38;2;226;232;255m" : "\033[1;97m");
    ffStrbufAppendS(ctx.frame.b, t.c_str());
    if (ctx.color) ffStrbufAppendS(ctx.frame.b, "\033[0m");
    ffStrbufAppendC(ctx.frame.b, '\n');
}
extern const FFModuleBaseInfo kTitleModuleInfo{ "title", "Title", "user@host header (bold when color is on)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectTitleModule,
    [](const FFContext& ctx) -> Result<std::string> { return titleText(ctx); },
    &printTitleModule, &jsonResultDefault, &jsonConfigDefault, kTitleFormat };

// ---- Separator ----
static void detectSeparatorModule(FFContext&) {}
static const char* const kSeparatorFormat[] = {"separator", nullptr};
static void printSeparatorModule(void*) noexcept {
    FFContext& ctx = *g_ctx;
    int n = ctx.separatorLength;
    if (n < 0) n = 0;
    if (n > 200) n = 200;
    const char* dim = ctx.color ? (ctx.truecolor ? "\033[2;38;2;110;118;128m" : "\033[2;37m") : "";
    const char* reset = ctx.color ? "\033[0m" : "";
    ffStrbufAppendS(ctx.frame.b, dim);
    while (n-- > 0) ffStrbufAppendS(ctx.frame.b, "─");
    ffStrbufAppendS(ctx.frame.b, reset);
    ffStrbufAppendC(ctx.frame.b, '\n');
}
extern const FFModuleBaseInfo kSeparatorModuleInfo{ "separator", "Separator", "a dim ASCII separator line",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectSeparatorModule,
    [](const FFContext&) -> Result<std::string> { return std::unexpected(Error{""}); },
    &printSeparatorModule, &jsonResultDefault, &jsonConfigDefault, kSeparatorFormat };

// ---- Colors ----
static void detectColorsModule(FFContext&) {}
static const char* const kColorsFormat[] = {"colors", nullptr};
static void printColorsModule(void*) noexcept {
    FFContext& ctx = *g_ctx;
    static constexpr int rgb[8][3] = {
        {  86, 194, 230}, { 110, 210, 110}, { 230, 200,  60}, { 230,  90,  80},
        { 220, 180, 230}, { 120, 180, 240}, { 230, 150,  60}, { 200, 200, 200},
    };
    for (int c = 0; c < 8; ++c) {
        char esc[48];
        if (ctx.color) {
            if (ctx.truecolor)
                std::snprintf(esc, sizeof esc, "\033[38;2;%d;%d;%dm■", rgb[c][0], rgb[c][1], rgb[c][2]);
            else {
                // 90 is bright black; bare 38 would not be a color code
                static constexpr int basic[8] = { 31, 32, 33, 34, 35, 36, 37, 90 };
                std::snprintf(esc, sizeof esc, "\033[%dm■\033[0m", basic[c]);
            }
        } else {
            esc[0] = '#'; esc[1] = ' '; esc[2] = '\0';
        }
        ffStrbufAppendS(ctx.frame.b, esc);
        ffStrbufAppendC(ctx.frame.b, ' ');
    }
    if (ctx.color) ffStrbufAppendS(ctx.frame.b, "\033[0m");
    ffStrbufAppendC(ctx.frame.b, '\n');
}
extern const FFModuleBaseInfo kColorsModuleInfo{ "colors", "Colors", "a row of truecolor color blocks",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectColorsModule,
    [](const FFContext&) -> Result<std::string> { return std::unexpected(Error{""}); },
    &printColorsModule, &jsonResultDefault, &jsonConfigDefault, kColorsFormat };

// ---- Command (config-supplied shell command) ----
static void detectCommandModule(FFContext& ctx) {
    if (ctx.configCommand.empty()) { ctx.commandLine = std::unexpected(Error{"no 'command' in config"}); return; }
    const char* sh = std::getenv("SHELL");
    if (!sh || !*sh)
        sh =
#if defined(__ANDROID__)
            "/system/bin/sh";
#else
            "/bin/sh";
#endif
    const char* const argv[] = { sh, "-c", ctx.configCommand.c_str(), nullptr }; // single -c string by design
    auto r = execCapture(argv, 1u << 16);
    if (!r) { ctx.commandLine = std::unexpected(Error{r.error()}); return; }
    std::string s{*r};
    auto nl = s.find('\n');
    if (nl != std::string::npos) s = s.substr(0, nl);
    auto t = trimVal(s);
    ctx.commandLine = t.and_then([](std::string v) -> Result<std::string> { if (v.empty()) return std::unexpected(Error{"command printed nothing"}); return std::move(v); });
}
static const char* const kCommandFormat[] = {"command", nullptr};
static void printCommandModule(void*) noexcept {
    FFContext& ctx = *g_ctx;
    std::string key = ctx.configCommandKey.empty() ? "Command" : ctx.configCommandKey;
    auto rename = ctx.keyRenames.find("command"); // uniform rename rule: module object key wins
    if (rename != ctx.keyRenames.end() && !rename->second.empty()) key = rename->second;
    auto v = ctx.commandLine;
    std::string value = v ? std::move(*v) : "N/A";
    printKeyLine(ctx, key, value);
}
extern const FFModuleBaseInfo kCommandModuleInfo{ "command", "Command", "run a shell command from config.jsonc and print the first line",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectCommandModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.commandLine; },
    &printCommandModule, &jsonResultDefault, &jsonConfigDefault, kCommandFormat };

// ---- Clipboard ----
static void detectClipboardModule(FFContext& ctx) {
#if defined(__ANDROID__)
    const char* const argv[] = { "termux-clipboard-get", nullptr };
#else
    const char* const argv[] = { "xclip", "-o", "-selection", "clipboard", nullptr };
#endif
    auto r = execCapture(argv, 4096);
    if (!r) { ctx.clipboardLine = std::unexpected(Error{"clipboard unavailable"}); return; }
    std::string s{*r};
    if (s.size() > 50) s = s.substr(0, 50) + "…";
    ctx.clipboardLine = trimVal(s).and_then(requireNonEmpty);
}
static const char* const kClipboardFormat[] = {"clipboard", nullptr};
extern const FFModuleBaseInfo kClipboardModuleInfo{ "clipboard", "Clipboard", "contents of the system clipboard (truncated)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectClipboardModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.clipboardLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kClipboardFormat };

// ---- Brightness ----
// Android: termux-api is write-only unrooted => always N/A.
// Linux: read the first readable backlight device (often permission-denied).
static void detectBrightnessModule(FFContext& ctx) {
#if defined(__ANDROID__)
    detectBrightnessAndroid(ctx);
#else
    // Try acpi_video0, then the first common discrete-GPU backlight; use
    // whichever pair (max, cur) is readable and compute the percentage once.
    const char* devs[] = { "/sys/class/backlight/acpi_video0", "/sys/class/backlight/amdgpu_bl0", nullptr };
    for (const char** d = devs; *d; ++d) {
        auto mx = readFirstLine(std::string(*d) + "/max_brightness");
        auto cu = readFirstLine(std::string(*d) + "/brightness");
        if (!mx || !cu) continue;
        uint64_t maxv = std::strtoull(mx->c_str(), nullptr, 10);
        uint64_t curv = std::strtoull(cu->c_str(), nullptr, 10);
        if (!maxv || curv > maxv) continue;
        char out[32];
        std::snprintf(out, sizeof out, "%llu%%", (unsigned long long)(curv * 100 / maxv));
        ctx.brightnessLine = std::string{out};
        break;
    }
    if (!ctx.brightnessLine) ctx.brightnessLine = std::unexpected(Error{"no readable backlight"});
#endif
}
static const char* const kBrightnessFormat[] = {"brightness", nullptr};
extern const FFModuleBaseInfo kBrightnessModuleInfo{ "brightness", "Brightness", "screen brightness (N/A on most devices — termux-api is write-only)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectBrightnessModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.brightnessLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kBrightnessFormat };

// ---- GPU (Linux only, lspci) ----
static void detectGpuModule(FFContext& ctx) {
#if defined(__ANDROID__)
    ctx.gpuLine = std::unexpected(Error{"no lspci on Android"});
#else
    detectGpuLinux(ctx);
#endif
}
static const char* const kGpuFormat[] = {"gpu", nullptr};
extern const FFModuleBaseInfo kGpuModuleInfo{ "gpu", "GPU", "the graphics controller (lspci)",
    &initOptionsDefault, &destroyOptionsDefault, nullptr,
    &detectGpuModule,
    [](const FFContext& ctx) -> Result<std::string> { return ctx.gpuLine; },
    &printModuleDefault, &jsonResultDefault, &jsonConfigDefault, kGpuFormat };

} // namespace plf
