#!/data/data/com.termux/files/usr/bin/env bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/plusafetch"
TMP=$(mktemp -d "${TMPDIR:-/data/data/com.termux/files/usr/tmp}/plusafetch-report.XXXXXX")
trap 'rm -rf -- "$TMP"' EXIT
LOG="$TMP/report.jsonl"

PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report --report-file "$LOG" >/dev/null
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report --report-file "$LOG" >"$TMP/out.txt"

grep -q 'plusafetch report' "$TMP/out.txt"
python3 - "$LOG" <<'PY'
import json, sys
rows = [json.loads(line) for line in open(sys.argv[1], encoding='utf-8') if line.strip()]
assert len(rows) >= 2, rows
assert all('ts' in row for row in rows)
assert any('mem' in row or 'load' in row for row in rows)
print(f'report smoke: {len(rows)} samples')
PY
