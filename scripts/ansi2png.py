#!/usr/bin/env python3
"""ansi2png — render ANSI-colored terminal output into a PNG, pixel-exact.

Reads raw bytes from a file/stdin (typically captured with
`plusafetch --color > out.ansi`), parses SGR escapes, and draws
a fixed-grid bitmap. Pixel math is numpy; drawing is PIL.

Usage:
  capture:  plusafetch --color > "$TMPDIR/plus.ansi"
  render:   python3 ansi2png.py "$TMPDIR/plus.ansi" docs/screenshot.png \
              --font-file "$HOME/.fonts/CascadiaCodePL-Regular.otf" --size 18
  stdin ok: … | python3 ansi2png.py - out.png
"""
import argparse
import re
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

# ---------- xterm 256-color table ----------
BASIC = [
    (0x00, 0x00, 0x00), (0xCD, 0x00, 0x00), (0x00, 0xCD, 0x00), (0xCD, 0xCD, 0x00),
    (0x00, 0x00, 0xEE), (0xCD, 0x00, 0xCD), (0x00, 0xCD, 0xCD), (0xE5, 0xE5, 0xE5),
    (0x7F, 0x7F, 0x7F), (0xFF, 0x00, 0x00), (0x00, 0xFF, 0x00), (0xFF, 0xFF, 0x00),
    (0x5C, 0x5C, 0xFF), (0xFF, 0x00, 0xFF), (0x00, 0xFF, 0xFF), (0xFF, 0xFF, 0xFF),
]
CUBE = [0, 95, 135, 175, 215, 255]
def xterm256(idx):
    if idx < 16:
        return BASIC[idx]
    if idx < 232:
        n = idx - 16
        return (CUBE[n // 36], CUBE[(n % 36) // 6], CUBE[n % 6])
    v = 8 + (idx - 232) * 10
    return (v, v, v)

def parse_sgr(params):
    """Yield state deltas from an SGR parameter list."""
    state = {"fg": None, "bg": None, "bold": False, "dim": False, "reset": False}
    i = 0
    nums = []
    if params.strip() == "":
        nums = [0]
    else:
        for tok in params.split(";"):
            if tok.isdigit():
                nums.append(int(tok))
            else:
                nums.append(0)
    while i < len(nums):
        n = nums[i]
        if n == 0:
            state = {"fg": None, "bg": None, "bold": False, "dim": False, "reset": True}
        elif n == 1:
            state["bold"] = True
        elif n == 2:
            state["dim"] = True
        elif n == 22:
            state["bold"] = False; state["dim"] = False
        elif 30 <= n <= 37:
            state["fg"] = (0, xterm256(n - 30))
        elif n == 39:
            state["fg"] = None
        elif 40 <= n <= 47:
            state["bg"] = (0, xterm256(n - 40))
        elif n == 49:
            state["bg"] = None
        elif 90 <= n <= 97:
            state["fg"] = (0, xterm256(n - 90 + 8))
        elif 100 <= n <= 107:
            state["bg"] = (0, xterm256(n - 100 + 8))
        elif n == 38 or n == 48:
            adv = 0
            mode = nums[i + 1] if i + 1 < len(nums) else -1
            if mode == 5 and i + 2 < len(nums):
                color = (0, xterm256(nums[i + 2]))
                adv = 2
            elif mode == 2 and i + 4 < len(nums):
                color = (1, (nums[i + 2] & 255, nums[i + 3] & 255, nums[i + 4] & 255))
                adv = 4
            else:
                color = None
            if color:
                if n == 38:
                    state["fg"] = color
                else:
                    state["bg"] = color
            i += adv
        i += 1
    return state

_OSC = re.compile(rb"\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)")
_CSI = re.compile(rb"\x1b\[([0-9;?]*)([A-Za-z@`])")
_ESC = re.compile(rb"\x1b[()][AB0]|\x1b=|\x1b>|\x1b\]([0-9;?]*)([A-Za-z@`])")

# ---------- minimal terminal emulator ----------
# Handles exactly the sequences raw-mode TTYs emit: CR/LF/BS, CUP/HVP/CHA/VPA,
# CUU/CUD/CUF/CUB, ED, EL, DECSTBM scroll region, DEC private mode 1049 (alt
# buffer), SGR (same parser as before), ESC M/D (reverse/index), save/restore.
_CSI = re.compile(rb"\x1b\[([0-9;?]*)([A-Za-z@`])")
_OSC = re.compile(rb"\x1b\][^\x07\x1b]*(?:\x07|\x1b\\)")

class Screen:
    def __init__(self, rows=34, cols=100):
        self.rows, self.cols = rows, cols
        self.reset_all()

    def blank(self):
        return [[(" ", None, None, False, False) for _ in range(self.cols)] for _ in range(self.rows)]

    def reset_all(self):
        self.grid = self.blank()
        self.saved = self.blank()
        self.alt = False
        self.r0, self.r1 = 0, self.rows - 1
        self.cr, self.cc, self.fr, self.fc = 0, 0, 0, 0
        self.fg = self.bg = None
        self.bold = self.dim = False

    def enter_alt(self):
        if not self.alt:
            self.saved = [row[:] for row in self.grid]
            self.grid = self.blank()
            self.alt = True
            self.cr, self.cc = 0, 0

    def leave_alt(self):
        if self.alt:
            self.grid = [row[:] for row in self.saved]
            self.alt = False
            self.cr, self.cc = 0, 0

    def set_sgr(self, payload):
        st = parse_sgr(payload)
        if st.get("reset"):
            self.fg = self.bg = None; self.bold = self.dim = False
        if st["fg"] is not None: self.fg = st["fg"]
        if st["bg"] is not None: self.bg = st["bg"]
        if "bold" in st: self.bold = st["bold"]
        if "dim" in st: self.dim = st["dim"]

    def scroll_up(self, n=1):
        for _ in range(n):
            del self.grid[self.r0]
            self.grid.insert(self.r1, [(" ", None, None, False, False) for _ in range(self.cols)])

    def scroll_down(self, n=1):
        for _ in range(n):
            del self.grid[self.r1]
            self.grid.insert(self.r0, [(" ", None, None, False, False) for _ in range(self.cols)])

    def cursor_down(self, n=1):
        while n > 0:
            if self.cr >= self.r1:
                self.scroll_up(1)
            else:
                self.cr += 1
            n -= 1

    def cursor_up(self, n=1):
        self.cr = max(self.r0, self.cr - n)

    def put_char(self, ch):
        if self.cc >= self.cols:
            self.cc = 0
            self.cursor_down(1)
        self.grid[self.cr][self.cc] = (ch, self.fg, self.bg, self.bold, self.dim)
        self.cc += 1

    def erase_line(self, mode):
        row = self.grid[self.cr]
        if mode == 0: span = range(self.cc, self.cols)
        elif mode == 1: span = range(0, self.cc + 1)
        else: span = range(self.cols)
        for i in span:
            row[i] = (" ", None, None, False, False)

    def erase_screen(self, mode):
        if mode == 2:
            self.grid = self.blank(); self.cr = self.cc = 0; return
        if mode == 0:
            self.erase_line(0)
            for r in range(self.cr + 1, self.rows):
                self.grid[r] = [(" ", None, None, False, False) for _ in range(self.cols)]
            return
        if mode == 1:
            for r in range(0, self.cr):
                self.grid[r] = [(" ", None, None, False, False) for _ in range(self.cols)]
            self.erase_line(1)
            return

    def save_cursor(self):
        self.fr, self.fc = self.cr, self.cc

    def restore_cursor(self):
        self.cr, self.cc = self.fr, self.fc

    def lines(self):
        return self.grid


def lex_events(raw: bytes):
    raw = _OSC.sub(b"", raw)
    pos = 0
    out = []
    for m in _CSI.finditer(raw):
        out.append(("text", raw[pos:m.start()].decode("utf-8", "replace")))
        out.append(("csi", m.group(1).decode() or "", m.group(2).decode()))
        pos = m.end()
    out.append(("text", raw[pos:].decode("utf-8", "replace")))
    return out


def csi_params(s):
    if s.strip() in ("", "?"):
        return []
    out = []
    for tok in s.split(";"):
        t = tok.strip()
        if t.isdigit():
            out.append(int(t))
    return out


def text_after(prep, kind):
    return prep if prep else ""


def emulate(raw: bytes, rows=34, cols=100):
    scr = Screen(rows, cols)
    for ev in lex_events(raw):
        if ev[0] == "text":
            for ch in ev[1]:
                if ch == "\n":
                    scr.cr, scr.cc = scr.cr + 1, 0
                    if scr.cr > scr.r1:
                        scr.scroll_up(1)
                        scr.cr = scr.r1
                elif ch == "\r":
                    scr.cc = 0
                elif ch == "\b":
                    scr.cc = max(0, scr.cc - 1)
                elif ch == "\f":
                    scr.cr, scr.cc = 0, 0
                elif ch == "\x07":
                    pass
                elif ord(ch) < 32:
                    pass
                else:
                    scr.put_char(ch)
            continue
        _, params, final = ev
        nums = csi_params(params)
        if final == "m":
            scr.set_sgr(params); continue
        if params.startswith("?"):
            low = params[1:].split(";")
            if final == "h" and "1049" in low: scr.enter_alt()
            elif final == "l" and "1049" in low: scr.leave_alt()
            continue
        n = nums[0] if nums else 1
        if final == "A": scr.cursor_up(n)
        elif final == "B": scr.cursor_down(n)
        elif final == "C": scr.cc = min(scr.cols - 1, scr.cc + n)
        elif final == "D": scr.cc = max(0, scr.cc - n)
        elif final == "E": scr.cursor_down(n); scr.cc = 0
        elif final == "F": scr.cursor_up(n); scr.cc = 0
        elif final == "G": scr.cc = min(scr.cols - 1, max(0, n - 1))
        elif final == "d": scr.cr = min(scr.rows - 1, max(0, n - 1))
        elif final == "H" or final == "f":
            r = nums[0] - 1 if len(nums) >= 1 else 0
            c = nums[1] - 1 if len(nums) >= 2 else 0
            scr.cr = min(max(0, r), scr.rows - 1); scr.cc = min(max(0, c), scr.cols - 1)
        elif final == "J": scr.erase_screen(n if nums else 0)
        elif final == "K": scr.erase_line(n if nums else 0)
        elif final == "r":
            r0 = nums[0] - 1 if nums else 0
            r1 = nums[1] - 1 if len(nums) >= 2 else scr.rows - 1
            scr.r0, scr.r1 = max(0, r0), min(scr.rows - 1, max(r0, r1))
        elif final == "s": scr.save_cursor()
        elif final == "u": scr.restore_cursor()
    # trim fully-blank rows top & bottom
    rows = scr.lines()
    while rows and all(c[0] == " " for c in rows[0]): rows.pop(0)
    while rows and all(c[0] == " " for c in rows[-1]): rows.pop(-1)
    return rows


def cells_from(raw: bytes, rows=34, cols=100):
    return emulate(raw, rows, cols)

DEFAULT_FG = (0xD6, 0xDE, 0xE8)
DEFAULT_BG = (0x0C, 0x1F, 0x17)
BOLD_FG = (0xFF, 0xFF, 0xFF)

def pick_color(c, default, bold=False):
    if c is None:
        return BOLD_FG if bold else default
    return c[1]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="ANSI file, or - for stdin")
    ap.add_argument("output", help="PNG path")
    ap.add_argument("--font-file", default=None, help="monospace TTF/OTF (fc-match fallback)")
    ap.add_argument("--size", type=int, default=18, help="font pixel size (default 18)")
    ap.add_argument("--pad", type=int, default=18, help="padding pixels (default 18)")
    ap.add_argument("--cols", type=int, default=100, help="terminal width in cells (default 100)")
    ap.add_argument("--rows", type=int, default=34, help="terminal height in cells (default 34)")
    ap.add_argument("--scale", type=int, default=2, help="integer upscale (default 2 = retina)")
    ap.add_argument("--bg", default="#0C1F17", help="page background (default Grub Blasters green-black)")
    args = ap.parse_args()

    raw = sys.stdin.buffer.read() if args.input == "-" else open(args.input, "rb").read()
    lines = cells_from(raw, args.rows, args.cols)
    if not lines:
        print("no content to render", file=sys.stderr)
        return 1

    font_path = args.font_file
    if not font_path:
        import subprocess
        try:
            font_path = subprocess.run(["fc-match", "-f", "%{file}", "Cascadia Code"],
                                       capture_output=True, text=True, check=True).stdout.strip()
        except Exception:
            font_path = "DejaVuSansMono"
    font = ImageFont.truetype(font_path, args.size)
    cw = max(1, round(font.getlength("M")))
    ch = args.size + max(4, args.size // 3)

    width = max(len(l) for l in lines)
    img_w = width * cw + 2 * args.pad
    img_h = len(lines) * ch + 2 * args.pad
    canvas = np.full((img_h, img_w, 3), np.array(tuple(int(args.bg.lstrip("#")[i:i+2], 16) for i in (0, 2, 4)), dtype=np.uint8), dtype=np.uint8)

    im = Image.fromarray(canvas)
    draw = ImageDraw.Draw(im)
    for row, line in enumerate(lines):
        y = args.pad + row * ch
        for col, (ch_, fg, bg, bold, dim) in enumerate(line):
            x = args.pad + col * cw
            bgc = pick_color(bg, None if bg is None else DEFAULT_BG, bold=False)
            fgr = pick_color(fg, DEFAULT_FG, bold=bold)
            if dim:
                fgr = tuple(int(c * 0.62) for c in fgr)
            if bg is not None:
                draw.rectangle([x, y, x + cw - 1, y + ch - 1], fill=bgc)
            if ch_ != " ":
                draw.text((x, y), ch_, font=font, fill=fgr)
    im = im.resize((im.width * args.scale, im.height * args.scale), Image.NEAREST)
    im.save(args.output)
    print(f"wrote {args.output} ({im.width}x{im.height}) from {len(lines)} lines")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
