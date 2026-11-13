// Plusafetch — modular C++23 system fetch for Termux/Android and Linux.
// Apache-2.0-inspired behavior list:
//   * parallel module detection via std::jthread (independent modules),
//   * single-write frame output to STDOUT_FILENO,
//   * zero network calls, zero logo, text-only data display,
//   * every module degrades gracefully to N/A.
#include "core/module_registry.hpp"
#include "core/ffstrbuf.hpp"
#include "detection/platform.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cctype>
#include <cerrno>

namespace {

using namespace plf;

constexpr const char* kVersion = "0.1.0";
constexpr const char* kName    = "plusafetch";

// Default layout: fast — only syscalls/file reads, no subprocesses.
const char* const kDefaultStructure[] = {
    "title", "os", "kernel", "host", "cpu", "memory", "disk",
    "uptime", "packages", "shell", "terminal", "datetime", "separator", "colors",
};

// --all adds the termux-api-backed & config modules (slow subprocesses).
const char* const kAllStructure[] = {
    "title", "os", "kernel", "host", "gpu", "cpu", "memory", "disk",
    "uptime", "packages", "shell", "terminal", "locale", "datetime",
    "battery", "brightness", "sensors", "telephony", "wifi", "clipboard",
    "separator", "colors",
};

extern "C" void onSignal(int sig) { _exit(128 + sig); }

// write(2) loop: partial writes and EINTR are retried, so a slow pipe or an
// interrupted syscall never silently truncates the frame.
bool writeAll(int fd, const char* data, std::size_t size) {
    std::size_t off = 0;
    while (off < size) {
        ssize_t w = ::write(fd, data + off, size - off);
        if (w < 0) { if (errno == EINTR) continue; return false; }
        if (w == 0) return false;
        off += static_cast<std::size_t>(w);
    }
    return true;
}

// Joinable fan-out; detection errors are captured inside each module's
// FFContext result (std::unexpected) and never thrown.
void runDetectParallel(FFContext& ctx, const std::vector<const FFModuleBaseInfo*>& mods) {
    std::vector<std::jthread> threads;
    threads.reserve(mods.size());
    for (const auto* m : mods) {
        threads.emplace_back([m](FFContext& c) { m->detect ? m->detect(c) : void(); }, std::ref(ctx));
    }
    // jthread auto-joins at scope exit; force join before rendering anyway.
    for (auto& t : threads) if (t.joinable()) t.join();
}

std::string configPath() {
    if (const char* p = std::getenv("PLUSAFETCH_CONFIG")) return p;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) return std::string(xdg) + "/plusafetch/config.jsonc";
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/plusafetch/config.jsonc";
}

nlohmann::json loadConfig() {
    const std::string path = configPath();
    auto raw = readTextFile(path, 1u << 20);
    if (!raw) return nlohmann::json::object(); // missing config is fine
    std::string stripped = stripTrailingCommas(stripJsonComments(*raw));
    auto j = nlohmann::json::parse(stripped, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        std::fprintf(stderr, "%s: warning: could not parse %s (using defaults)\n", kName, path.c_str());
        return nlohmann::json::object();
    }
    return j.is_object() ? j : nlohmann::json::object();
}

