#!/usr/bin/env python3
"""Drive a raw-mode interactive TUI inside a pty and capture everything it prints.

Usage: ptycapture.py <out.bin> [--cols C] [--rows R] -- cmd args...
        printf lines into the pty via a simple schedule:
        ptycapture.py --into 'line1\n' --into 'line2\n' --out out.bin -- ./prog
"""
import argparse
import os
import pty
import select
import subprocess
import sys
import time


def run(cmd, into, out, settle=1.2, row=30, col=100, kill_after=None):
    master, slave = pty.openpty()
    import fcntl, termios, struct
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", row, col, 0, 0))
    proc = subprocess.Popen(cmd, stdin=slave, stdout=slave, stderr=slave, close_fds=True,
                            start_new_session=True, env={**os.environ, "TERM": "xterm-256color"})
    os.close(slave)
    buf = bytearray()
    into_idx = 0
    t0 = time.time()
    try:
        while time.time() - t0 < (kill_after if kill_after else 30):
            r, _, _ = select.select([master], [], [], 0.05)
            if r:
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    break
                if not chunk:
                    break
                buf += chunk
                continue
            if into_idx < len(into) and time.time() - t0 > settle + into_idx * 0.4:
                os.write(master, into[into_idx].replace("\\n", "\n").encode())
                into_idx += 1
                continue
            ended = proc.poll() is not None
            if into_idx >= len(into) and ended:
                time.sleep(0.3)
                try:
                    while True:
                        r, _, _ = select.select([master], [], [], 0.05)
                        if not r:
                            break
                        chunk = os.read(master, 65536)
                        if not chunk:
                            break
                        buf += chunk
                except OSError:
                    pass
                break
    finally:
        try:
            proc.terminate()
            time.sleep(0.2)
        except Exception:
            pass
        os.close(master)
    with open(out, "wb") as f:
        f.write(buf)
    print(f"wrote {out} ({len(buf)} bytes)")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--settle", type=float, default=1.2)
    ap.add_argument("--kill-after", type=float, default=None)
    ap.add_argument("--cols", type=int, default=100)
    ap.add_argument("--rows", type=int, default=30)
    ap.add_argument("--into", action="append", default=[])
    ap.add_argument("--cmd", required=True, nargs=argparse.REMAINDER)
    args = ap.parse_args()
    cmd = args.cmd
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    run(cmd, args.into, args.out, args.settle, args.rows, args.cols, args.kill_after)
