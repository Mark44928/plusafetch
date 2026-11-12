# Contributing to Plusafetch

## Ground rules

1. **Termux-first.** Plusafetch targets unrooted Android via Termux first; Linux is the fallback path. Do not add unrooted-Android-unsafe reads behind `#ifdef` guesses — confirm or print N/A.
2. **Honest degradation.** If a read can fail (/sys denied, termux-api missing, invalid JSON), print N/A, never crash, never invent values.
3. **Hot-path discipline.** No subprocess on the default render path. Subprocesses belong behind `--all`/config.
4. **No exceptions** in module code; every fallible call returns `std::expected<T, plf::Error>`. No `assert()`.
5. **No logo, no art, no network.**

## Adding a module

1. Create `src/modules/<name>.cpp` with its `extern const FFModuleBaseInfo k<Name>ModuleInfo`.
2. Detection code goes to `src/detection/` (shared) or `platform_android.cpp` / `platform_linux.cpp`.
3. Register in `src/registry.cpp` (A–Z slot under the right letter).
4. `--list-modules` must show it; `--json` must emit its `displayName` key (or `null`).
5. Keep render output single-line, key padded organically, values <120 chars.

## Style

- Dense, comment-light code; comments only why, not what.
- Verify on Termux if possible; report RAM/CPU device class in your PR.
- `./build/plusafetch`, `--json`, `--format`, `--list-modules` must all stay green locally.
