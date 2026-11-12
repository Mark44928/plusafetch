#pragma once
// FFStrBuf: zero-allocation string buffer in the spirit of fastfetch's FFstrbuf
// Spec: struct FFstrbuf { uint32_t length; uint32_t allocated; char* chars; };
//   allocated == 0 -> static mode (chars points at a literal / non-owned memory),
//   allocated  > 0 -> dynamic, heap-owned buffer.
// Field ORDER is as asked in the task spec (fastfetch stores allocated first).
// Static mode first turns all hot-path literals into zero-allocation pointers.
// Append operations that need to grow transparently promote static -> dynamic.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstddef>
#include <unistd.h>

namespace plf {

inline constexpr uint32_t kStrbufDefaultAlloc = 32; // matches FASTFETCH_STRBUF_DEFAULT_ALLOC

struct FFstrbuf {
    uint32_t length    = 0;
    uint32_t allocated = 0; // 0 => static mode, >0 => heap-owned bytes capacity (incl. terminator)
    char*    chars     = const_cast<char*>(""); // CHAR_NULL_PTR equivalent
};

[[nodiscard]] inline FFstrbuf ffStrbufInit() noexcept { return {}; }

inline void ffStrbufInitStatic(FFstrbuf& b, const char* s) noexcept {
    const char* str = s ? s : "";
    b = { static_cast<uint32_t>(std::strlen(str)), 0, const_cast<char*>(str) };
}

[[nodiscard]] inline uint32_t ffStrbufGetFree(const FFstrbuf& b) noexcept {
    if (b.allocated == 0) return 0;
    return b.allocated - b.length - 1; // keep one byte for terminator
}

inline void ffStrbufClear(FFstrbuf& b) noexcept {
    if (b.allocated == 0) { b = FFstrbuf{}; return; }
    b.chars[0] = '\0';
    b.length = 0;
}

inline void ffStrbufDestroy(FFstrbuf& b) noexcept {
    if (b.allocated != 0) std::free(b.chars);
    b = FFstrbuf{};
}

[[nodiscard]] inline bool ffStrbufEnsureFree(FFstrbuf& b, uint32_t freeBytes) noexcept {
    if (b.allocated != 0 && ffStrbufGetFree(b) >= freeBytes) return true;
    uint32_t cap = b.allocated ? b.allocated : kStrbufDefaultAlloc;
    while (static_cast<uint64_t>(b.length) + freeBytes + 1 > cap) {
        cap = (cap < kStrbufDefaultAlloc) ? kStrbufDefaultAlloc : cap * 2; // doubling, floor 32
    }
    char* nb = static_cast<char*>(std::malloc(cap));
    if (!nb) return false; // OOM: caller drops the append rather than overflow
    std::memcpy(nb, b.chars, static_cast<size_t>(b.length) + 1);
    if (b.allocated != 0) std::free(b.chars);
    b.chars = nb;
    b.allocated = cap;
    return true;
}

inline void ffStrbufAppendNS(FFstrbuf& b, const char* s, uint32_t n) noexcept {
    if (s == nullptr || n == 0) return;
    if (!ffStrbufEnsureFree(b, n)) return; // OOM: drop bytes, never corrupt heap
    std::memcpy(b.chars + b.length, s, n);
    b.length += n;
    b.chars[b.length] = '\0';
}

inline void ffStrbufAppendS(FFstrbuf& b, const char* s) noexcept {
    if (s == nullptr) return;
    ffStrbufAppendNS(b, s, static_cast<uint32_t>(std::strlen(s)));
}

inline void ffStrbufAppendC(FFstrbuf& b, char c) noexcept { ffStrbufAppendNS(b, &c, 1); }

inline void ffStrbufAppendF(FFstrbuf& b, const char* fmt, ...) noexcept {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) ffStrbufAppendNS(b, tmp, static_cast<uint32_t>(n) < sizeof tmp ? static_cast<uint32_t>(n) : static_cast<uint32_t>(sizeof tmp - 1));
}

[[nodiscard]] inline uint32_t ffStrbufWriteToFd(const FFstrbuf& b, int fd) noexcept {
    if (b.length == 0) return 0;
    ssize_t w = ::write(fd, b.chars, b.length);
    return w < 0 ? 0 : static_cast<uint32_t>(w);
}

// RAII convenience for std::string-like call site, exception-free.
struct FFStrBufGuard {
    FFstrbuf b;
    FFStrBufGuard() : b(ffStrbufInit()) {}
    ~FFStrBufGuard() { ffStrbufDestroy(b); }
    FFStrBufGuard(const FFStrBufGuard&) = delete;
    FFStrBufGuard& operator=(const FFStrBufGuard&) = delete;
};

} // namespace plf
