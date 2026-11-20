#!/data/data/com.termux/files/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/plusafetch"

out="$("$BIN" --no-color --modules os,cpu,memory --exclude cpu)"
grep -q '^OS:' <<<"$out"
grep -q '^Memory:' <<<"$out"
if grep -q '^CPU:' <<<"$out"; then
    echo "exclude did not drop CPU:"
    printf '%s\n' "$out"
    exit 1
fi

echo "modules smoke OK"