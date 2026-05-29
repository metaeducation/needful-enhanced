#!/usr/bin/env python3
"""
check-no-raw-assert.py - Ensure enhanced Needful code uses NEEDFUL_ASSERT.

Scans the C++ enhancement headers and rejects raw assert(...) calls in code.
Comments and string/char literals are ignored so examples in documentation
comments do not trigger failures.
"""

from __future__ import annotations

import pathlib
import sys


def strip_comments_and_literals(text: str) -> str:
    result: list[str] = []
    i = 0
    n = len(text)

    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
                result.append("\n")
            else:
                result.append(" ")
            i += 1
            continue

        if in_block_comment:
            if ch == "*" and nxt == "/":
                result.extend("  ")
                in_block_comment = False
                i += 2
            else:
                result.append("\n" if ch == "\n" else " ")
                i += 1
            continue

        if in_string:
            if ch == "\\":
                result.extend("  ")
                i += 2
                continue
            result.append("\n" if ch == "\n" else " ")
            if ch == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            if ch == "\\":
                result.extend("  ")
                i += 2
                continue
            result.append("\n" if ch == "\n" else " ")
            if ch == "'":
                in_char = False
            i += 1
            continue

        if ch == "/" and nxt == "/":
            result.extend("  ")
            in_line_comment = True
            i += 2
            continue

        if ch == "/" and nxt == "*":
            result.extend("  ")
            in_block_comment = True
            i += 2
            continue

        if ch == '"':
            result.append(" ")
            in_string = True
            i += 1
            continue

        if ch == "'":
            result.append(" ")
            in_char = True
            i += 1
            continue

        result.append(ch)
        i += 1

    return "".join(result)


def find_raw_asserts(path: pathlib.Path) -> list[tuple[int, str]]:
    cleaned = strip_comments_and_literals(path.read_text(encoding="utf-8"))
    hits: list[tuple[int, str]] = []

    for line_number, line in enumerate(cleaned.splitlines(), start=1):
        start = 0
        while True:
            idx = line.find("assert(", start)
            if idx == -1:
                break

            prev = line[idx - 1] if idx > 0 else ""
            if prev not in ("_",) and not prev.isalnum():
                original_line = path.read_text(encoding="utf-8").splitlines()[line_number - 1]
                hits.append((line_number, original_line.rstrip()))
            start = idx + len("assert(")

    return hits


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    targets = sorted(repo_root.glob("*.hpp"))

    failures: list[tuple[pathlib.Path, int, str]] = []
    for path in targets:
        for line_number, line in find_raw_asserts(path):
            failures.append((path, line_number, line))

    if failures:
        print("Raw assert(...) found in Needful enhancement headers:", file=sys.stderr)
        for path, line_number, line in failures:
            rel = path.relative_to(repo_root)
            print(f"  {rel}:{line_number}: {line}", file=sys.stderr)
        print("Use NEEDFUL_ASSERT(...) instead.", file=sys.stderr)
        return 1

    print(f"Checked {len(targets)} header(s); no raw assert(...) calls found.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())