bool writeConfigAtomic(const std::string& dest, const std::string& payload) {
    const char* tmpDirEnv = std::getenv("TMPDIR");
    const char* homeEnv   = std::getenv("HOME");
    const std::string tmpDir = (tmpDirEnv && *tmpDirEnv) ? tmpDirEnv : (homeEnv && *homeEnv ? std::string{homeEnv} : std::string{"."});
    char tmpl[4096];
    std::snprintf(tmpl, sizeof tmpl, "%s/plusafetch_config_XXXXXX", tmpDir.c_str());
    int fd = ::mkstemp(tmpl); // O_RDWR|O_CREAT|O_EXCL, mode 0600 — no predictable tmp file
    if (fd < 0) return false;
    for (std::size_t off = 0; off < payload.size(); ) {
        ssize_t w = ::write(fd, payload.data() + off, payload.size() - off);
        if (w <= 0) { ::close(fd); ::unlink(tmpl); return false; }
        off += static_cast<std::size_t>(w);
    }
    if (::close(fd) != 0) { ::unlink(tmpl); return false; }
    // Ensure the parent directory exists (mkdir -p style).
    const auto slash = dest.rfind('/');
    const std::string dir = (slash == std::string::npos) ? std::string{"."} : (slash == 0 ? std::string{"/"} : dest.substr(0, slash));
    std::string cur;
    for (char c : dir) { cur += c; if (c == '/' && cur.size() > 1) ::mkdir(cur.c_str(), 0755); }
    ::mkdir(dir.c_str(), 0755); // final segment; EEXIST is fine
    if (::rename(tmpl, dest.c_str()) == 0) return true;
    const int e = errno;
    if (e == EXDEV) {
        // TMPDIR on another filesystem: fall back to a plain copy (sacrifices atomicity).
        int in = ::open(tmpl, O_RDONLY | O_CLOEXEC);
        int out = in >= 0 ? ::open(dest.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600) : -1;
        bool ok = false;
        if (out >= 0) {
            char blk[4096];
            ok = true;
            for (;;) {
                ssize_t n = ::read(in, blk, sizeof blk);
                if (n < 0) { if (errno == EINTR) continue; ok = false; break; }
                if (n == 0) break;
                std::string_view rest{blk, static_cast<std::size_t>(n)};
                while (!rest.empty()) {
                    ssize_t w = ::write(out, rest.data(), rest.size());
                    if (w <= 0) { ok = false; break; }
                    rest.remove_prefix(static_cast<std::size_t>(w));
                }
                if (!ok) break;
            }
            ::close(out);
        }
        if (in >= 0) ::close(in);
        ::unlink(tmpl);
        return ok;
    }
    ::unlink(tmpl);
    return false;
}

void printUsage(FILE* out) {
    std::fprintf(out,
        "plusafetch %s — modular C++23 system fetch for Termux/Android and Linux\n"
        "\n"
        "USAGE:\n"
        "  plusafetch              Show the default layout\n"
        "  plusafetch --all        Include termux-api extras (battery, sensors,\n"
        "                            telephony, wifi, clipboard, command, gpu)\n"
        "  plusafetch --json       JSON dump of detected modules\n"
        "  plusafetch --format FMT Placeholders like {os}, {cpu}, {memory};\n"
        "                            slices: {os~0,5}\n"
        "  plusafetch --modules a,b Comma-separated module list\n"
        "                            (default: config or built-in)\n"
        "  plusafetch --list-modules   List every registered module\n"
        "  plusafetch --values-only    Print values only (no key column)\n"
        "  plusafetch --verify-config  Validate $HOME/.config/plusafetch/\n"
        "                            config.jsonc (exit 0 if valid)\n"
        "  plusafetch --gen-config     Write default config to\n"
        "                            $HOME/.config/plusafetch/config.jsonc\n"
        "  plusafetch --notify         Send a Termux notification when done\n"
        "                            (Android)\n"
        "  plusafetch --no-color       Disable colors\n"
        "  plusafetch --color          Always emit colors (even when piped)\n"
        "  plusafetch --version        Show version\n"
        "  plusafetch --help           Show this help\n"
        "\n"
        "CONFIG: $HOME/.config/plusafetch/config.jsonc (JSONC)\n"
        "  { \"modules\": [\"os\", \"cpu\", ...], \"colors\": true, \"truecolor\": true,\n"
        "    \"command\": \"date\", \"commandKey\": \"Uptime\" }\n"
        "\n"
        "No logo, ever. Text and data only.\n", kVersion);
}

void printListModules() {
    for (const auto* m : allModules()) {
        std::printf("%-12s %s\n", m->name, m->description ? m->description : "");
    }
}

