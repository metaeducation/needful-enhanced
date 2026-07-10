# needful-enhanced Tests

## Philosophy

Needful's primary value is compile-time enforcement — most of what it promises
is that certain things **will not compile**, not that they produce a particular
runtime value.  This shapes the whole structure of the test suite.

Tests are split into two fundamentally different categories:


### Positive Tests (`positive/`)

Standard compiled executables that must build successfully and exit 0 at
runtime.  Each covers one area of the library.  Positive tests use two kinds
of assertions:

**Static assertions** (`STATIC_ASSERT`, `STATIC_ASSERT_SAME`, etc.) check type
system properties at compile time.  They are zero-cost, run in every build
mode, and cannot have false negatives due to optimizer decisions.  Most of the
interesting checks in needful are of this kind:

```cpp
// Option(int) must not be implicitly convertible to int
STATIC_ASSERT(not needful_is_convertible_v(decltype(oi1), int));
```

**Runtime assertions** (`assert(...)`) verify dynamic behavior — that `unwrap`
returns the right value, that `none` is falsey, that Result propagation
actually propagates.  These are the minority of the tests but cover the parts
of needful that have runtime semantics.

Each concern gets its own `.cpp` file and its own binary.  This keeps failure
isolation clean: a crash or assertion failure in one test does not mask
failures in another.


### Negative Tests (`negative/`)

Source files that **must fail to compile** with `NEEDFUL_CPP_ENHANCED=1`.
Each file contains exactly one construct that needful should reject.  The test
passes when the compiler exits non-zero.

A critical discipline: a negative test that fails due to a **typo or
unrelated syntax error** is indistinguishable from one that fails for the
intended reason.  To address this, every negative test file carries a
`// MATCH-ERROR-TEXT:` comment stating a phrase that must appear in the
compiler's error output.  This phrase should be chosen to appear in the output
of all three target compilers (GCC, Clang, MSVC) for the specific violation
being tested:

```cpp
// MATCH-ERROR-TEXT: is not implicitly convertible
```

The `run-negative-tests.py` script enforces this: it captures the compiler's
output and checks that at least one expected phrase is present.  A negative
test that compiles successfully, or that fails with none of the expected
phrases, is reported as a failure.

Because different compilers word the same error differently, a single phrase
rarely covers all three targets.  Use multiple `// MATCH-ERROR-TEXT:` lines —
the check passes if *any one* matches:

```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang
```

By default (`python run-negative-tests.py`), a test file with *no*
`MATCH-ERROR-TEXT` comments still passes on any compile failure, with a
warning.  Run with `--match-error-text` to make missing or non-matching
phrases a hard failure.  CI always runs with `--match-error-text`.  This
two-tier design lets you write a new negative test and commit it immediately
— you can add the exact phrases later once you've seen what each compiler
actually says.

CTest also runs negative tests via `WILL_FAIL TRUE`, which gives a fast
"did any negative test unexpectedly compile?" check without needing Python.
The two mechanisms are complementary: CTest provides the first-pass gate;
`run-negative-tests.py` provides the "right error for the right reason" check.


## What Constitutes a Good Test

For each needful construct, the questions to ask are:

1. **What does it claim to allow?** → positive tests
2. **What does it claim to block?** → negative tests
3. **Is the claim a compile-time type property?** → static assertion (cheapest)
4. **Is the claim about runtime behavior?** → runtime assertion
5. **Is the claim about a compile failure?** → negative test file

Avoid testing things the C build already guarantees (basic syntax, arithmetic).
Focus on the delta: behavior that differs between `NEEDFUL_CPP_ENHANCED=0`
and `=1`.  The cases where the C build silently accepts something that the C++
enhanced build rejects are the most valuable tests to have.


## Docs-as-Tests

Negative tests are not only in `negative/` — they are also embedded directly
in the documentation source under `docs/*.md`.  This keeps the compile-time
example in the page that explains it, rather than in a separate file.

Fenced code blocks in `docs/*.md` are tagged with an HTML comment on the
immediately preceding line:

```markdown
<!-- doctest: positive-test -->
```cpp
// compiled and run; must exit 0
```

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: no viable conversion
// must fail to compile
```

