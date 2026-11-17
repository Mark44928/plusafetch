# Plusafetch

A modular **C++23** system fetch tool for **Termux (Android, unrooted)** with a **Linux** fallback.
Fastfetch-style module registry, zero-allocation string buffers, parallel detection, single-write output.
**No logo. Ever.** Text and data only.

> Status: v0.4.0 · text-only · MIT

## Example

![Plusafetch — system fetch on Android 12 (Termux)](docs/screenshot.png)

More terminal captures:

| `--all` | `--doctor` | `--list-themes` |
|---|---|---|
| ![Plusafetch --all](docs/screenshot-all.png) | ![Plusafetch doctor](docs/screenshot-doctor.png) | ![Plusafetch themes](docs/screenshot-themes.png) |

Rendered with [`scripts/ansi2png.py`](scripts/ansi2png.py), a tiny terminal-emulator → PNG renderer (numpy + Pillow) that turns captured ANSI output into a pixel-perfect screenshot — no browser required. Capture with `plusafetch --color > out.ansi`, render with `python3 scripts/ansi2png.py out.ansi screenshot.png`.

## Build

### Termux (Android)

```sh
pkg install clang cmake termux-api   # termux-api optional but recommended
git clone https://github.com/Mark44928/plusafetch.git
cd plusafetch
./install.sh
```

### Linux

```sh
git clone https://github.com/Mark44928/plusafetch.git
cd plusafetch
./install.sh                   # apt/dnf/pacman; installs clang + cmake
```

The installer uses an fzf component menu when fzf is already available (it
never installs fzf), records what it writes, backs up replaced files, and
installs shell completions plus a `plusfet` symlink to `plusafetch`:

```sh
./install.sh --prefix ~/.local      # choose a prefix
./install.sh --no-deps              # never invoke a package manager
./install.sh --no-completions
./install.sh --uninstall            # remove recorded files
```

Or manually:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build -j2
cmake --install build          # installs to <prefix>/bin; honors --prefix=, e.g. $HOME/.local
plusafetch
```

## Usage

| Command | Effect |
| --- | --- |
| `plusafetch` | default layout (fast: syscalls + files only) |
| `plusafetch --all` | adds termux-api modules (Android): battery, sensors, telephony, wifi, clipboard, command, gpu |
| `plusafetch --json` | JSON dump of detected modules (`null` when a module is Unsupported) |
| `plusafetch -w [sec]` / `--watch [sec]` | refresh continuously (default 1s, Ctrl-C to stop; not with `--json`) |
| `plusfet` | short alias — installed as a `plusfet` symlink to `plusafetch` |
| `plusafetch --alert RULE` | alert rule, e.g. `mem>90`, `load>8`, `battery<20`; repeatable |
| `plusafetch --alert-hook 'CMD'` | run `sh -c CMD` when an alert fires (also `alertHook` in config) |
| `plusafetch --alert-cooldown 300` | minimum seconds between hook fires (default 60) |
| `plusafetch --check-alerts` | one-shot check; exit 0 when clear, 1 on a hit (cron-friendly) |
| `plusafetch --format '{os}\n{cpu}\n{memory}'` | template; placeholders `{name}`, slices `{os~0,5}` |
| `plusafetch --modules os,cpu,battery` | comma-separated module list |
| `plusafetch --values-only` | print values only (no key column) |
| `plusafetch --verify-config` | validate config.jsonc (exit 0 OK / 65 invalid) |
| `plusafetch --doctor` | report config validity, theme, color mode, module counts, termux-api and TMPDIR |
| `plusafetch --statusline` | one compact `mem / swap / disk / load` line for tmux, Starship or a shell prompt |
| `plusafetch --report` | save a tiny JSONL sample and print a 30-run trend sparkline (`mem`, `swap`, `disk`, `load`) |
| `plusafetch --report-file FILE` | write the report log to `FILE` instead of the XDG state path |
| `plusafetch --gen-config` | write JSONC config (atomic, see below) |
| `plusafetch --list-modules` | list every registered module |
| `plusafetch --notify` | send a Termux notification when done (Android only) |
| `plusafetch --no-color` | disable colors (auto when `COLORTERM` is absent) |

### Example `--format`

```sh
plusafetch --format 'os={os} cpu={cpu} mem={memory}'
```

### Config (`$HOME/.config/plusafetch/config.jsonc`)

JSONC with comments (stripped before strict parsing). `$XDG_CONFIG_HOME` is honored; `PLUSAFETCH_CONFIG` overrides the path. Config writes use `mkstemp()` in `$TMPDIR` (0600) + `rename()`.

```jsonc
{
  // display order of modules
  "modules": ["title","os","kernel","host","cpu","memory","disk","uptime",
              "packages","shell","terminal","datetime","separator","colors"],
  // per-module key rename: {"name": "cpu", "key": "Processor"} (render + JSON)
  "colors": true,      // master switch
  "truecolor": true,   // 24-bit ANSI (\\033[38;2;R;G;Bm)
  "keyColor": "#78b4ff", // hex RGB (or [r,g,b]) for the key column
  "boldKeys": false,   // bold key labels
  "hideKeys": false,   // values-only mode (like --values-only)
  "separatorLength": 14, // Separator module dash count (0..200)
  //"command": "date", // Command module: sh -c string, first line is printed
  //"commandKey": "Uptime" // label for the Command module
}
```

### Doctor

`plusafetch --doctor` is the quick environment check: it reports whether the
config exists and parses, which theme/key colour will win, the active colour
mode, registered/default/`--all` module counts, termux-api availability, the
detected platform and `TMPDIR`. It exits 65 when the config or theme is invalid.

### Statusline

`plusafetch --statusline` prints one plain line, e.g. `mem 74% · swap 40% · disk 58% · load 3.2`.
It detects only memory/swap/disk/load, so it stays fast enough for a prompt or tmux
`#(plusafetch --statusline)` refresh. It is intentionally incompatible with `--watch`.

