#!/data/data/com.termux/files/usr/bin/env bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/plusafetch"
TMP=$(mktemp -d "${TMPDIR:-/data/data/com.termux/files/usr/tmp}/plusafetch-report.XXXXXX")
trap 'rm -rf -- "$TMP"' EXIT
LOG="$TMP/report.jsonl"
printf '{"ts":1,"mem":10,"temp":-5}\n' > "$LOG"

PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report --report-file "$LOG" >/dev/null
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report --report-file "$LOG" >"$TMP/out.txt"

grep -q 'plusafetch report' "$TMP/out.txt"
python3 - "$LOG" <<'PY'
import json, sys
rows = [json.loads(line) for line in open(sys.argv[1], encoding='utf-8') if line.strip()]
assert len(rows) >= 2, rows
assert all('ts' in row for row in rows)
assert any('mem' in row or 'load' in row for row in rows)
assert any(row.get('temp') == -5 for row in rows), 'negative temperature was dropped'
print(f'report smoke: {len(rows)} samples')
PY

HTML="$TMP/report.html"
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$HTML" --report-file "$LOG" >"$TMP/html-out.txt"
grep -q "html  $HTML" "$TMP/html-out.txt"
grep -q '<title>Plusafetch report</title>' "$HTML"
grep -q 'Generated ' "$HTML"
grep -q 'Plusafetch' "$HTML"
grep -q '<table>' "$HTML"
grep -q -- '-5' "$HTML"
if grep -q -- '-999' "$HTML"; then echo 'html-report: -999 sentinel leaked into page' >&2; exit 1; fi
if grep -Eq '<script|https?://' "$HTML"; then echo 'html-report: external asset or script found' >&2; exit 1; fi

# escaping: a & in the log path must not survive raw into the page
LOG_AMP="$TMP/a&b.jsonl"
printf '{"ts":1750000000,"mem":50,"swap":40,"disk":30,"load":1.5,"battery":80,"temp":36}\n' > "$LOG_AMP"
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$TMP/amp.html" --report-file "$LOG_AMP" >/dev/null
grep -q 'a&amp;b.jsonl' "$TMP/amp.html"
if grep -q 'a&b.jsonl' "$TMP/amp.html"; then echo 'html-report: unescaped path in page' >&2; exit 1; fi

# corrupt + missing JSONL must not crash and must exit 0
printf 'not json\n{"ts":"bad"}\n{"ts":1750000001,"mem":"oops","swap":42}\n' > "$TMP/corrupt.jsonl"
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$TMP/corrupt.html" --report-file "$TMP/corrupt.jsonl" >/dev/null
grep -q 'Plusafetch' "$TMP/corrupt.html"
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$TMP/missing.html" --report-file "$TMP/no-such-file.jsonl" >/dev/null
grep -q 'No samples yet' "$TMP/missing.html"

# cap at 30 samples
python3 - "$TMP/many.jsonl" <<'PY'
import sys
with open(sys.argv[1], 'w', encoding='utf-8') as f:
    for i in range(40):
        f.write('{"ts":%d,"mem":10,"swap":20,"disk":30,"load":1.5,"battery":80,"temp":33}\n' % (1750000000 + i * 60))
PY
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$TMP/many.html" --report-file "$TMP/many.jsonl" >/dev/null
rows=$(grep -o '<td>' "$TMP/many.html" | wc -l | tr -d ' ')
# 7 value cells x 30 sample rows (40 written, capped at 30)
if [ "$rows" -ne 210 ]; then echo "html-report: expected 210 table cells from 30 samples, got $rows" >&2; exit 1; fi

# combine with --report: appends a sample, prints text report and writes HTML
COMBO="$TMP/combo.jsonl"
cp "$LOG" "$COMBO"
PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report --html-report "$TMP/combo.html" --report-file "$COMBO" >"$TMP/combo-out.txt"
grep -q 'plusafetch report' "$TMP/combo-out.txt"
grep -q '^html  ' "$TMP/combo-out.txt"
grep -q 'Generated' "$TMP/combo.html"

# default output path lands under HOME/.local/state (XDG vars cleared)
env HOME="$TMP" XDG_STATE_HOME= PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report --report-file "$LOG" >/dev/null
test -f "$TMP/.local/state/plusafetch/report.html"

# --html-report= with no path is rejected
if PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report= --report-file "$LOG" >/dev/null 2>&1; then
  echo 'html-report: empty --html-report= accepted' >&2; exit 1
fi

# --report-file without --report/--html-report is rejected
if PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --report-file "$LOG" >/dev/null 2>&1; then
  echo 'html-report: bare --report-file accepted' >&2; exit 1
fi

# existing symlink at the output path is refused, target untouched
printf 'orig' > "$TMP/target.html"
ln -s "$TMP/target.html" "$TMP/link.html"
if PLUSAFETCH_CONFIG="$TMP/missing.jsonc" "$BIN" --html-report "$TMP/link.html" --report-file "$LOG" >/dev/null 2>&1; then
  echo 'html-report: symlink output was not refused' >&2; exit 1
fi
test "$(cat "$TMP/target.html")" = 'orig'
echo 'html-report smoke: ok'
