# Changelog

## [0.5.0] - 2026-11-17

- Extended `--report` with battery percentage and battery-temperature samples when the Android battery source exposes them.
- Documented the new report metrics and kept missing values out of the JSONL log instead of fabricating zeros.

## [0.4.0] - 2026-11-17

- Added `--report` to save up to 30 `mem`/`swap`/`disk`/`load` samples as JSONL and print a plain-text trend sparkline with deltas.
- Added `--report-file FILE` for isolated report logs and a `tests/report-smoke.sh` gate for output and JSON validation.
- Documented the trend report in README, shell completions and `--help`; bumped the public version to 0.4.0.

## [0.1.0] - 2026-11-12

Initial release plus on-device additions (no version bump — same public tag):
- `--verify-config`: validate `config.jsonc` (exit 0 if OK/missing, 65 if invalid; suspicious types reported to stderr as warnings).
- Per-glyph font fallback chain in `scripts/ansi2png.py` (CaskaydiaCove NF → DejaVu Sans Mono) — re-rendered example screenshot.
- Cross-family README links and release `v0.1.0` published. of Plusafetch (C++23, fastfetch-style A–Z module registry, zero-allocation FFstrbuf, std::expected, jthread parallel detection, single-write output). Everything below shipped in this one release:

- fastfetch-style A–Z module registry (`FFModuleBaseInfo`: `name`, `displayName`, `description`, `initOptions`, `destroyOptions`, `parseJsonObject`, `detect`, `moduleValue`, `printModule`, `generateJsonResult`, `generateJsonConfig`, `formatArgs`).
- 23 modules: title, os, kernel, host, gpu, cpu, memory, disk, uptime, packages, shell, terminal, locale, datetime, separator, colors, battery, brightness, sensors, telephony, wifi, clipboard, command.
- Parallel detection via `std::jthread`; single `write()` frame; zero subprocesses on the default path.
- Android/Termux: OS/host via NDK `__system_property_get` (no subprocess); Linux fallback: `/etc/os-release`, sysfs battery, `lspci` GPU.
- `--json`, `--format FMT` (placeholders `{os}`, slicing `{os~0,5}`), `--modules`, `--all`, `--list-modules`, `--notify`, `--gen-config`, `--values-only`, `--no-color`/`--color`, truecolor detection.
- JSONC config at `$HOME/.config/plusafetch/config.jsonc` (comments stripped). Atomic writes via `$TMPDIR` (`mkstemp`, 0600) + `rename()`.
- Customization: per-module key renames (`{"name":"cpu","key":"Processor"}`), `keyColor` (hex/`[r,g,b]`), `boldKeys`, `hideKeys`/`--values-only`, `separatorLength`.
- Uptime on unrooted Android via `CLOCK_BOOTTIME` (kernel boot time incl. suspend); `/proc/uptime` as fallback.
- Disk reports `/data` (Android user storage) instead of the read-only system partition; Linux keeps `/`.
- GitHub Actions CI: ubuntu Clang, ubuntu GCC, macOS Apple Clang matrix + clang-tidy (all green).

### Known limitations (honest)
- `<15 ms` target not fully met including process startup on Helio P22 (≈23 ms measured; detection itself sub-ms).
- Brightness always `N/A` on Android (`termux-api` is write-only).
- `--all` spawns termux-api subprocesses (~0.5 s) — never on the default path.
