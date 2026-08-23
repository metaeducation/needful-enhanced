#!/usr/bin/env python3
"""
run-negative-tests.py - Compile tests that are expected to FAIL.

Each .cpp file in the negative/ directory must fail to compile when
NEEDFUL_CPP_ENHANCED=1 is defined.  A file that compiles successfully
is a test failure: it means a constraint that needful should be enforcing
is not being enforced.

Additionally, each test file may carry one or more MATCH-ERROR-TEXT comments
naming phrases that should appear in the compiler's output.  If any one phrase
matches, the error-phrase check passes.  Multiple lines cover per-compiler
variations (GCC, Clang, and MSVC often word the same error differently):

    // MATCH-ERROR-TEXT: cannot convert        <- matches GCC and MSVC
    // MATCH-ERROR-TEXT: no viable conversion  <- matches Clang

By default, a missing MATCH-ERROR-TEXT comment is accepted with a warning.
Pass --match-error-text to make missing or non-matching comments a hard
failure.  CI should always run with --match-error-text.

By convention, needful.h lives one directory above needful-enhanced/, so from
tests/ the path is ../.. .  Override with --needful-dir or NEEDFUL_H_DIR.

Usage:
    python run-negative-tests.py
    python run-negative-tests.py --match-error-text
    python run-negative-tests.py --compiler clang++
    python run-negative-tests.py --needful-dir /path/to/dir/containing/needful.h
    CXX=g++ python run-negative-tests.py --match-error-text
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def find_compiler():
    for name in ['c++', 'g++', 'clang++']:
        if shutil.which(name):
            return name
    return None


def read_expected_errors(source: Path):
    """Return all MATCH-ERROR-TEXT phrases from the source file (may be empty).

    Trailing annotations of the form '<- ...' are stripped so that compiler
    labels like '<- GCC, MSVC' can be used as inline documentation without
    becoming part of the matched phrase.
    """
    pattern = re.compile(r'//\s*MATCH-ERROR-TEXT:\s*(.+)')
    annotation = re.compile(r'\s*<-.*$')
    phrases = []
    with open(source, encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                phrase = annotation.sub('', m.group(1)).strip()
                if phrase:
                    phrases.append(phrase)
    return phrases


# Ordering for `// REQUIRES-STD:`.  Spelled out rather than compared
# numerically, because the two-digit suffixes do not sort: "98" is C++98, which
# is *older* than "11".  Anything not listed maps to 0, which means "unknown".
#
_STD_YEARS = {
    '98': 1998, '03': 2003, '11': 2011, '14': 2014,
    '17': 2017, '20': 2020, '23': 2023, '26': 2026,
}


def std_year(std):
    """'c++17' -> 2017, 'gnu++11' -> 2011, anything unrecognized -> 0."""
    m = re.search(r'(\d+)$', std or '')
    return _STD_YEARS.get(m.group(1), 0) if m else 0


def read_required_std(source: Path):
    """Return the `// REQUIRES-STD: <std>` value from a test, or None.

    Some guarantees only exist above a given language level -- Fallible(T)'s
    must-use property needs [[nodiscard]], so it is unenforced before C++17 and
    its negative test cannot fail there.  Such a test declares its floor, and
    is skipped (not failed) when run below it.
    """
    pattern = re.compile(r'//\s*REQUIRES-STD:\s*(\S+)')
    with open(source, encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if m:
                return m.group(1).strip()
    return None


def compile_file(compiler, std_flag, defines, includes, source, tmpdir):
    """Compile source; return (exit_code, stderr_text).

    Built with -Werror, matching tests/CMakeLists.txt.  Some of what Needful
    promises to reject is rejected by *warning* rather than by error --
    [[nodiscard]] most clearly: discarding a Fallible(T) warns and then exits
    0.  Without -Werror those tests could never fail, so they could never pass,
    and the guarantee would go unverified.  The headers are warning-clean, so
    this does not make other tests fail for spurious reasons, and the
    MATCH-ERROR-TEXT check still confirms each failed for its own reason.
    """
    outfile = os.path.join(tmpdir, Path(source).stem + '.out')
    cmd = (
        [compiler]
        + [std_flag]
        + ['-Werror']
        + [f'-D{d}' for d in defines]
        + [f'-I{i}' for i in includes]
        + [str(source), '-o', outfile]
    )
    result = subprocess.run(cmd, capture_output=True, text=True)
    # Merge stdout+stderr: MSVC writes diagnostics to stdout
    combined = result.stdout + result.stderr
    return result.returncode, combined


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--compiler', help='C++ compiler to use (default: auto-detect)')
    parser.add_argument('--needful-dir', help='Directory containing needful.h')
    parser.add_argument('--std', default='c++11', help='C++ standard (default: c++11)')
    parser.add_argument('--match-error-text', action='store_true',
        help='Treat missing or non-matching MATCH-ERROR-TEXT as a hard failure')
    parser.add_argument('--extra-dir', action='append', default=[],
        help='Additional directory of .cpp files to run as negative tests '
             '(may be specified multiple times; e.g. doctest-extracted/negative)')
    args = parser.parse_args()

    script_dir = Path(__file__).parent.resolve()
    negative_dir = script_dir / 'negative'

    compiler = args.compiler or os.environ.get('CXX') or find_compiler()
    if not compiler:
        print("ERROR: No C++ compiler found. Set CXX or use --compiler.")
        sys.exit(1)

    needful_h_dir = Path(
        args.needful_dir
        or os.environ.get('NEEDFUL_H_DIR')
        or (script_dir / '../..')
    ).resolve()

    if not (needful_h_dir / 'needful.h').exists():
        print(f"ERROR: needful.h not found at '{needful_h_dir}/needful.h'")
        print("Use --needful-dir or set NEEDFUL_H_DIR.")
        sys.exit(1)

    tests = sorted(negative_dir.glob('*.cpp'))
    for extra in args.extra_dir:
        extra_path = Path(extra).resolve()
        if extra_path.is_dir():
            tests = tests + sorted(extra_path.glob('*.cpp'))
        else:
            print(f"WARNING: --extra-dir '{extra}' is not a directory; skipping",
                  file=sys.stderr)

    if not tests:
        print("No negative tests found in negative/ (or extra dirs)")
        sys.exit(0)

    print(f"Compiler    : {compiler}")
    print(f"Standard    : {args.std}")
    print(f"needful.h   : {needful_h_dir}/needful.h")
    print(f"Tests       : {len(tests)}")
    print()

    passed = 0
    failed = 0
    warned = 0
    skipped = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for test in tests:
            # Skip, don't fail, a test whose guarantee does not exist at this
            # language level.  Only skip when BOTH levels are recognized: if
            # either is unknown, run the test and let the result speak.
            required = read_required_std(test)
            need_year = std_year(required)
            have_year = std_year(args.std)
            if required and need_year and have_year and need_year > have_year:
                print(f"  SKIP  {test.name}"
                      f"  (needs -std={required}, running -std={args.std})")
                skipped += 1
                continue

            expected_phrases = read_expected_errors(test)
            exit_code, output = compile_file(
                compiler=compiler,
                std_flag=f'-std={args.std}',
                defines=['NEEDFUL_CPP_ENHANCED=1'],
                includes=[str(needful_h_dir)],
                source=test,
                tmpdir=tmpdir,
            )

            if exit_code == 0:
                # Compiled successfully — this is always a failure
                print(f"  FAIL  {test.name}  (compiled when it should not have)")
                if expected_phrases:
                    for p in expected_phrases:
                        print(f"        (expected error phrase: '{p}')")
                failed += 1
                continue

            # Compilation failed — now check it was for the right reason
            if not expected_phrases:
                if args.match_error_text:
                    print(f"  FAIL  {test.name}  (no MATCH-ERROR-TEXT comment; required with --match-error-text)")
                    failed += 1
                else:
                    print(f"  PASS  {test.name}  [no MATCH-ERROR-TEXT — any failure accepted]")
                    warned += 1
                    passed += 1
            elif any(p.lower() in output.lower() for p in expected_phrases):
                print(f"  PASS  {test.name}")
                passed += 1
            else:
                print(f"  FAIL  {test.name}  (failed, but no expected phrase found in output)")
                for p in expected_phrases:
                    print(f"        expected : '{p}'")
                # Show first few lines of output to aid diagnosis
                snippet = output.strip().splitlines()[:5]
                for line in snippet:
                    print(f"        output   : {line}")
                failed += 1

    print()
    if warned:
        print(f"  ({warned} test(s) lack MATCH-ERROR-TEXT comments; use --match-error-text to require them)")
    if skipped:
        print(f"  ({skipped} test(s) skipped as above their REQUIRES-STD floor;"
              f" run with a higher --std to include them)")
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