<!-- doctest: negative-test match="phrase1|phrase2" -->
```cpp
// match= shorthand: injected as MATCH-ERROR-TEXT comments
```
```

The HTML comment is invisible in rendered pages; only `extract-doctests.py`
reads it.  Run via `tools/extract-doctests.py`, which writes extracted files
to `tests/doctest-extracted/{positive,negative}/`.  CMake runs it
automatically at configure time when `NEEDFUL_EXTRACT_DOCTESTS=ON`.

The `doctest-extracted/` directory is `.gitignore`d — it is always regenerated
from source.


## Coverage Goals

The constructs and their current test coverage:

| Construct | Positive | Negative |
|-----------|----------|----------|
| `Option(T)` | test-needful-option.cpp | docs/option.md |
| `Fallible(T)` | — | docs/fallible.md |
| Casts (`cast`, `m_cast`, etc.) | test-needful-casts.cpp | docs/cast.md |
| Const metaprogramming | test-needful-const.cpp | — |
| `Need(T)` | test-needful-need.cpp | docs/need.md |
| `Result(T)` / `trap` / `except` / `assume` / `rescue` | test-needful-result.cpp | docs/result.md |
| `known(T, expr)` / `rigid_known` / `known_any` | test-needful-known.cpp | docs/known.md |
| `Sink` / `Init` / `Contra` | test-needful-contra.cpp | docs/contra.md |
| `nocast` / `nocast_0` | test-needful-nocast.cpp | — |
| Commentary macros (`possibly`, `dont`, etc.) | test-needful-comments.cpp | docs/comments.md |
| Corruption / `NEEDFUL_DOES_CORRUPTIONS` | — | — |


## Installation Smoke Tests

CTest also runs a small installation-layout matrix that exercises the intended
consumer setup in a temporary directory. The contract is:

- `needful.h` alone must work.
- `NEEDFUL_CPP_ENHANCED` must work when `needful-enhanced/` is present beside it.
- `NEEDFUL_CPP_ENHANCED` must fail clearly when the companion tree is missing or out of sync.

The four smoke tests cover that contract directly:

- `smoke-standalone-header-only` copies only `needful.h` and verifies it can
	compile and run without any `needful-enhanced/` directory present.
- `smoke-enhanced-matching` copies `needful.h` plus the current
	`needful-enhanced/` tree and verifies a checked build compiles and runs.
- `smoke-enhanced-missing` requests `NEEDFUL_CPP_ENHANCED` without copying the
	companion directory and verifies compilation fails.
- `smoke-enhanced-mismatched-version` copies both, then intentionally mutates
	the copied compatibility macro to verify the version-mismatch `#error`.

These tests exist to catch layout regressions and header/repo skew before they
turn into confusing downstream setup failures.


## Running the Tests

**Positive + negative (via CTest):**
```sh
cd tests
cmake -B build && cmake --build build && ctest --test-dir build -V
```

**Negative tests with error phrase verification:**
```sh
cd tests
python run-negative-tests.py --match-error-text
```

**Pointing at a non-default `needful.h` location:**
```sh
cmake -B build -DNEEDUL_H_DIR=/path/to/dir/containing/needful.h
# or
NEEDFUL_H_DIR=/path/to/dir python run-negative-tests.py
```

By convention, `needful.h` is expected one directory above the `needful-enhanced/`
repo checkout — the same place a user would place it when using just the
single-file distribution.


## Testing FAQ


### Why does `STATIC_ASSERT_SAME(decltype(known(T, expr)), T)` fail?

`known()` and related macros (`rigid_known`, `known_any`, `known_not`, etc.)
expand to a comma expression of the form `(dummy_instance, (expr))`.  Because
`expr` is a local variable (an lvalue), `decltype` of a comma expression
yields a reference type: `T&` rather than `T`.

To verify the return type is correct, assign the result to a typed variable
and let the compiler check the implicit conversion:

```cpp
// Wrong: decltype gives int*& for a local variable
STATIC_ASSERT_SAME(decltype(known(int*, p)), int*);   // FAILS

// Right: assignment verifies the type is assignment-compatible with int*
int* result = known(int*, p);
```

### What does `lenient_exactly(T, expr)` actually check?

The name is counterintuitive: `lenient_exactly(T, expr)` checks that `expr`
is of type `constify(T)` — the *const* version of `T`.  It is designed for
accepting a const expression and annotating that it is *exactly* the const
form of what you expected.  So:

```cpp
const int* cp = ...;
lenient_exactly(int*, cp);    // OK: cp is const int* == constify(int*)

int* p = ...;
lenient_exactly(int*, p);     // ERROR: p is int*, not const int*
rigid_exactly(int*, p);       // OK: p is exactly int*
```

Use `rigid_exactly(T, expr)` when the source is mutable and you want an
exact-type check.  Use `lenient_exactly(T, expr)` when the source is const
and you are annotating the constified form.
