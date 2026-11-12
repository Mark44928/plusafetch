#pragma once
// Detection context: one per invocation, filled in parallel by the jthread
// fan-out (each detector writes only its own disjoint fields), consumed by
// the render phase (single-threaded). Never throws.
#include <chrono>
#include "../core/ffstrbuf.hpp"
#include "../core/error.hpp"

namespace plf {

enum class Platform : unsigned char { AndroidTermux, Linux };

struct FFContext {
    Platform platform = Platform::Linux;

    // Render flags
    bool color     = true;
    bool truecolor = false;
    bool jsonMode  = false;
    std::string formatText;     // --format template, empty => default layout

    // Frame output (normal mode)
    FFStrBufGuard frame;

    // ---- Detection results (each written exactly once by one jthread) ----
    Result<std::string> osLine      {std::unexpected(Error{"unsupported"})};
    Result<std::string> kernelLine  {std::unexpected(Error{"unsupported"})};
    Result<std::string> hostLine    {std::unexpected(Error{"unsupported"})};
    Result<std::string> cpuLine     {std::unexpected(Error{"unsupported"})};
    Result<std::string> memLine     {std::unexpected(Error{"unsupported"})};
    Result<std::string> diskLine    {std::unexpected(Error{"unsupported"})};
    Result<std::string> uptimeLine  {std::unexpected(Error{"unsupported"})};
    Result<std::string> pkgLine     {std::unexpected(Error{"unsupported"})};
    Result<std::string> shellLine   {std::unexpected(Error{"unsupported"})};
    Result<std::string> terminalLine{std::unexpected(Error{"unsupported"})};
    Result<std::string> localeLine  {std::unexpected(Error{"unsupported"})};
    Result<std::string> dateTimeLine{std::unexpected(Error{"unsupported"})};
    Result<std::string> batteryLine {std::unexpected(Error{"unsupported"})};
    Result<std::string> brightnessLine{std::unexpected(Error{"unsupported"})};
    Result<std::string> sensorLine  {std::unexpected(Error{"unsupported"})};
    Result<std::string> telephonyLine{std::unexpected(Error{"unsupported"})};
    Result<std::string> wifiLine    {std::unexpected(Error{"unsupported"})};
    Result<std::string> commandLine {std::unexpected(Error{"unsupported"})};
    Result<std::string> clipboardLine{std::unexpected(Error{"unsupported"})};
    Result<std::string> gpuLine      {std::unexpected(Error{"unsupported"})};

    // Config (from $HOME/.config/plusafetch/config.jsonc)
    std::string configCommand;    // "" => Command module shows N/A
    std::string configCommandKey{"Command"};

    // Host pieces (computed once, reused by title/host/linuxgpu)
    std::string userName;
    std::string hostName;
    std::string androidRelease;
    std::string androidModel;
    std::string androidManufacturer;

    FFContext() {
        const char* u = std::getenv("LOGNAME");
        if (!u) u = std::getenv("USER");
        userName = u ? u : "user";
        const char* h = std::getenv("COMPUTERNAME"); // windows-free, but harmless fallback
        if (h && *h) hostName = h;
    }
};

// Non-owning global for the render phase (single-threaded); never null after main sets it.
inline FFContext* g_ctx = nullptr;

} // namespace plf
