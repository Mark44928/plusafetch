#!/data/data/com.termux/files/usr/bin/env bash
# Install Plusafetch on Termux (Android) or Linux (fallback).
set -euo pipefail

info() { printf '\033[38;2;120;200;255m[plusafetch]\033[0m %s\n' "$*"; }
warn() { printf '\033[38;2;230;150;60m[plusafetch]\033[0m %s\n' "$*" >&2; }
die()  { warn "$*"; exit 1; }

if [ -n "${TERMUX_VERSION:-}" ] || [ "${PREFIX:-}" = "/data/data/com.termux/files/usr" ]; then
  info "Termux detected — installing clang, cmake and termux-api…"
  pkg update
  pkg install -y clang cmake termux-api || die "pkg install failed; check network or run 'pkg update' first"
else
  info "Linux detected — installing clang and cmake…"
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update && sudo apt-get install -y clang cmake || warn "apt install failed — ensure clang + cmake are installed manually"
  elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install clang cmake || warn "dnf install failed"
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm clang cmake || warn "pacman -S failed"
  else
    warn "Unknown package manager — install clang and cmake manually"
  fi
fi

info "Configuring…"
cmake -B build -DCMAKE_BUILD_TYPE=Release .
info "Building…"
cmake --build build -j2
info "Installing…"
if [ -n "${TERMUX_VERSION:-}" ] || [ "${PREFIX:-}" = "/data/data/com.termux/files/usr" ]; then
  cmake --install build
elif [ "$(id -u)" = "0" ]; then
  cmake --install build
elif command -v sudo >/dev/null 2>&1; then
  sudo cmake --install build
else
  info "Installing to $HOME/.local/bin (add it to PATH)"
  cmake --install build --prefix "$HOME/.local"
fi
info "Done. Run: plusafetch --help"
