#pragma once
#include "context.hpp"
#include "../core/error.hpp"

namespace plf {
// Shared helpers (defined in platform.cpp)
std::string androidProp(const char* key) noexcept;
std::string stripJsonComments(std::string_view src);
std::string stripTrailingCommas(std::string_view src);
std::string sanitizeForTerminal(std::string_view s);
bool fileReadable(std::string_view path);

// Shared detectors (common_detect.cpp) — POSIX, all platforms
void detectCpu(FFContext& ctx);
void detectMemory(FFContext& ctx);
void detectDisk(FFContext& ctx);
void detectKernel(FFContext& ctx);
void detectUptime(FFContext& ctx);
void detectPackages(FFContext& ctx);
void detectShell(FFContext& ctx);
void detectTerminal(FFContext& ctx);
void detectLocale(FFContext& ctx);
void detectDateTime(FFContext& ctx);

// Linux detectors (platform_linux.cpp)
void detectOsLinux(FFContext& ctx);
void detectHostLinux(FFContext& ctx);
void detectBatteryLinux(FFContext& ctx);
void detectGpuLinux(FFContext& ctx);

// Android/Termux detectors (platform_android.cpp)
void detectOsAndroid(FFContext& ctx);
void detectHostAndroid(FFContext& ctx);
void detectBatteryAndroid(FFContext& ctx);
void detectSensorsAndroid(FFContext& ctx);
void detectTelephonyAndroid(FFContext& ctx);
void detectWifiAndroid(FFContext& ctx);
void detectBrightnessAndroid(FFContext& ctx);
} // namespace plf
