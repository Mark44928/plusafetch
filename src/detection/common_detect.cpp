// Platform-agnostic detectors shared by Android(Termux) and Linux.
// All reads are bounded; every function never throws and fills its
// disjoint FFContext field.
#include "platform.hpp"
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace plf {

// ---- CPU ----
void detectCpu(FFContext& ctx) {
    auto cpu = readTextFile("/proc/cpuinfo");
    if (!cpu) { ctx.cpuLine = std::unexpected(Error{"cpuinfo unavailable"}); return; }
    std::string name;
    int cores = 0;
    std::string line;
    std::size_t pos = 0;
    while (pos < cpu->size()) {
        std::size_t eol = cpu->find('\n', pos);
        if (eol == std::string::npos) eol = cpu->size();
        line = cpu->substr(pos, eol - pos);
        pos = eol + 1;
        // arm64: "Processor  : ..." / x86: "model name\t: ..." / "Hardware\t:"
        if (name.empty()) {
            const char* keys[] = { "model name", "Hardware", "Processor", nullptr };
            for (const char** k = keys; *k; ++k) {
                auto t = line.find(*k);
                if (t != std::string::npos) {
                    auto c = line.find(':', t);
                    if (c != std::string::npos) {
                        name = trimVal(line.substr(c + 1)).value_or("");
                        break;
                    }
                }
            }
        }
        if (line.rfind("processor", 0) == 0) ++cores;
    }
    if (name.empty()) name = "unknown CPU";
    if (cores <= 0) {
        const char* const nprocArgv[] = { "nproc", nullptr };
        auto nprocRes = execCapture(nprocArgv, 64);
        cores = nprocRes ? std::atoi(nprocRes->c_str()) : 0;
        if (cores <= 0) cores = 1;
    }
    char out[288];
    std::snprintf(out, sizeof out, "%s (%d cores)", name.c_str(), cores);
    ctx.cpuLine = std::string{out};
}

// ---- Memory ----
void detectMemory(FFContext& ctx) {
    auto mem = readTextFile("/proc/meminfo");
    if (!mem) { ctx.memLine = std::unexpected(Error{"meminfo unavailable"}); return; }
    auto fieldKiB = [&](std::string_view key) -> uint64_t {
        std::size_t pos = mem->find(key);
        if (pos == std::string::npos) return 0;
        auto colon = mem->find(':', pos);
        if (colon == std::string::npos) return 0;
        std::size_t end = mem->find_first_not_of(" \t", colon + 1);
        if (end == std::string::npos) return 0;
        std::size_t stop = mem->find_first_not_of("0123456789", end);
        if (stop == std::string::npos) stop = mem->size();
        return std::strtoull(mem->substr(end, stop - end).c_str(), nullptr, 10);
    };
    uint64_t total = fieldKiB("MemTotal:");
    uint64_t avail = fieldKiB("MemAvailable:");
    if (total == 0) { ctx.memLine = std::unexpected(Error{"MemTotal missing"}); return; }
    uint64_t used = avail < total ? total - avail : 0;
    char out[160];
    std::snprintf(out, sizeof out, "%.1f GiB used / %.1f GiB",
                  used  / 1048576.0, total / 1048576.0);
    ctx.memLine = std::string{out};
}

// ---- Disk (root filesystem) ----
void detectDisk(FFContext& ctx) {
    struct statvfs st{};
    if (statvfs("/", &st) != 0 || st.f_blocks == 0) {
        ctx.diskLine = std::unexpected(Error{"statvfs(/) failed"});
        return;
    }
    uint64_t total = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
    uint64_t free  = static_cast<uint64_t>(st.f_bfree)  * st.f_frsize;
    uint64_t used  = total > free ? total - free : 0;
    char out[160];
    std::snprintf(out, sizeof out, "%.1f GiB used / %.1f GiB", used / 1073741824.0, total / 1073741824.0);
    ctx.diskLine = std::string{out};
}

// ---- Kernel ----
void detectKernel(FFContext& ctx) {
    struct utsname u{};
    if (uname(&u) != 0) { ctx.kernelLine = std::unexpected(Error{"uname failed"}); return; }
    ctx.kernelLine = std::string(u.sysname) + " " + u.release;
}

// ---- Uptime ----
void detectUptime(FFContext& ctx) {
    auto up = readTextFile("/proc/uptime");
    if (up) {
        double secs = std::strtod(up->c_str(), nullptr);
        if (secs > 0 || up->size() > 2) {
            uint64_t s = static_cast<uint64_t>(secs);
            uint64_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
            char out[128];
            if (d) std::snprintf(out, sizeof out, "%llud %lluh %llum", (unsigned long long)d, (unsigned long long)h, (unsigned long long)m);
            else if (h) std::snprintf(out, sizeof out, "%lluh %llum", (unsigned long long)h, (unsigned long long)m);
            else std::snprintf(out, sizeof out, "%llum %llus", (unsigned long long)m, (unsigned long long)(s % 60));
            ctx.uptimeLine = std::string{out};
            return;
        }
    }
#ifdef __ANDROID__
    // ro.runtime.firstboot is empty on many devices; treat as N/A then.
    std::string first = androidProp("ro.runtime.firstboot");
    if (!first.empty()) {
        uint64_t boot = std::strtoull(first.c_str(), nullptr, 10);
        if (boot > 0 && first.size() >= 12) {
            using namespace std::chrono;
            auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            uint64_t secs = (uint64_t)ms / 1000 > boot / 1000 ? ((uint64_t)ms - boot) / 1000 : 0;
            uint64_t d = secs / 86400, h = (secs % 86400) / 3600, m = (secs % 3600) / 60;
            char out[128];
            std::snprintf(out, sizeof out, "%llud %lluh %llum", (unsigned long long)d, (unsigned long long)h, (unsigned long long)m);
            ctx.uptimeLine = std::string{out};
            return;
        }
    }
#endif
    ctx.uptimeLine = std::unexpected(Error{"uptime unavailable on this platform"});
}

// ---- Shell ----
void detectShell(FFContext& ctx) {
    const char* sh = std::getenv("SHELL");
    if (!sh || !*sh) { ctx.shellLine = std::unexpected(Error{"SHELL unset"}); return; }
    std::string s{sh};
    auto slash = s.rfind('/');
    ctx.shellLine = slash == std::string::npos ? s : s.substr(slash + 1);
}

// ---- Terminal ----
void detectTerminal(FFContext& ctx) {
    const char* t = std::getenv("TERM");
    if (!t || !*t) { ctx.terminalLine = std::unexpected(Error{"TERM unset"}); return; }
    ctx.terminalLine = std::string{t};
}

// ---- Locale ----
void detectLocale(FFContext& ctx) {
    const char* keys[] = { "LC_ALL", "LC_CTYPE", "LC_MESSAGES", "LANG", nullptr };
    for (const char** k = keys; *k; ++k) {
        const char* v = std::getenv(*k);
        if (v && *v) { ctx.localeLine = std::string{v}; return; }
    }
    ctx.localeLine = std::unexpected(Error{"no locale env set"});
}

// ---- DateTime ----
void detectDateTime(FFContext& ctx) {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
    if (std::tm* tm_ptr = std::localtime(&now)) tmBuf = *tm_ptr;
    char out[128];
    if (std::strftime(out, sizeof out, "%a %b %d, %H:%M:%S", &tmBuf) == 0) {
        ctx.dateTimeLine = std::unexpected(Error{"strftime failed"});
        return;
    }
    ctx.dateTimeLine = std::string{out};
}

// ---- Packages (dpkg status paragraphs) ----
void detectPackages(FFContext& ctx) {
    std::string target;
    if (const char* envPref = std::getenv("PREFIX")) target = std::string(envPref) + "/var/lib/dpkg/status";
    else target =
#ifdef __ANDROID__
        "/data/data/com.termux/files/usr/var/lib/dpkg/status";
#else
        "/var/lib/dpkg/status";
#endif
    auto whole = readTextFile(target, 8u << 20);
    if (!whole) {
#ifdef __linux__
        const char* rpm[] = { "rpm", "-qa", nullptr };
        auto r = execCapture(rpm, 1u << 20);
        if (r) {
            int n = 0; for (char c : *r) if (c == '\n') ++n;
            ctx.pkgLine = std::to_string(std::max(0, n)) + " rpm";
            return;
        }
#endif
        ctx.pkgLine = std::unexpected(Error{"no dpkg/rpm database found"});
        return;
    }
    uint64_t pkgs = 0;
    std::size_t pos = 0;
    while (pos < whole->size()) {
        pos = whole->find("Package:", pos);
        if (pos == std::string::npos) break;
        ++pkgs;
        ++pos;
    }
    ctx.pkgLine = std::to_string(pkgs) + " (dpkg)";
}

} // namespace plf
