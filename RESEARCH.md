# Plusafetch — Research Notes

Research phase for Plusafetch, a modular C++23 system fetch tool for Termux/Android with a Linux fallback.
All claims are cited; anything not directly verifiable is marked **UNVERIFIED** and handled with the safest fallback.

Date: 2026-11-12 · Target: Android 12 via Termux (unrooted), standard Linux (fallback) · Helio P22

## 1. fastfetch module registry architecture

Sources:

- DeepWiki, *Module System Architecture*: https://deepwiki.com/fastfetch-cli/fastfetch/2.3-module-system-architecture
- fastfetch `CMakeLists.txt`/`.github/workflows`: n/a (not needed)
- Module registry source: https://github.com/fastfetch-cli/fastfetch/blob/a720e1f9/src/modules/modules.c
- `FFModuleBaseInfo` source: https://github.com/fastfetch-cli/fastfetch/blob/639252ec/src/common/option.h

Extracted facts:

- Registry `ffModuleInfos` is a 27-element array (A–Z plus one for non-alphabetic), each element points to a `NULL`-terminated array of `FFModuleBaseInfo*` for modules starting with that letter. Command-line/config lookup uses `toupper(key) - 'A'`.
- `FFModuleBaseInfo` fields (from `src/common/option.h` 46-63 and DeepWiki): `name`, `description`, `displayName`, `initOptions`, `destroyOptions`, `parseJsonObject`, `printModule`, `generateJsonResult`, `generateJsonConfig`, `formatArgs` (plus `defaultOrder`).
- Module layer owns UI (JSON options, format string, key/icon, console output, JSON result); the **detection layer is separate** (`src/detection/`), fills a plain result struct, returns `const char*` — `nullptr` on success, error string otherwise ("Not supported on this platform"). Modules without OS dependency (Title, Separator, DateTime, Custom, Break) have no detection layer.
- `printModule` returns bool; `generateJsonResult` returning NULL means Unsupported in JSON format.
- Every module name match is case-insensitive.

Adaptation for Plusafetch (C++23): we keep the same registry concept and the same struct field names (in C++ spelling). The detection vs module split is preserved via `detection/` + `FFContext`.

**UNVERIFIED / adapted:**

- `FFModuleArgs` (keys, icons, key widths) and `alignas(uint64_t) optionBuf[FF_OPTION_MAX_SIZE]` ("C polymorphism") are fastfetch-specific and are intentionally replaced with a plain per-module C++ options struct. No behavior is promised beyond: name, description, formatArgs.

## 2. fastfetch FFstrbuf zero-allocation string

Sources:

- DeepWiki, *FFstrbuf Dynamic String System*: https://deepwiki.com/fastfetch-cli/fastfetch/5.1-ffstrbuf-dynamic-string-system
- fastfetch `src/common/FFstrbuf.h`: https://github.com/fastfetch-cli/fastfetch/blob/639252ec/src/common/FFstrbuf.h

Extracted facts:

- Struct layout (upstream order in the current docs): `{ uint32_t allocated; uint32_t length; char* chars; }`. The task spec asks for `{ uint32_t length; uint32_t allocated; char* chars; }` — **field names and semantics are identical; order is kept as specified in the task** (documented in `src/core/ffstrbuf.hpp`).
- `allocated == 0` means *static mode*: `chars` points at a literal / non-owned memory, nothing is freed on destroy. `allocated > 0` means *dynamic mode*: heap-owned.
- Fresh buffer: `chars = CHAR_NULL_PTR` (a global `char* CHAR_NULL_PTR = ""`), `length == 0`. `ffStrbufGetFree` = `allocated - length - 1` (0 when static). Growth: candidate = `length + free`; if < 32, use 32; otherwise round up to power of two via `__builtin_clz` (or double from 32). Static→dynamic copies the existing content into a new heap buffer; dynamic→dynamic uses `realloc`.
- A `FASTFETCH_STRBUF_DEFAULT_ALLOC` of 32 exists; tests use it (`ffStrbufInitA(&strbuf, 32)`).

Adaptation for Plusafetch: `FFStrBuf` RAII C++ wrapper with static mode for literals, power-of-two growth, move-only, `clear()`/`destroy()`. No exceptions. Used for the single frame buffer.

## 3. Termux API data sources (Android only)

### 3.1 Battery — `termux-battery-status`

Sources:

