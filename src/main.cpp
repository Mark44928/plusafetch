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

volatile std::sig_atomic_t g_quit = 0;
extern "C" void onSignal(int) { g_quit = 1; _exit(130); }

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
    return j.is_object() ? j : nlohmann::json::object();
}

bool writeConfigAtomic(const std::string& dest, const std::string& payload) {
    std::string tmpDir = std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp";
    std::string tmp = tmpDir + "/plusafetch_config.tmp";
    int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    for (std::size_t off = 0; off < payload.size(); ) {
        ssize_t w = ::write(fd, payload.data() + off, payload.size() - off);
        if (w <= 0) { ::close(fd); return false; }
        off += static_cast<std::size_t>(w);
    }
    ::close(fd);
    // ensure the parent directory exists
    std::string dir = dest.substr(0, dest.rfind('/'));
    std::string mkcmd = std::string{};
    // mkdir -p style: create each segment
    std::string cur;
    for (char c : dir) { cur += c; if (c == '/' && cur.size() > 1) ::mkdir(cur.c_str(), 0755); }
    if (::rename(tmp.c_str(), dest.c_str()) != 0) { ::unlink(tmp.c_str()); return false; }
    return true;
}

void printUsage(FILE* out) {
    std::fprintf(out,
        "plusafetch %s — modular C++23 system fetch for Termux/Android and Linux\n\n"
        "USAGE:\n"
        "  plusafetch                 Show the default layout\n"
        "  plusafetch --all           Include termux-api modules (battery, sensors, telephony, wifi, clipboard, command, gpu)\n"
        "  plusafetch --json          JSON dump of detected modules\n"
        "  plusafetch --format FMT    FMT with placeholders like {os}, {cpu}, {memory}; slices: {os~0,5}\n"
        "  plusafetch --modules a,b   Comma-separated module list (default: config or built-in)\n"
        "  plusafetch --list-modules  List every registered module\n"
        "  plusafetch --gen-config    Write default config to $HOME/.config/plusafetch/config.jsonc\n"
        "  plusafetch --notify        Send a Termux notification when done (Android)\n"
        "  plusafetch --no-color      Disable colors\n"
        "  plusafetch --version       Show version\n"
        "  plusafetch --help          Show this help\n\n"
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
    bool wantJson = false, wantNotify = false, wantAll = false, wantGenConfig = false, wantHelp = false, wantVersion = false, wantList = false;
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
        else if (a == "--gen-config") wantGenConfig = true;
        else if (a == "--list-modules") wantList = true;
        else if (a == "--version" || a == "-v") wantVersion = true;
        else if (a == "--help" || a == "-h") wantHelp = true;
        else if (a == "--format" && i + 1 < argc) formatText = argv[++i];
        else if (a.rfind("--format=", 0) == 0) formatText = std::string(a.substr(9));
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
#if defined(__ANDROID__)
    ctx.platform = Platform::AndroidTermux;
#else
    ctx.platform = Platform::Linux;
#endif
    // Config (before modules so 'command' is available to the Command module)
    nlohmann::json cfg = loadConfig();
    if (cfg.contains("command") && cfg["command"].is_string())
        ctx.configCommand = cfg["command"].get<std::string>();
    if (cfg.contains("commandKey") && cfg["commandKey"].is_string())
        ctx.configCommandKey = cfg["commandKey"].get<std::string>();

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
    std::vector<const FFModuleBaseInfo*> mods;
    auto addName = [&](std::string_view n) { if (auto* m = findModule(n)) mods.push_back(m); };
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
        for (const auto& v : cfg["modules"]) if (v.is_string()) addName(v.get<std::string>());
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
            out[m->displayName ? std::string{m->displayName} : std::string{m->name}] = std::move(slot);
        }
        std::string payload = out.dump(2) + "\n";
        ssize_t w = ::write(STDOUT_FILENO, payload.data(), payload.size());
        (void)w;
    } else if (!formatText.empty()) {
        std::string rendered = renderFormatTemplate(ctx, formatText) + "\n";
        ssize_t w = ::write(STDOUT_FILENO, rendered.data(), rendered.size());
        (void)w;
    } else {
        for (const auto* m : mods) {
            if (m->printModule) m->printModule(const_cast<FFModuleBaseInfo*>(m));
        }
        ffStrbufAppendS(ctx.frame.b, ""); // no trailing newline surprise; each module line adds its own
        ssize_t w = ::write(STDOUT_FILENO, ctx.frame.b.chars, ctx.frame.b.length);
        (void)w;
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
