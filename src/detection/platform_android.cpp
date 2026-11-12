// Android/Termux detection: OS from the Android property service via NDK
// (zero subprocess), host from ro.product.model / ro.product.manufacturer,
// battery: /sys/class/power_supply/battery first (SELinux-denied on most
// unrooted devices), termux-api fallback. Sensors / telephony / wifi are
// termux-api subprocesses (parallel, never on the default path).
#include "platform.hpp"
#include <sys/utsname.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <vector>

namespace plf {

void detectOsAndroid(FFContext& ctx) {
    std::string release;
    bool have = false;
#ifdef __ANDROID__
    release = androidProp("ro.build.version.release");
    have = !release.empty();
#endif
    struct utsname u{};
    std::string kernelRelease;
    if (uname(&u) == 0) kernelRelease = u.release;

    if (have) ctx.osLine = std::string("Android ") + release;
    else if (!kernelRelease.empty()) ctx.osLine = std::string("Android (kernel ") + kernelRelease + ")";
    else ctx.osLine = std::unexpected(Error{"android release unknown"});
    ctx.androidRelease = std::move(release);
    ctx.androidModel = androidProp("ro.product.model");
    ctx.androidManufacturer = androidProp("ro.product.manufacturer");
}

void detectHostAndroid(FFContext& ctx) {
    // The Termux hostname is hard-coded to "localhost"; surface the hardware.
    std::string model;
#ifdef __ANDROID__
    model = androidProp("ro.product.model");
#endif
    if (!model.empty()) { ctx.hostLine = std::move(model); return; }
    char buf[256]{};
    if (gethostname(buf, sizeof buf) == 0 && buf[0] != '\0' && std::string_view{buf} != "localhost")
        ctx.hostLine = buf;
    else
        ctx.hostLine = std::unexpected(Error{"host unavailable (no model, localhost)"});
}

// Battery: try linux sysfs (fast, zero fork) — typically SELinux-denied,
// fallback to termux-battery-status (termux-api package required).
void detectBatteryAndroid(FFContext& ctx) {
    const char* bats[] = { "/sys/class/power_supply/battery", "/sys/class/power_supply/bms", nullptr };
    for (const char** b = bats; *b; ++b) {
        auto cap = readFirstLine(std::string(*b) + "/capacity");
        if (cap) {
            std::string status = readFirstLine(std::string(*b) + "/status").value_or("");
            std::string out = *cap + "%";
            if (!status.empty()) out += " (" + status + ")";
            ctx.batteryLine = std::move(out);
            return;
        }
    }
    const char* const argv[] = { "termux-battery-status", nullptr };
    auto raw = execCapture(argv, 4096);
    if (!raw) { ctx.batteryLine = std::unexpected(Error{"no battery source (sysfs denied, termux-api missing)"}); return; }
    try {
        auto j = nlohmann::json::parse(*raw, nullptr, false);
        if (!j.is_object()) { ctx.batteryLine = std::unexpected(Error{"battery: invalid JSON"}); return; }
        std::string out;
        if (j.contains("percentage")) out = j["percentage"].dump() + "%";
        if (j.contains("status"))    out += " (" + j["status"].get<std::string>() + ")";
        if (j.contains("health") && j["health"].get<std::string>() != "GOOD")
            out += " · " + j["health"].get<std::string>();
        if (j.contains("temperature")) {
            // v50/v51 termux-api formats: number or locale-decimal string ("27,4" / "27.4")
            std::string t = j["temperature"].is_string() ? j["temperature"].get<std::string>() : j["temperature"].dump();
            for (auto& ch : t) if (ch == ',') ch = '.';
            out += ", " + t + "°C";
        }
        if (j.contains("plugged") && j["plugged"].get<std::string>() != "PLUGGED_USB_ANDROID")
            out += ", " + j["plugged"].get<std::string>();
        ctx.batteryLine = std::move(out);
    } catch (...) {
        ctx.batteryLine = std::unexpected(Error{"battery parse failed"});
    }
}

// Sensors: exact names from `termux-sensor -l`; values array joined with ", ".
void detectSensorsAndroid(FFContext& ctx) {
    const char* const listArgv[] = { "termux-sensor", "-l", nullptr };
    auto raw = execCapture(listArgv, 8192);
    if (!raw) { ctx.sensorLine = std::unexpected(Error{"termux-api missing"}); return; }
    std::vector<std::string> names;
    try {
        auto j = nlohmann::json::parse(*raw, nullptr, false);
        if (j.is_object() && j.contains("sensors") && j["sensors"].is_array())
            for (const auto& s : j["sensors"]) if (s.is_string()) names.push_back(s.get<std::string>());
    } catch (...) {}
    if (names.empty()) { ctx.sensorLine = std::unexpected(Error{"no sensors listed"}); return; }

    struct Wish { const char* want; const char* label; };
    const Wish wishes[] = {
        {"Accelerometer", "Accel"},
        {"Light sensor", "Light"},
        {"Proximity sensor", "Prox"},
        {"Gyroscope", "Gyro"},
    };
    std::string out;
    int shown = 0;
    for (const auto& w : wishes) {
        auto it = std::find(names.begin(), names.end(), w.want);
        if (it == names.end()) continue; // exact match only (case-sensitive as in -l output)
        const char* const argv[] = { "termux-sensor", "-s", it->c_str(), "-n", "1", nullptr };
        auto one = execCapture(argv, 8192);
        if (!one) continue;
        try {
            auto j = nlohmann::json::parse(*one, nullptr, false);
            if (!j.is_object() || !j.contains(w.want) || !j[w.want].is_object()) continue;
            const auto& vs = j[w.want]["values"];
            if (!vs.is_array() || vs.empty()) continue;
            if (!out.empty()) out += "  ";
            out += w.label;
            out += ": ";
            bool first = true;
            for (const auto& v : vs) {
                if (!first) out += ", ";
                first = false;
                if (v.is_number()) {
                    char nb[32];
                    std::snprintf(nb, sizeof nb, "%.2f", v.get<double>());
                    out += nb;
                }
            }
            ++shown;
        } catch (...) {}
    }
    if (shown == 0) ctx.sensorLine = std::unexpected(Error{"no wishlist sensors present"});
    else ctx.sensorLine = std::move(out);
}

void detectTelephonyAndroid(FFContext& ctx) {
    const char* const argv[] = { "termux-telephony-deviceinfo", nullptr };
    auto raw = execCapture(argv, 8192);
    if (!raw) { ctx.telephonyLine = std::unexpected(Error{"termux-api missing"}); return; }
    try {
        auto j = nlohmann::json::parse(*raw, nullptr, false);
        if (!j.is_object()) { ctx.telephonyLine = std::unexpected(Error{"invalid JSON"}); return; }
        auto str = [&](const char* k) -> std::string {
            if (!j.contains(k)) return {};
            const auto& v = j[k];
            if (v.is_string()) return v.get<std::string>();
            if (v.is_null()) return {};
            return v.dump(); // booleans/numbers render verbatim
        };
        std::string op = str("network_operator_name");
        std::string net = str("network_type");
        std::string pt  = str("phone_type");
        std::string de  = str("data_enabled");
        std::vector<std::string> parts;
        if (!op.empty())  parts.push_back(op);
        if (!net.empty()) parts.push_back("(" + net + ")");
        if (!pt.empty())  parts.push_back(pt);
        if (!de.empty())  parts.push_back(std::string("data ") + (de == "true" ? "on" : "off"));
        std::string out;
        for (const auto& p2 : parts) { if (!out.empty()) out += " · "; out += p2; }
        if (out.empty()) { ctx.telephonyLine = std::unexpected(Error{"all fields empty"}); return; }
        ctx.telephonyLine = std::move(out);
    } catch (...) {
        ctx.telephonyLine = std::unexpected(Error{"telephony parse failed"});
    }
}

void detectWifiAndroid(FFContext& ctx) {
    const char* const argv[] = { "termux-wifi-connectioninfo", nullptr };
    auto raw = execCapture(argv, 8192);
    if (!raw) { ctx.wifiLine = std::unexpected(Error{"termux-api missing"}); return; }
    try {
        auto j = nlohmann::json::parse(*raw, nullptr, false);
        if (!j.is_object()) { ctx.wifiLine = std::unexpected(Error{"invalid JSON"}); return; }
        if (j.contains("API_ERROR")) { ctx.wifiLine = std::string("N/A (") + j["API_ERROR"].dump() + ")"; return; }
        auto str = [&](const char* k) -> std::string {
            if (!j.contains(k)) return {};
            const auto& v = j[k];
            if (v.is_string()) return v.get<std::string>();
            if (v.is_null()) return {};
            return v.dump();
        };
        std::string ssid = str("ssid");
        std::vector<std::string> parts;
        if (!ssid.empty() && ssid != "<unknown ssid>") parts.push_back(ssid);
        if (!str("ip").empty()) parts.push_back("IP " + str("ip"));
        if (!str("link_speed_mbps").empty()) parts.push_back(str("link_speed_mbps") + " Mb/s");
        if (!str("rssi").empty()) parts.push_back("rssi " + str("rssi") + " dBm");
        std::string out;
        for (const auto& p2 : parts) { if (!out.empty()) out += " · "; out += p2; }
        if (out.empty()) out = "N/A";
        ctx.wifiLine = std::move(out);
    } catch (...) {
        ctx.wifiLine = std::unexpected(Error{"wifi parse failed"});
    }
}

void detectBrightnessAndroid(FFContext& ctx) {
    // termux-brightness is set-only; there is no unrooted read path.
    ctx.brightnessLine = std::unexpected(Error{"unavailable (termux-api brightness is write-only unrooted)"});
}


} // namespace plf