- Termux Wiki: https://wiki.termux.com/wiki/Termux-battery-status
- Example output + field table: https://hugmouse.github.io/gotermux-docs/0.1/termux-api/termux-battery-status/
- termux-api issue #763 (temperature float format changed): https://github.com/termux/termux-api/issues/763

Confirmed JSON keys (stable): `present` (bool), `technology`, `health` (GOOD/COLD/DEAD/OVERHEAT/...), `plugged` (UNPLUGGED/PLUGGED_AC/PLUGGED_USB/...), `status` (CHARGING/DISCHARGING/FULL/NOT_CHARGING), `temperature` (**string or number, changed across termux-api v50→v51; in v51 it may be a comma-decimal string like "27,4" in some locales** — parser must tolerate both), `percentage` (int), `voltage` (int mV), `energy`, `cycle` (optional, Android 14+).

**Design decision:** on Android the module reads `/sys/class/power_supply/battery/*` first (same as fastfetch — zero subprocesses, ~1 ms); `termux-battery-status` via `popen`/fork+exec is the fallback only when sysfs is unreadable.

**UNVERIFIED:** `/sys/class/power_supply/battery/` layout varies per device (some devices expose `capacity`, `status`, `health`, `technology`; some expose `voltage_now` in µV). Fallback gives N/A if missing.

### 3.2 Telephony — `termux-telephony-deviceinfo`

Sources:

- termux-api `TelephonyAPI.java` (official field names): https://github.com/termux/termux-api/blob/master/app/src/main/java/com/termux/api/apis/TelephonyAPI.java
- @sigma/termux TypeScript declarations: https://jsr.io/@sigma/termux/doc/all_symbols

Confirmed JSON keys: `data_enabled` (string "true"/"false"), `data_activity`, `data_state`, `device_id` (nullable, Android 10+ restricted), `phone_type` (cdma/gsm/none/sip), `phone_count`, `network_operator`, **`network_operator_name`**, `network_country_iso`, `network_type` (lte/nr/hspa/...), `network_roaming` (bool), `sim_state`, `sim_operator_name`.

Permissions: `READ_PHONE_STATE` required; missing permission → `device_id`/`sim_*` come back null/empty or the call fails. Module prints N/A per field.

### 3.3 WiFi — `termux-wifi-connectioninfo`

Sources:

- termux-api issue #304 (example output): https://github.com/termux/termux-api/issues/304
- Termux Wiki: https://wiki.termux.com/wiki/Termux-wifi-connectioninfo

Confirmed JSON keys: `ssid`, `bssid`, `link_speed_mbps`, `frequency_mhz`, `ip`, `mac_address`, `network_id`, `rssi`, `ssid_hidden`, `supplicant_state`, `API_ERROR` (present when permission/location is off — module must surface `API_ERROR` as N/A).

Requires Location permission and location enabled, otherwise `ssid` comes back `<unknown ssid>` or `API_ERROR` appears.

### 3.4 Sensors — `termux-sensor`

Sources:

- termux-api `SensorAPI.java`: https://github.com/termux/termux-api/blob/97b41002/app/src/main/java/com/termux/api/apis/SensorAPI.java
- termux-api issue #469 (example single read): https://github.com/termux/termux-api/issues/469

Confirmed shapes:

- List: `termux-sensor -l` → `{"sensors": ["<name>", ...]}`.
- Single read: `termux-sensor -s "<name>" -n 1` → `{"<name>": {"values": [v1, ...]}}` e.g. `{"TMD4906 lux Sensor": {"values": [33]}}`.

**Match semantics:** `-s "<name>"` requires the exact sensor name from `-l` (shown case-insensitively in docs; exact match recommended). Module queries a fixed wishlist by exact name if present in `-l`: accelerometer (`Accelerometer`), light (`Light sensor`), proximity (`Proximity sensor`), gyroscope (`Gyroscope`).

### 3.5 Brightness — `termux-brightness`

Source:

- Termux-api issue #1414 (set-only, root required to read): https://github.com/termux/termux-app/issues/1414

**UNVERIFIED as a *getter*: it isn't one.** `termux-brightness` *sets* the screen brightness (body `brightness: int`, 0-255 clamped) and cannot read it without root. On unrooted Android there is no legitimate Termux-API read path. Linux fallback reads nothing either (brightness is GPU/panel-specific). Plusafetch's Brightness module therefore prints `N/A` with this documented reason — the honest fallback, per the task's "If a /sys path is unreadable or termux-api is missing, print N/A."

### 3.6 Termux filesystems / environment

