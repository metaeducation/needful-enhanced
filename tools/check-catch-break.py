#!/usr/bin/env python3
"""
check-catch-break.py -- find `break` / `continue` that bind to catch_if_failed

`catch_if_failed (Error* e) { ... }` expands to a `for` loop that runs once
(it's the only way to scope the declaration in C99).  So a `break` or
`continue` written in the handler leaves THAT loop, and not a loop of yours
which encloses it:

    while (More()) {
        Step() catch_if_failed (Error* e) {
            break;      // leaves the catch; the `while` keeps going!
        }
    }

No compiler diagnoses this, in C or in the enhanced C++ build, because both
agree on the (unintended) meaning.  This script is the diagnostic.  See:

    https://needful.metaeducation.com/result#catch-is-a-loop

A `break` is fine if a `switch` or loop nested inside the handler owns it, and
a `continue` is fine if a nested loop owns it.  Only braced bodies are
understood (`for (...) break;` with no braces is not seen as owning it).

Usage:

    check-catch-break.py [--alias NAME]... [--jump NAME]... PATH [PATH...]

    --alias NAME   another spelling of catch_if_failed (e.g. `except`)
    --jump NAME    a macro that expands to `break` or `continue`, so is
                   treated as one (repeatable)

Exits 1 if anything was found, 0 otherwise.
"""

import argparse
import os
import re
import sys

EXTENSIONS = ('.c', '.h', '.cpp', '.hpp', '.cc', '.hh', '.inc')

LOOP_WORDS = ('for', 'while', 'do')


def blank_comments_and_strings(src):
    """Replace comments and literals with spaces, keeping offsets and newlines."""
    pattern = re.compile(
        r'//[^\n]*'
        r'|/\*.*?\*/'
        r'|"(?:\\.|[^"\\\n])*"'
        r"|'(?:\\.|[^'\\\n])*'",
        re.DOTALL,
    )

    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))

    return pattern.sub(blank, src)


def matching_brace(text, open_pos):
    depth = 0
    for i in range(open_pos, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return i
    return None


def block_introducer(text, open_pos):
    """The keyword that owns the `{` at open_pos: 'for', 'switch', ... or ''."""
    i = open_pos - 1
    while i >= 0 and text[i].isspace():
        i -= 1

    if i >= 0 and text[i] == ')':  # `for (...) {`: step back over the parens
        depth = 0
        while i >= 0:
            if text[i] == ')':
                depth += 1
            elif text[i] == '(':
                depth -= 1
                if depth == 0:
                    break
            i -= 1
        i -= 1
        while i >= 0 and text[i].isspace():
            i -= 1

    end = i + 1
    while i >= 0 and (text[i].isalnum() or text[i] == '_'):
        i -= 1
    return text[i + 1:end]  # e.g. `do {` or `else {` have no parens


def owners_between(text, body_open, pos):
    """Introducers of every brace block opened in the handler and enclosing pos."""
    owners = []
    stack = []
    for i in range(body_open + 1, pos):
        if text[i] == '{':
            stack.append(i)
        elif text[i] == '}' and stack:
            stack.pop()
    for open_pos in stack:
        owners.append(block_introducer(text, open_pos))
    return owners


def handler_bodies(text, names):
    """Yield (name, body_open, body_close) for each handler, and its `else`."""
    call = re.compile(r'\b(' + '|'.join(map(re.escape, names)) + r')\s*\(')
    for m in call.finditer(text):
        line_start = text.rfind('\n', 0, m.start()) + 1
        if text[line_start:m.start()].lstrip().startswith('#'):
            continue  # the #define of an alias, not a use

        depth = 0
        i = m.end() - 1
        while i < len(text):  # skip the (declaration)
            if text[i] == '(':
                depth += 1
            elif text[i] == ')':
                depth -= 1
                if depth == 0:
                    break
            i += 1

        while True:  # the handler, then any `else` that attaches to it
            j = i + 1
            while j < len(text) and text[j].isspace():
                j += 1
            if j >= len(text) or text[j] != '{':
                break
            close = matching_brace(text, j)
            if close is None:
                break
            yield (m.group(1), j, close)

            k = close + 1
            while k < len(text) and text[k].isspace():
                k += 1
            if not re.match(r'else\b(?!\s*if\b)', text[k:]):
                break
            i = k + len('else') - 1


def check_file(path, names, jumps):
    with open(path, encoding='utf-8', errors='replace') as f:
        text = blank_comments_and_strings(f.read())

    words = ['break', 'continue'] + jumps
    jump = re.compile(r'\b(' + '|'.join(map(re.escape, words)) + r')\b\s*;')

    found = []
    for name, body_open, body_close in handler_bodies(text, names):
        for m in jump.finditer(text, body_open, body_close):
            owners = owners_between(text, body_open, m.start())
            in_loop = any(o in LOOP_WORDS for o in owners)
            in_switch = 'switch' in owners

            word = m.group(1)
            if in_loop:
                continue  # a nested loop owns either kind of jump
            if word == 'break' and in_switch:
                continue  # a nested switch owns `break` (not `continue`!)

            line = text.count('\n', 0, m.start()) + 1
            found.append((line, word, name))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('paths', nargs='+')
    parser.add_argument('--alias', action='append', default=[])
    parser.add_argument('--jump', action='append', default=[])
    args = parser.parse_args()

    names = ['needful_catch_if_failed', 'catch_if_failed'] + args.alias

    files = []
    for path in args.paths:
        if os.path.isfile(path):
            files.append(path)
            continue
        for dirpath, _, filenames in os.walk(path):
            for filename in sorted(filenames):
                if filename.endswith(EXTENSIONS):
                    files.append(os.path.join(dirpath, filename))

    total = 0
    for path in files:
        for line, word, name in check_file(path, names, args.jump):
            print(
                "%s:%d: `%s` in a `%s` handler exits the handler, not an "
                "enclosing loop (use goto/return)" % (path, line, word, name)
            )
            total += 1

    if total:
        print("\n%d found.  See /result#catch-is-a-loop" % total)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
