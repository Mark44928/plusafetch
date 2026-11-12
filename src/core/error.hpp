#pragma once
// std::expected helpers for Plusafetch. All fallible operations use
// std::expected<T, plf::Error> and never throw. Good for reading files,
// running subprocesses, and decoding input. Routines are noexcept.
//
//   auto& os = readTextFile("/etc/os-release").and_then(parsePrettyName);
//   const auto pretty = os.value_or("N/A");
#include <expected>
#include <string>
#include <string_view>

namespace plf {

// Single error type carries context-formatted messages end-to-end.
using Error = std::string;
template <typename T> using Result = std::expected<T, Error>;

[[nodiscard]] inline Error errCtx(std::string_view what, int code) {
    std::string m{what};
    if (code) m += " (code " + std::to_string(code) + ")";
    return m;
}

// Attach context to an existing expected<std::string, Error> chain.
[[nodiscard]] inline Result<std::string> withContext(Result<std::string> r, std::string_view what) {
    if (r) return r;
    return std::unexpected(std::string(what) + ": " + r.error());
}

// Uniform "render" fallback for pretty output / --json value fields.
[[nodiscard]] inline std::string valueOrUnsupported(const Result<std::string>& r) {
    return r ? *r : "N/A";
}

// ---- IO primitives (definitions live in src/detection/platform.cpp) ----
// Bounded read of a text file; maxBytes guards against runaway /proc files.
// Returns the raw file contents (not trimmed).
[[nodiscard]] Result<std::string> readTextFile(std::string_view path, std::size_t maxBytes = 1u << 16);

// Read one line from a text file (newline-stripped). Missing/empty -> error.
[[nodiscard]] Result<std::string> readFirstLine(std::string_view path);

// Run argv through fork+execve, capture combined stdout (bounded to maxBytes).
// No shell is used (args array must be NULL-terminated externally via overload).
// On Android the PATH is searched exactly like posix_spawnp would.
[[nodiscard]] Result<std::string> execCapture(const char* const argv[], std::size_t maxBytes = 1u << 16);

// Monadic helpers kept here for reuse by every detection unit.
[[nodiscard]] inline Result<std::string> trimVal(std::string s) {
    const auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v'; };
    std::size_t a = 0, b = s.size();
    while (a < b && issp(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && issp(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

[[nodiscard]] inline Result<std::string> requireNonEmpty(std::string s) {
    if (s.empty()) return std::unexpected(Error{"empty"});
    return std::move(s);
}

[[nodiscard]] inline Result<std::string> prefixUpper(std::string_view what, Result<std::string> r) {
    if (r) return std::move(*r);
    return std::unexpected(std::string(what) + ": " + r.error());
}

} // namespace plf
