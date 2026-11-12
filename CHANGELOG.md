# Changelog

## [0.1.1] - 2026-11-12

### Fixed
- Disk module on Android now reports the real user-storage partition (`/data`, e.g. 50 GB) instead of the tiny read-only root system partition.

## [0.1.0] - 2026-11-12

### Added
- Initial release: fastfetch-style A–Z module registry (`FFModuleBaseInfo`: `name`, `displayName`, `description`, `initOptions`, `destroyOptions`, `parseJsonObject`, `detect`, `moduleValue`, `printModule`, `generateJsonResult`, `generateJsonConfig`, `formatArgs`).
- 23 modules: title, os, kernel, host, gpu, cpu, memory, disk, uptime, packages, shell, terminal, locale, datetime, separator, colors, battery, brightness, sensors, telephony, wifi, clipboard, command.
- Parallel detection via `std::jthread`; single `write()` frame; zero subprocesses on the default path.
- Android/Termux: OS/host via NDK `__system_property_get` (no subprocess); Linux fallback: `/etc/os-release`, sysfs battery, `lspci` GPU.
- `--json`, `--format FMT` (placeholders `{os}`, slicing `{os~0,5}`), `--modules`, `--all`, `--list-modules`, `--notify`, `--gen-config`, `--no-color`/`--color`, truecolor detection.
- JSONC config at `$HOME/.config/plusafetch/config.jsonc` (comments stripped); atomic config writes via `$TMPDIR` + `rename()`.
- GitHub Actions CI: ubuntu Clang, ubuntu GCC, macOS Apple Clang matrix + clang-tidy (all green).

### Known limitations (honest)
- `<15 ms` target not fully met including process startup on Helio P22 (≈23 ms measured; detection itself sub-ms).
- Uptime `N/A` on unrooted Android (SELinux denies `/proc/uptime`).
- Brightness always `N/A` on Android (`termux-api` is write-only).
- `--all` spawns termux-api subprocesses (~0.5 s) — never on the default path.
