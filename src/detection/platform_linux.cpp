// Linux detection: OS from /etc/os-release (PRETTY_NAME), host from
// hostname + optional DMI product_name (printed N/A when unreadable),
// battery from /sys/class/power_supply/BAT0 (Linux only),
// GPU via lspci when present (printed N/A otherwise).
#include "platform.hpp"
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace plf {

// ---- collect key=value pairs from /etc/os-release ----
static std::string osReleaseValue(std::string_view content, std::string_view key) {
    std::size_t pos = 0;
    const std::string k = std::string{key} + "=";
    while (pos < content.size()) {
        std::size_t eol = content.find('\n', pos);
        if (eol == std::string::npos) eol = content.size();
        std::string_view line = std::string_view{content}.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.rfind(k, 0) != 0) continue;
        std::string v{line.substr(k.size())};
        if (!v.empty() && v.front() == '"' && v.back() == '"' && v.size() > 1) v = v.substr(1, v.size() - 2);
        return v;
    }
    return {};
}

void detectOsLinux(FFContext& ctx) {
    const char* candidates[] = { "/etc/os-release", "/usr/lib/os-release", "/usr/lib64/os-release", nullptr };
    for (const char** c = candidates; *c; ++c) {
        auto f = readTextFile(*c);
        if (!f) continue;
        std::string pretty = osReleaseValue(*f, "PRETTY_NAME");
        if (!pretty.empty()) { ctx.osLine = std::move(pretty); return; }
        std::string name = osReleaseValue(*f, "NAME");
        if (!name.empty()) { ctx.osLine = std::move(name); return; }
    }
    struct utsname u{};
    if (uname(&u) == 0) ctx.osLine = std::string(u.sysname) + " " + u.release;
    else ctx.osLine = std::unexpected(Error{"os-release unavailable"});
}

void detectHostLinux(FFContext& ctx) {
    char buf[256]{};
    if (gethostname(buf, sizeof buf) == 0 && buf[0] != '\0') ctx.hostLine = buf;
    else ctx.hostLine = std::unexpected(Error{"gethostname failed"});
}

void detectBatteryLinux(FFContext& ctx) {
    // Typical sysfs battery on Linux laptops; on Android this is SELinux-denied
    // and the termux-api fallback path handles it instead.
    const char* bats[] = { "/sys/class/power_supply/BAT0", "/sys/class/power_supply/BAT1", nullptr };
    for (const char** b = bats; *b; ++b) {
        auto cap = readFirstLine(std::string(*b) + "/capacity");
        if (!cap) continue;
        std::string status = readFirstLine(std::string(*b) + "/status").value_or("");
        std::string health = readFirstLine(std::string(*b) + "/health").value_or("");
        std::string model  = readFirstLine(std::string(*b) + "/model_name").value_or("");
        std::string out = *cap + "%";
        if (!status.empty()) out += " (" + status + ")";
        if (!model.empty()) out += " · " + model;
        if (!health.empty() && health != "Good" && health != "GOOD") out += " · " + health;
        ctx.batteryLine = std::move(out);
        return;
    }
    ctx.batteryLine = std::unexpected(Error{"no battery sysfs exposed"});
}

void detectGpuLinux(FFContext& ctx) {
    const char* const argv[] = { "lspci", "-d", "::0300", "-nn", nullptr };
    auto r = execCapture(argv, 8192);
    if (!r) { ctx.gpuLine = Result<std::string>{"N/A"}; return; }
    std::string line = *r;
    auto nl = line.find('\n');
    if (nl != std::string::npos) line = line.substr(0, nl);
    // Strip vendor brackets like "[10de:2484]"
    std::string out;
    bool bracket = false;
    for (char ch : line) {
        if (ch == '[') { bracket = true; continue; }
        if (ch == ']') { bracket = false; continue; }
        if (!bracket) out += ch;
    }
    // lspci lines look like "0000:00:02.0 VGA compatible controller: Device ...";
    // the first ": " separates the class description from the device name —
    // the bare colons before it belong to the bus slot ("0000:00:02.0").
    auto b = out.find(": ");
    out = trimVal(b == std::string::npos ? out : out.substr(b + 2)).value_or("");
    ctx.gpuLine = out.empty() ? Result<std::string>{"N/A"} : Result<std::string>{std::move(out)};
}

} // namespace plf