### Trend report

`plusafetch --report` appends one compact JSONL sample to the report log (default
`$XDG_STATE_HOME/plusafetch/report.jsonl`, or `~/.local/state/plusafetch/report.jsonl`) and
prints the latest sample as `mem`, `swap`, `disk` and `load` sparklines. The log is capped
at 30 samples. Use `--report-file FILE` for an isolated log.

```sh
plusafetch --report
plusafetch --report --report-file ~/.local/state/plusafetch/report.jsonl
```

The report is plain text, needs no network access, and the JSONL file is safe to graph or
inspect later.

### Alerts

Supported metrics: `mem` (also `memory`, `ram`), `swap`, `disk` — percent of capacity used; `load` — first load average; `battery` (also `batt`) — percent. Operators are `>` and `<`.

```sh
plusafetch --watch --alert mem>85 --alert-hook 'termux-notification -t Plusafetch -c "Memory over 85%"'
plusafetch --check-alerts --alert disk>90
```

Rules fire every `--alert-cooldown` seconds (default 60) while still breached, not just on the first crossing. `--check-alerts` is implied when `--alert` is given without `--watch`, and exits 1 on a hit so cron jobs can react.

```jsonc
{
  // "alerts": ["mem>85", "disk>90", "battery<20", "load>8"],
  // "alertHook": "termux-notification -t Plusafetch -c \"threshold hit\"",
  // "alertCooldown": 300
}
```

The hook is executed via `sh -c`; only use commands you control.

### Ricer skins

Ready-to-copy configs in [`docs/skins/`](docs/skins/) — every one passes `--verify-config`:

| Skin | File | Look |
|---|---|---|
| Greenit | `docs/skins/greenit.jsonc` | phosphor-green bold keys, 44-dash rule |
| Tokyo Night | `docs/skins/tokyonight.jsonc` | blue key column, quiet labels |
| Gruvbox | `docs/skins/gruvbox.jsonc` | amber bold keys, hardware-first order |
| Minimal | `docs/skins/minimal.jsonc` | values only, no key column, no rule |

```sh
cp docs/skins/greenit.jsonc ~/.config/plusafetch/config.jsonc
plusafetch --verify-config && plusafetch
```

All four skins are validated by `make skins` (CI-friendly).

### Colour themes

Built-in presets set the key colour without a config rewrite (inspired by the
fastfetch theme configs and Starship palettes):

