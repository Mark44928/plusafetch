// Shared IO primitives: bounded file reads, fork+execve subprocess capture,
// JSONC comment stripping, and Android property reads (NDK). Linux variants
// live in platform_linux.cpp; Android variants in platform_android.cpp.
#include "../core/ffstrbuf.hpp"
#include "../core/error.hpp"
#include "context.hpp"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <cctype>
#include <vector>
#include <array>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

namespace plf {

Result<std::string> readTextFile(std::string_view path, std::size_t maxBytes) {
    int fd = ::open(std::string(path).c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::unexpected(errCtx("open failed for " + std::string(path), errno));
    std::string out;
    out.reserve(512);
    char blk[2048];
    std::size_t total = 0;
    for (;;) {
        ssize_t n = ::read(fd, blk, sizeof blk);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return std::unexpected(errCtx("read failed", errno));
        }
        if (n == 0) break;
        total += static_cast<std::size_t>(n);
        if (total > maxBytes) {
            out.append(blk, maxBytes - (total - static_cast<std::size_t>(n)));
            break;
        }
        out.append(blk, static_cast<size_t>(n));
    }
    ::close(fd);
    return std::move(out);
}

Result<std::string> readFirstLine(std::string_view path) {
    return readTextFile(path)
        .and_then([](std::string s) -> Result<std::string> {
            auto nl = s.find('\n');
            return trimVal(nl == std::string::npos ? s : s.substr(0, nl));
        });
}

Result<std::string> execCapture(const char* const argv[], std::size_t maxBytes) {
    int pipefd[2];
    if (::pipe(pipefd) != 0)
        return std::unexpected(errCtx("pipe failed", errno));
    for (int fd : pipefd) {
        int fl = ::fcntl(fd, F_GETFD);
        if (fl >= 0) ::fcntl(fd, F_SETFD, fl | FD_CLOEXEC);
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]); ::close(pipefd[1]);
        return std::unexpected(errCtx("fork failed", errno));
    }
    if (pid == 0) {
        // child: stdout (and stderr) into the pipe, /dev/null for stdin
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[0]); ::close(pipefd[1]);
        int nul = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (nul >= 0) { ::dup2(nul, STDIN_FILENO); if (nul > 2) ::close(nul); }
        ::execvp(argv[0], const_cast<char* const*>(argv));
        _exit(127); // exec failed
    }
    ::close(pipefd[1]);
    std::string out;
    out.reserve(512);
    char blk[4096];
    std::size_t total = 0;
    for (;;) {
        ssize_t n = ::read(pipefd[0], blk, sizeof blk);
        if (n < 0) { if (errno == EINTR) continue; break; }
        if (n == 0) break;
        total += static_cast<std::size_t>(n);
        if (total > maxBytes) { out.append(blk, maxBytes - (total - static_cast<std::size_t>(n))); break; }
        out.append(blk, static_cast<size_t>(n));
    }
    ::close(pipefd[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (total == 0 && (WIFEXITED(status) && WEXITSTATUS(status) != 0))
        return std::unexpected(errCtx(std::string("exec failed: ") + argv[0], WEXITSTATUS(status)));
    return std::move(out);
}

// Strip JSONC comments (// and /* */) while respecting JSON strings.
// Also tolerates trailing commas before }/] — common in hand-written JSONC.
std::string stripJsonComments(std::string_view src) {
    std::string out;
    out.reserve(src.size());
    bool inStr = false, esc = false;
    for (std::size_t i = 0; i < src.size(); ) {
        char c = src[i];
        if (inStr) {
            out.push_back(c);
            if (esc) { esc = false; ++i; continue; }
            if (c == '\\') { esc = true; ++i; continue; }
            if (c == '"') inStr = false;
            ++i; continue;
        }
        if (c == '"') { inStr = true; out.push_back(c); ++i; continue; }
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            i += 2;
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            i = (i + 2 <= src.size()) ? i + 2 : src.size();
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

bool fileReadable(std::string_view path) {
    int fd = ::open(std::string(path).c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ::close(fd);
    return true;
}

#ifdef __ANDROID__
std::string androidProp(const char* key) noexcept {
    char buf[PROP_VALUE_MAX]{};
    int n = __system_property_get(key, buf);
    return std::string(buf, static_cast<size_t>(n > 0 ? n : 0));
}
#else
std::string androidProp(const char*) noexcept { return {}; }
#endif

// Count non-comment virtual lines; trims a trailing comma before }/]
// (removes 'trailing commas' from JSONC safely outside strings).
std::string stripTrailingCommas(std::string_view src) {
    std::string out;
    out.reserve(src.size());
    bool inStr = false, esc = false;
    for (std::size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (inStr) {
            out.push_back(c);
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; out.push_back(c); continue; }
        if (c == ',') {
            std::size_t j = i + 1;
            while (j < src.size() && std::isspace(static_cast<unsigned char>(src[j]))) ++j;
            if (j < src.size() && (src[j] == '}' || src[j] == ']')) continue; // drop comma
        }
        out.push_back(c);
    }
    return out;
}

} // namespace plf
