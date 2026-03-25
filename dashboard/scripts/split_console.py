#!/usr/bin/env python3
"""Run xv6/QEMU under a PTY and split interactive output from telemetry frames."""

from __future__ import annotations

import argparse
import os
import pty
import re
import select
import signal
import subprocess
import sys
import termios
import tty
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
KEYWORD_RE = re.compile(r"(DASHBOARDD_BEGIN|DASHBOARDD_END|DASHBOARDD_ERROR|SNAP\s+|PROC\s+|VMA\s+)")
TELEMETRY_PREFIXES = (
    "DASHBOARDD_BEGIN",
    "DASHBOARDD_END",
    "DASHBOARDD_ERROR",
    "SNAP ",
    "PROC ",
    "VMA ",
)


def sanitize(line: str) -> str:
    clean = ANSI_RE.sub("", line)
    clean = clean.replace("\r", "")
    clean = clean.replace("$", "")
    return "".join(ch for ch in clean if ch == "\n" or ch == "\t" or 32 <= ord(ch) <= 126)


def classify_prefix(buffer: str) -> str:
    stripped = ANSI_RE.sub("", buffer).lstrip()
    if not stripped:
        return "undecided"
    if any(stripped.startswith(prefix) for prefix in TELEMETRY_PREFIXES):
        return "telemetry"
    if any(prefix.startswith(stripped) for prefix in TELEMETRY_PREFIXES):
        return "undecided"
    return "regular"


class OutputSplitter:
    def __init__(self, console_log, telemetry_log, terminal):
        self.console_log = console_log
        self.telemetry_log = telemetry_log
        self.terminal = terminal
        self.buffer = ""

    def _emit_terminal(self, text: str) -> None:
        if text:
            self.terminal.write(text)
            self.terminal.flush()

    def _emit_telemetry(self, text: str) -> None:
        clean = sanitize(text)
        match = KEYWORD_RE.search(clean)
        if not match:
            return
        frame = clean[match.start() :].strip()
        if frame:
            self.telemetry_log.write(frame + "\n")
            self.telemetry_log.flush()

    def _flush_line(self, text: str) -> None:
        kind = classify_prefix(text)
        if kind == "telemetry":
            self._emit_telemetry(text)
        else:
            self._emit_terminal(text)

    def feed(self, text: str) -> None:
        if not text:
            return
        self.console_log.write(text)
        self.console_log.flush()

        for ch in text:
            self.buffer += ch
            if ch == "\n":
                self._flush_line(self.buffer)
                self.buffer = ""
                continue

            kind = classify_prefix(self.buffer)
            if kind == "regular":
                self._emit_terminal(self.buffer)
                self.buffer = ""

    def flush(self) -> None:
        if not self.buffer:
            return
        self._flush_line(self.buffer)
        self.buffer = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Split interactive xv6 console output from telemetry frames")
    parser.add_argument("--console-log", required=True)
    parser.add_argument("--telemetry-log", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        parser.error("missing command after --")
    return args


def set_winsize(fd: int) -> None:
    if not sys.stdin.isatty():
        return
    try:
        size = os.get_terminal_size(sys.stdin.fileno())
    except OSError:
        return

    import fcntl
    import struct

    packed = struct.pack("HHHH", size.lines, size.columns, 0, 0)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, packed)


def main() -> int:
    args = parse_args()
    console_path = Path(args.console_log)
    telemetry_path = Path(args.telemetry_log)
    console_path.parent.mkdir(parents=True, exist_ok=True)
    telemetry_path.parent.mkdir(parents=True, exist_ok=True)
    console_path.write_text("", encoding="utf-8")
    telemetry_path.write_text("", encoding="utf-8")

    master_fd, slave_fd = pty.openpty()
    set_winsize(slave_fd)

    child = subprocess.Popen(
        args.command,
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        close_fds=True,
        start_new_session=True,
    )
    os.close(slave_fd)

    old_tty = None
    stdin_fd = None
    if sys.stdin.isatty():
        stdin_fd = sys.stdin.fileno()
        old_tty = termios.tcgetattr(stdin_fd)
        tty.setraw(stdin_fd)

    def forward_signal(signum, _frame):
        try:
            os.killpg(child.pid, signum)
        except ProcessLookupError:
            pass

    signal.signal(signal.SIGINT, forward_signal)
    signal.signal(signal.SIGTERM, forward_signal)
    if hasattr(signal, "SIGWINCH"):
        signal.signal(signal.SIGWINCH, lambda _signum, _frame: set_winsize(master_fd))

    with console_path.open("a", encoding="utf-8", errors="ignore") as console_log, telemetry_path.open("a", encoding="utf-8", errors="ignore") as telemetry_log:
        splitter = OutputSplitter(console_log, telemetry_log, sys.stdout)
        try:
            while True:
                read_fds = [master_fd]
                if stdin_fd is not None:
                    read_fds.append(stdin_fd)
                ready, _, _ = select.select(read_fds, [], [], 0.1)

                if master_fd in ready:
                    try:
                        data = os.read(master_fd, 4096)
                    except OSError:
                        data = b""
                    if not data:
                        break
                    splitter.feed(data.decode("utf-8", errors="ignore"))

                if stdin_fd is not None and stdin_fd in ready:
                    try:
                        data = os.read(stdin_fd, 1024)
                    except OSError:
                        data = b""
                    if data:
                        os.write(master_fd, data)

                if child.poll() is not None and not ready:
                    break
        finally:
            splitter.flush()
            if old_tty is not None and stdin_fd is not None:
                termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_tty)
            os.close(master_fd)

    return child.wait()


if __name__ == "__main__":
    raise SystemExit(main())