```bash
plusafetch --list-themes     # greenit, bluney, catppuccin-mocha, dracula,
                             # gruvbox, nord, rose-pine, solarized, tokyo-night, mono
plusafetch --theme bluney    # keys in #7DBBFF
```

Or keep it in the config:

```jsonc
{ "theme": "gruvbox", "modules": ["os", "cpu", "memory"] }
```

Precedence: `--theme` beats a config `theme`, and an explicit `keyColor` in the
config beats a config `theme` (custom colours always win over presets).

### Shell completions

Bash and zsh completions live in [`completions/`](completions/):

```sh
make completions-install        # installs into $PREFIX/share/{bash-completion,zsh}
```

or source them directly: `source completions/plusafetch.bash`.

## What shows up where

**Core modules (all platforms, no subprocesses):** title, os*, kernel, host*, cpu, memory, disk, uptime, packages, shell, terminal, locale, datetime, separator, colors.

\* Android (Termux): `Os` from `ro.build.version.release` via the NDK property service (`__system_property_get`), `Host/Title` from `ro.product.model` — no subprocess. Linux: `/etc/os-release` and `gethostname()`.

| Module | Source | If unavailable |
| --- | --- | --- |
| OS | `/etc/os-release` (Linux) / NDK props (Android) | `N/A` |
| Kernel | `uname()` | `N/A` |
| CPU | `/proc/cpuinfo` (`model name`/`Hardware`/`Processor` + core count) | `N/A` |
| Memory | `/proc/meminfo` (`MemTotal`/`MemAvailable`) | `N/A` |
| Disk | Android: `statvfs("/data")` · Linux: `statvfs("/")` | `N/A` |
| Uptime | `CLOCK_BOOTTIME` (kernel boot time incl. suspend — works unrooted); falls back to `/proc/uptime` | `N/A` |
| Packages | dpkg `status` paragraphs / `rpm -qa` | `N/A` |
| Shell/Terminal/Locale/DateTime | env / `std::time` | `N/A` |
| Battery | `/sys/class/power_supply/*` (Linux/Android, fast); `termux-battery-status` (Android fallback) | `N/A` |
| Sensors | `termux-sensor -l` → `-s <name> -n 1` exact-name wishlist (accel, light, proximity, gyro) | `N/A` |
| Telephony | `termux-telephony-deviceinfo` | `N/A` |
| WiFi | `termux-wifi-connectioninfo` (surfaces `API_ERROR` if location off) | `N/A` |
| Brightness | Android: **always N/A** — `termux-brightness` is write-only; Linux: best-effort `/sys/class/backlight/*` | `N/A` |
| Clipboard | `termux-clipboard-get` (Android) / `xclip` (Linux) | `N/A` |
| GPU | Linux only: `lspci -d ::0300 -nn` | `N/A` ← printed as N/A on Android |

**Privacy:** we never print `device_id`, `sim_serial_number`, `sim_subscriber_id` (Android restricts these anyway), WiFi BSSID/MAC, or anything else personal. Battery/telephony/wifi values are shown, not logged.

## Performance

Default layout does **zero subprocesses** — everything is a `uname()`/`statvfs()`/bounded file read (plus one `opendir`-free dpkg parse of a few hundred KiB) — and detection fans out over `std::jthread`. On an unrooted MediaTek Helio P22, median wall time from process start to single `write()` is **≈23 ms** (measured 22–30 ms over 30 runs). Independent modules (CPU, Memory, Disk, Battery…) each write only their own `FFContext` slot before the render phase joins.

> The <15 ms aspiration is not fully met including process startup on this device class; the detection phase itself is sub-millisecond. `--all` (Android) spawns long-lived termux-api subprocesses and runs them in parallel — total ≈0.5 s, by design, and the default path never pays for them.

### Measured vs fastfetch

Same device and shell, 20 runs after one warm-up, stdout discarded
(Termux / Android 12, MediaTek MT6762, fastfetch 2.71.0):