int run(int argc, char** argv) {
    bool wantJson = false, wantNotify = false, wantAll = false, wantGenConfig = false, wantHelp = false, wantVersion = false, wantList = false, wantValuesOnly = false, wantVerify = false;
    bool formatSet = false;
    std::string formatText;
    std::string modulesCsv;
    int colorMode = 0; // 0 auto, 1 always, -1 none

    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        if (a == "--json") wantJson = true;
        else if (a == "--notify") wantNotify = true;
        else if (a == "--all") wantAll = true;
        else if (a == "--no-color") colorMode = -1;
        else if (a == "--color") colorMode = 1;
        else if (a == "--values-only") wantValuesOnly = true;
        else if (a == "--verify-config") wantVerify = true;
        else if (a == "--gen-config") wantGenConfig = true;
        else if (a == "--list-modules") wantList = true;
        else if (a == "--version" || a == "-v") wantVersion = true;
        else if (a == "--help" || a == "-h") wantHelp = true;
        else if (a == "--format" && i + 1 < argc) { formatText = argv[++i]; formatSet = true; }
        else if (a.rfind("--format=", 0) == 0) { formatText = std::string(a.substr(9)); formatSet = true; }
        else if (a == "--modules" && i + 1 < argc) modulesCsv = argv[++i];
        else if (a.rfind("--modules=", 0) == 0) modulesCsv = std::string(a.substr(10));
        else { std::fprintf(stderr, "%s: unknown option '%.*s'\n", kName, (int)a.size(), a.data()); return 64; }
    }

    if (wantVersion) { std::printf("plusafetch %s\n", kVersion); return 0; }
    if (wantHelp) { printUsage(stdout); return 0; }
    if (wantList) { printListModules(); return 0; }

    // Signal handlers for SIGINT/SIGTERM: reset to default and exit quietly.
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    signal(SIGHUP, SIG_IGN);

    FFContext ctx;
    if (wantVerify) {
        const std::string path = configPath();
        auto raw = readTextFile(path, 1u << 20);
        if (!raw) {
            std::fprintf(stderr, "%s: no config at %s (defaults will be used — OK)\n", kName, path.c_str());
            return 0;
        }
        std::string stripped = stripTrailingCommas(stripJsonComments(*raw));
        auto j = nlohmann::json::parse(stripped, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded()) {
            std::fprintf(stderr, "%s: config at %s is not valid JSONC\n", kName, path.c_str());
            return 65;
        }
        if (!j.is_object()) {
            std::fprintf(stderr, "%s: config at %s must be a JSON object\n", kName, path.c_str());
            return 65;
        }
        // Surface suspicious types so silently-empty effects don't surprise.
        if (j.contains("modules") && !j["modules"].is_array())
            std::fprintf(stderr, "%s: 'modules' is %s (expected array) — defaulting to builtin\n", kName, j["modules"].type_name());
        if (j.contains("colors") && !j["colors"].is_boolean())
            std::fprintf(stderr, "%s: 'colors' is %s (expected bool) — ignored\n", kName, j["colors"].type_name());
        if (j.contains("truecolor") && !j["truecolor"].is_boolean())
            std::fprintf(stderr, "%s: 'truecolor' is %s (expected bool) — ignored\n", kName, j["truecolor"].type_name());
        if (j.contains("hideKeys") && !j["hideKeys"].is_boolean())
            std::fprintf(stderr, "%s: 'hideKeys' is %s (expected bool) — ignored\n", kName, j["hideKeys"].type_name());
        if (j.contains("boldKeys") && !j["boldKeys"].is_boolean())
            std::fprintf(stderr, "%s: 'boldKeys' is %s (expected bool) — ignored\n", kName, j["boldKeys"].type_name());
        if (j.contains("separatorLength") && !j["separatorLength"].is_number())
            std::fprintf(stderr, "%s: 'separatorLength' is %s (expected number) — ignored\n", kName, j["separatorLength"].type_name());
        std::fprintf(stdout, "%s: config at %s is valid\n", kName, path.c_str());
        return 0;
    }
#if defined(__ANDROID__)
    ctx.platform = Platform::AndroidTermux;
#else
    ctx.platform = Platform::Linux;
