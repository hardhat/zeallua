#!/usr/bin/env python3
"""Add labels from a matching .sym file into a .dis file.

Given an input .dis file, this script looks for a sibling .sym file with the
same basename (e.g. foo.dis -> foo.sym). It then inserts `label:` lines before
any disassembly line whose address matches a label in the .sym file, and also
rewrites operand address literals like `$4011` to symbol names like `pc_ptr`
when a matching symbol exists.

By default output is written to stdout so you can redirect it to a file.

Example:
  python label_dis.py test_host.dis > test_host.labeled.dis
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import DefaultDict, Iterable


_ADDR_IN_DIS_RE = re.compile(r";\[\s*([0-9A-Fa-f]{1,8})\s*\]")
_HEX_OPERAND_RE = re.compile(r"\$([0-9A-Fa-f]{1,8})\b")
_SYM_LINE_RE = re.compile(r"^\s*([A-Za-z_.$?@][\w.$?@]*)\s+EQU\s+(.+?)\s*$", re.IGNORECASE)


def _parse_sym_value(value: str) -> int:
    value = value.strip()
    if not value:
        raise ValueError("empty value")

    # Common notations: 0x1234, $1234, 1234h
    if value.lower().startswith("0x"):
        return int(value[2:], 16)
    if value.startswith("$"):
        return int(value[1:], 16)
    if value.lower().endswith("h") and len(value) > 1:
        return int(value[:-1], 16)

    # In this codebase, addresses are typically hexadecimal even without a prefix.
    return int(value, 16)


def load_sym_labels(sym_path: Path) -> DefaultDict[int, list[str]]:
    labels: DefaultDict[int, list[str]] = defaultdict(list)

    with sym_path.open("r", encoding="utf-8", errors="replace") as f:
        for raw_line in f:
            line = raw_line.rstrip("\n")

            # Strip simple ';' comments if present
            if ";" in line:
                line = line.split(";", 1)[0]

            line = line.strip()
            if not line:
                continue

            m = _SYM_LINE_RE.match(line)
            if not m:
                continue

            name, value = m.group(1), m.group(2)
            try:
                addr = _parse_sym_value(value)
            except ValueError:
                continue

            labels[addr].append(name)

    return labels


def substitute_operand_labels(line: str, labels_by_addr: dict[int, list[str]]) -> str:
    code_part, sep, comment_part = line.partition(";")

    def replace_addr(match: re.Match[str]) -> str:
        addr = int(match.group(1), 16)
        labels = labels_by_addr.get(addr)
        if not labels:
            return match.group(0)
        return labels[0]

    rewritten_code = _HEX_OPERAND_RE.sub(replace_addr, code_part)
    if not sep:
        return rewritten_code
    return rewritten_code + sep + comment_part


def iter_labeled_dis_lines(dis_lines: Iterable[str], labels_by_addr: dict[int, list[str]]) -> Iterable[str]:
    emitted_addrs: set[int] = set()

    for line in dis_lines:
        m = _ADDR_IN_DIS_RE.search(line)
        if m:
            try:
                addr = int(m.group(1), 16)
            except ValueError:
                addr = None  # type: ignore[assignment]

            if addr is not None and addr in labels_by_addr and addr not in emitted_addrs:
                for name in labels_by_addr[addr]:
                    yield f"{name}:\n"
                emitted_addrs.add(addr)

        yield substitute_operand_labels(line, labels_by_addr)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Insert labels and operand symbol names from a matching .sym file into a .dis file.",
    )
    parser.add_argument(
        "dis_file",
        help="Path to input .dis file (matching .sym is inferred by basename)",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Write output to this file (default: stdout)",
    )

    args = parser.parse_args(argv)

    dis_path = Path(args.dis_file)
    if dis_path.suffix.lower() != ".dis":
        parser.error("input must have a .dis extension")

    sym_path = dis_path.with_suffix(".sym")
    if not sym_path.exists():
        print(f"error: matching .sym not found: {sym_path}", file=sys.stderr)
        return 2

    labels_by_addr = load_sym_labels(sym_path)

    with dis_path.open("r", encoding="utf-8", errors="replace") as dis_f:
        labeled_iter = iter_labeled_dis_lines(dis_f, labels_by_addr)

        if args.output:
            out_path = Path(args.output)
            with out_path.open("w", encoding="utf-8", newline="\n") as out_f:
                out_f.writelines(labeled_iter)
        else:
            try:
                sys.stdout.writelines(labeled_iter)
            except BrokenPipeError:
                # Common when piping into commands that close early (e.g. `head`).
                return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