| Run | median | min |
|---|---:|---:|
| `plusafetch` (default layout) | **23.5 ms** | 20.0 ms |
| `fastfetch --pipe` (this device's user config) | 716.3 ms | 625.8 ms |
| `fastfetch -c none --pipe` (pure defaults, no user config) | 461.1 ms | 403.6 ms |
| `fastfetch -c none --pipe --logo none --structure title:os:kernel:host:cpu:memory:disk:uptime:packages:shell:terminal:datetime` | 346.4 ms | 277.5 ms |

The last fastfetch row is the fairer comparison: it matches the eleven
modules shared with Plusafetch's default layout and disables the logo. The
pure-defaults row (`-c none`) still runs fastfetch's full 27-row layout, so
the 19× gap there is module count as much as startup cost.
Plusafetch is ~15× faster here because the default path spawns no
subprocesses and has a small fixed module set.

| | Plusafetch | fastfetch |
|---|---|---|
| Language / binary | C++23 · 396 KB | C · 2.0 MB |
| Modules | 24 | 100+ |
| Logo | none, by design | ASCII/image logo |
| Platforms | Termux/Android, Linux, macOS | Linux, macOS, Windows, BSD… |
| Themes | 10 built-in `--theme` presets | theme = config file |
| Default-path subprocesses | none | varies by module |

fastfetch is the feature-complete choice; Plusafetch optimises for a tiny,
subprocess-free Android/Termux default path.

## Constraints met / deliberate exclusions

- C++23, `clang++`/`g++ -std=c++23 -O3 -pthread`, stdlib + POSIX only (no fmt/boost/…).
- `nlohmann/json` vendored in `third_party/nlohmann/` (MIT).
- `std::expected` everywhere; no exceptions in business logic; no `assert()`.
- A–Z registry indexed via `toupper(name[0]) - 'A'`; `FFModuleBaseInfo` carries `name`, `displayName`, `description`, `initOptions`, `destroyOptions`, `parseJsonObject`, `detect`, `moduleValue`, `printModule`, `generateJsonResult`, `generateJsonConfig`, `formatArgs`.
- `FFstrbuf { length; allocated; chars }` — static-literal fast path, power-of-two growth, RAII cleanup.
- **No logo**, no network calls, no telemetry, no config-write without `--gen-config`.

## Build requirements

- Termux: `pkg install clang cmake termux-api` (install script does this)
- Linux: `clang` or `g++` + `cmake` (CI checks both, plus Apple Clang on macOS)


## 🛠️ More from Mark44928

Tiny, dependency-free tools from the same family:

- [Pythonator](https://github.com/Mark44928/Pythonator) — Python rewrite of Plusalator — stdlib-only, `--eval`/`--json`
- [Bluney](https://github.com/Mark44928/Bluney) — blue-injected theme family — dark + light, single palette.json
- [Mark44928](https://github.com/Mark44928) — profile and project index
- [Plusalator](https://github.com/Mark44928/Plusalator) — C++17 calculator REPL — Qalculate-flavored, ghost autosuggest, 18 themes
- [GrubBlastersHTML](https://github.com/Mark44928/GrubBlastersHTML) — Turn-based artillery in one HTML file — 72 weapons, fully destructible terrain
- [SnakTUI](https://github.com/Mark44928/SnakTUI) — Snake game for the terminal — Textual + Python
- [Easy-Termux-Package-Manager](https://github.com/Mark44928/Easy-Termux-Package-Manager) — fzf-driven package manager for Termux — snapshots, audit, 188 tests
- [NoNameOS](https://github.com/Mark44928/NoNameOS) — OS simulation in a single C++17 file — virtual FS, real shell
- [Greenit](https://github.com/Mark44928/Greenit) — green-injected theme family — dark + light, single palette.json
- [CalculaHTML](https://github.com/Mark44928/CalculaHTML) — static HTML calculator — 15 themes + System mode, zero deps
- [android12-fastfetch](https://github.com/Mark44928/android12-fastfetch) — fastfetch config for Android 12 + native X11 Termux desktops
- [Anti-bloatware-list-for-Android-TV-Boxes-and-Sticks-for-rooted](https://github.com/Mark44928/Anti-bloatware-list-for-Android-TV-Boxes-and-Sticks-for-rooted) — ADB debloat for rooted Android 9+ TV sticks & boxes
- [Android9-audios](https://github.com/Mark44928/Android9-audios) — 217 stock Android 9 system sounds + per-file SHA-256

## License

MIT — see [LICENSE](LICENSE). Research notes: [RESEARCH.md](RESEARCH.md).
Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Security reports: [SECURITY.md](SECURITY.md).