#endif
    if (wantValuesOnly) ctx.hideKeys = true;

    // Config (before modules so 'command' is available to the Command module)
    nlohmann::json cfg = loadConfig();
    if (cfg.contains("command") && cfg["command"].is_string())
        ctx.configCommand = cfg["command"].get<std::string>();
    if (cfg.contains("commandKey") && cfg["commandKey"].is_string())
        ctx.configCommandKey = cfg["commandKey"].get<std::string>();

    // Customization knobs from config (CLI --values-only wins over config hideKeys)
    if (cfg.contains("boldKeys") && cfg["boldKeys"].is_boolean()) ctx.boldKeys = cfg["boldKeys"].get<bool>();
    if (cfg.contains("hideKeys") && cfg["hideKeys"].is_boolean() && cfg["hideKeys"].get<bool>()) ctx.hideKeys = true;
    if (cfg.contains("separatorLength") && cfg["separatorLength"].is_number()) {
        double d = cfg["separatorLength"].get<double>(); // get<int> on out-of-range floats would be UB
        ctx.separatorLength = d < 0 ? 0 : d > 200 ? 200 : static_cast<int>(d);
    }
    auto setKeyColor = [&](unsigned char r, unsigned char g, unsigned char b) {
        ctx.keyColorR = r; ctx.keyColorG = g; ctx.keyColorB = b; ctx.keyColorSet = true;
    };
    if (cfg.contains("keyColor")) {
        const auto& kc = cfg["keyColor"];
        if (kc.is_string()) { // "#rrggbb" handled (with or without #)
            std::string hex = kc.get<std::string>();
            if (!hex.empty() && hex.front() == '#') hex.erase(0, 1);
            if (hex.size() == 6) {
                auto hx = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                int d[6];
                bool ok = true;
                for (int i = 0; i < 6; ++i) { d[i] = hx(hex[i]); if (d[i] < 0) { ok = false; break; } }
                if (ok) setKeyColor((unsigned char)(d[0] * 16 + d[1]), (unsigned char)(d[2] * 16 + d[3]), (unsigned char)(d[4] * 16 + d[5]));
                else std::fprintf(stderr, "%s: config 'keyColor' not a hex color (use \"#rrggbb\")\n", kName);
            } else std::fprintf(stderr, "%s: config 'keyColor' not a hex color (use \"#rrggbb\")\n", kName);
        } else if (kc.is_array() && kc.size() >= 3 && kc[0].is_number() && kc[1].is_number() && kc[2].is_number()) {
            auto clamp = [](double v) { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : (int)v); };
            setKeyColor(clamp(kc[0].get<double>()), clamp(kc[1].get<double>()), clamp(kc[2].get<double>()));
        } else {
            std::fprintf(stderr, "%s: config 'keyColor' must be \"#rrggbb\" or [r,g,b]\n", kName);
        }
    }

    // Color resolution: --color / --no-color override config & env.
    const char* colorterm = std::getenv("COLORTERM");
    bool envTruecolor = colorterm && (std::string_view{colorterm}.find("truecolor") != std::string_view::npos
                                       || std::string_view{colorterm}.find("24bit") != std::string_view::npos);
    bool cfgTruecolor = cfg.contains("truecolor") && cfg["truecolor"].is_boolean() && cfg["truecolor"].get<bool>();
    bool cfgColor = !cfg.contains("colors") || (cfg["colors"].is_boolean() ? cfg["colors"].get<bool>() : true);
    ctx.truecolor = (cfgTruecolor || envTruecolor) && colorMode != -1;
    ctx.color = colorMode == 1 ? true : (colorMode == -1 ? false : cfgColor);

    if (wantGenConfig) {
        std::string tmpl =
            "// Plusafetch config (JSONC — comments allowed).\n"
            "{\n"
            "  // Modules in display order; remove or reorder as you like.\n"
            "  \"modules\": [\"title\",\"os\",\"kernel\",\"host\",\"cpu\",\"memory\",\"disk\",\"uptime\",\"packages\",\"shell\",\"terminal\",\"datetime\",\"separator\",\"colors\"],\n"
            "  \"colors\": true,          // master color switch\n"
            "  \"truecolor\": true,        // use 24-bit ANSI (\\033[38;2;R;G;Bm) keys & blocks\n"
            "  // \"keyColor\": \"#78B4FF\",   // hex RGB (or [r,g,b]) for the key column\n"
            "  // \"boldKeys\": true,       // render keys in bold\n"
            "  // \"hideKeys\": false,      // true => print values only, no keys (like --values-only)\n"
            "  // \"separatorLength\": 14,    // '─' count for the Separator module\n"
            "  // per-module key rename: entries may be { \"name\": \"cpu\", \"key\": \"Processor\" }\n"
            "  // \"command\": \"date\",    // Command module: run this sh -c string, prints the first line\n"
            "  // \"commandKey\": \"Uptime\" // the label to render for the Command module\n"
            "}\n";
        if (writeConfigAtomic(configPath(), tmpl)) {
            std::fprintf(stderr, "wrote %s\n", configPath().c_str());
            return 0;
        }
        std::fprintf(stderr, "%s: could not write config to %s\n", kName, configPath().c_str());
        return 74;
    }

    // Build the module list: CLI --modules > config "modules" > --all > default.
    // Config "modules" entries may be strings ("cpu") or objects ({"name":"cpu","key":"Processor"}).
    std::vector<const FFModuleBaseInfo*> mods;
    auto addName = [&](std::string_view n) {
        if (auto* m = findModule(n)) mods.push_back(m);
        else std::fprintf(stderr, "%s: warning: unknown module '%.*s' (ignored)\n", kName, (int)n.size(), n.data());
    };
    if (cfg.contains("modules") && !cfg["modules"].is_array())
        std::fprintf(stderr, "%s: warning: config 'modules' must be an array of strings/objects (ignoring it)\n", kName);
    if (!modulesCsv.empty()) {
        std::string_view s{modulesCsv};
        std::size_t pos = 0;
        while (pos < s.size()) {
            std::size_t comma = s.find(',', pos);
            std::string_view tok = s.substr(pos, comma == std::string_view::npos ? s.size() - pos : comma - pos);
            while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t')) tok.remove_prefix(1);
            while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t')) tok.remove_suffix(1);
            addName(tok);
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }
    } else if (cfg.contains("modules") && cfg["modules"].is_array()) {
        for (const auto& v : cfg["modules"]) {
            if (v.is_string()) { addName(v.get<std::string>()); continue; }
            if (v.is_object() && v.contains("name") && v["name"].is_string()) {
                std::string nm = v["name"].get<std::string>();
                if (auto* m = findModule(nm)) {
                    mods.push_back(m);
                    if (v.contains("key") && v["key"].is_string())
                        ctx.keyRenames[m->name] = v["key"].get<std::string>();
                    else if (v.contains("key"))
                        std::fprintf(stderr, "%s: warning: config module '%s' 'key' must be a string (ignored)\n", kName, nm.c_str());
                } else {
                    std::fprintf(stderr, "%s: warning: unknown module '%s' (ignored)\n", kName, nm.c_str());
                }
                continue;
            }
            std::fprintf(stderr, "%s: warning: config 'modules' entries must be strings or {\"name\": ...} objects (ignored)\n", kName);
        }
    } else if (wantAll) {
        for (const char* n : kAllStructure) addName(n);
    } else {
        for (const char* n : kDefaultStructure) addName(n);
    }
    if (mods.empty()) {
        std::fprintf(stderr, "%s: no valid modules selected (check names with --list-modules)\n", kName);
        return 65;
    }

    g_ctx = &ctx;

    // ---- Phase 1: parallel detection (independent modules) ----
    runDetectParallel(ctx, mods);

    // ---- Phase 2: render ----
    std::string frame; // built only on the single-write path
    if (wantJson) {
        nlohmann::json out = nlohmann::json::object();
        for (const auto* m : mods) {
            nlohmann::json slot(nullptr);
            if (m->generateJsonResult) m->generateJsonResult(const_cast<FFModuleBaseInfo*>(m), slot);
            std::string jkey{m->displayName ? m->displayName : m->name};
            { auto it = ctx.keyRenames.find(m->name); if (it != ctx.keyRenames.end() && !it->second.empty()) jkey = it->second; }
            if (out.contains(jkey)) {
                std::fprintf(stderr, "%s: warning: duplicate JSON key '%s' (keeping first; rename keys must be unique)\n", kName, jkey.c_str());
                continue;
            }
            out[jkey] = std::move(slot);
        }
        std::string payload = out.dump(2) + "\n";
        writeAll(STDOUT_FILENO, payload.data(), payload.size());
    } else if (formatSet) {
        std::string rendered = renderFormatTemplate(ctx, formatText) + "\n";
        writeAll(STDOUT_FILENO, rendered.data(), rendered.size());
    } else {
        for (const auto* m : mods) {
            if (m->printModule) m->printModule(const_cast<FFModuleBaseInfo*>(m));
        }
        writeAll(STDOUT_FILENO, ctx.frame.b.chars, ctx.frame.b.length);
    }

    if (wantNotify) {
#if defined(__ANDROID__)
        const char* const argv[] = { "termux-notification", "--title", "Plusafetch", "--content", "Fetch complete", nullptr };
        auto r = execCapture(argv, 64); // fire-and-forget
        (void)r;
#else
        std::fprintf(stderr, "%s: --notify is Android-only (install termux-api)\n", kName);
#endif
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (...) {
        std::fprintf(stderr, "%s: internal error\n", kName);
        return 70;
    }
}