Sources:

- Termux wiki mirror, *Differences from Linux*: https://termux-wiki.vercel.app/differences-from-linux
- Termux internals guide: https://ggprompts.com/techguides/termux.html

Confirmed: every installed package lives under `$PREFIX = /data/data/com.termux/files/usr` (reads like `/usr`): binaries in `$PREFIX/bin`, configs `$PREFIX/etc`, no FHS to satisfy. `$HOME = /data/data/com.termux/files/home`. `$TMPDIR = $PREFIX/tmp` (atomic config writes must use `$TMPDIR`, which exists and is writable). `/tmp` does not exist / is not writable.

## 4. C++23 `std::expected` for fallible operations

Sources:

- C++ Stories — *std::expected - Monadic Extensions*: https://www.cppstories.com/2024/expected-cpp23-monadic/
- bigcpp.com reference — *Monadic Operations*: https://bigcpp.com/reference/idioms/monadic-operations
- javathecode.com — *std::expected vs Exceptions for CLI Tools*: https://www.javathecode.com/2026/08/stdexpected-vs-exceptions-for-cli-tools.html
- Michael Eiler — *C++23: A std::expected-ly Graceful Release*: https://blog.eiler.eu/posts/20230501/

Extracted facts:

- Monadic ops: `and_then` (value→expected same E, short-circuits on error), `transform` (value→plain U, wraps), `or_else` (error handler, `expected<T,F>` returned), `transform_error` (error remap). Every step in a chain must return `expected<_, E>` with the same `E`; otherwise static-assert failure. Convert error types at subsystem boundaries (`transform_error`).
- Best practice for CLI: routine, expected failures (missing file, bad input, missing binary) → `expected<T, E>`, never throw. Programming bugs → exceptions may propagate up to `_exit`-style guard; destructors must not throw. All callbacks should be `noexcept`.
- Never call `value()` unchecked; use `value_or` or check `has_value()`.

Adaptation for Plusafetch: one error type, `using Error = std::string` (message already carries context), `and_then`/`transform`/`or_else` chains in the detection layer; module code uses `.value_or("N/A")`. Never throw in business logic; `main` catches everything and prints to stderr with a nonzero exit (`std::set_terminate` not required — handler only for SIGINT/SIGTERM).

## 5. Explicit non-goals & honesty register

1. **Uptime alternative (verified):** `/proc/uptime` and `/proc/stat` return SELinux "Permission denied" unrooted here, but `CLOCK_BOOTTIME` — kernel boot time incl. suspend, equivalent of `SystemClock.elapsedRealtime()` — returns real elapsed seconds unrooted. Used as the primary uptime source; `/proc/uptime` kept as fallback.
1. **< 15 ms on Helio P22 is not achievable if Termux-API subprocesses (termux-battery-status etc.) are on the critical path** — each forks Java through Termux:API's socket, 100–400 ms each. Plusafetch reads sysfs first (fastfetch-style, ~1–3 ms) and only falls back to termux-api subprocesses. Default layout contains **no** subprocess; sensors/telephony/wifi/clipboard/command are behind flags. Measured wall time on this device may exceed 15 ms for `--all`; that is reported, not hidden.
2. **Brightness read: impossible unrooted** (see 3.5) → `N/A`, honestly.
3. **`device_id`/`sim_serial_number`/`sim_subscriber_id`:** always N/A on modern unrooted Android (restricted); we never print their values anyway (privacy).
4. **JSONC__ config** (`$HOME/.config/plusafetch/config.jsonc`): comments stripped by a small RFC-aware pass (strings respected), then strict nlohmann/json. `$TMPDIR/plusafetch_config.tmp` → `rename()` atomic write.
5. **Lua in format strings:** `FF_HAVE_LUA` is *not* defined; `--format` supports `{placeholder}` + slicing `{~start,length}` only. Lua support is out of scope (space/sandboxing reasons) — flagged UNVERIFIED and not shipped.
6. **`--logo`:** does not exist, by design (text-only tool).

## 6. Performance design

- Single-write `write(STDOUT_FILENO, buf, len)` for the whole frame.
- Parallel detection: `std::jthread` fan-out for independent modules (CPU, Memory, Disk, Battery, Sensors, Telephony, WiFi) writing disjoint context fields; join before rendering.
- FFStrBuf zero-allocation static literals on hot paths; power-of-two growth.
- No syscalls except the module reads themselves on the default path; no network I/O (no public IP lookup).
