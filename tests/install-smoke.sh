#!/data/data/com.termux/files/usr/bin/env bash
# Installer CI: fake HOME -> install -> rerun -> uninstall.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="${TMPDIR:-$HOME/.cache}/plusafetch-install-smoke.$$"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/home" "$TMP/state" "$TMP/prefix"
export HOME="$TMP/home" XDG_STATE_HOME="$TMP/state"
bash "$ROOT/install.sh" --prefix "$TMP/prefix" --no-deps --no-menu >/dev/null
"$TMP/prefix/bin/plusafetch" --version >/dev/null
test -L "$TMP/prefix/bin/plusfet"
test -f "$TMP/prefix/share/zsh/site-functions/_plusfet"
test -f "$TMP/prefix/share/zsh/site-functions/_plusafetch"
test -f "$TMP/prefix/share/bash-completion/completions/plusafetch"
test -f "$XDG_STATE_HOME/plusafetch/installed.tsv"
bash "$ROOT/install.sh" --prefix "$TMP/prefix" --no-deps --no-menu >/dev/null
bash "$ROOT/install.sh" --uninstall --yes >/dev/null
test ! -e "$TMP/prefix/bin/plusafetch"
test ! -e "$TMP/prefix/bin/plusfet"
test ! -e "$TMP/prefix/share/zsh/site-functions/_plusfet"
test ! -e "$XDG_STATE_HOME/plusafetch/installed.tsv"
echo "installer smoke OK"
