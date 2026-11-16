#!/data/data/com.termux/files/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/plusafetch"
mkdir -p "${TMPDIR:-$HOME/.cache}"
TMP="$(mktemp -d "${TMPDIR:-$HOME/.cache}/plusafetch-alerts.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

set +e
"$BIN" --check-alerts --alert 'mem>0' >"$TMP/hit.out"
hit=$?
"$BIN" --check-alerts --alert 'mem<0' >"$TMP/clear.out"
clear=$?
"$BIN" --check-alerts --alert bogus >/dev/null 2>&1
bad=$?
set -e

[ "$hit" -eq 1 ] || { echo "expected hit rc=1, got $hit"; cat "$TMP/hit.out"; exit 1; }
[ "$clear" -eq 0 ] || { echo "expected clear rc=0, got $clear"; exit 1; }
[ "$bad" -eq 64 ] || { echo "expected bad-rule rc=64, got $bad"; exit 1; }

cat >"$TMP/config.jsonc" <<EOF
{ "modules": ["memory"], "alerts": ["mem>0"], "alertHook": "touch ${TMP}/hook" }
EOF
set +e
PLUSAFETCH_CONFIG="$TMP/config.jsonc" "$BIN" --check-alerts >/dev/null
cfg=$?
set -e
[ "$cfg" -eq 1 ] || { echo "expected config-alert rc=1, got $cfg"; exit 1; }
[ -f "$TMP/hook" ] || { echo "hook did not run"; exit 1; }
echo "alerts smoke OK"
