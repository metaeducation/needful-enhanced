---
layout: home
title: Needful
nav_order: 1
permalink: /
---

# Needful

**[One header.][needful-h-raw]**
**[Zero-dependency shipping builds.](/design#zero-dependency)**
**Rust-grade type checking for C.**

Needful is a [single-file, header-only library][needful-repo] that brings
`Option(T)`, `Result(T)`, non-null `Need(T)`, type-safe casts, and executable
comment annotations to C codebases.

It is a type-discipline layer, not a memory-safety layer. It catches
mis-modeled values and interface mistakes; it does not prevent use-after-free,
buffer overruns, data races, or overflow bugs.

The key trick is architectural, not magical: the same source can be compiled
in multiple modes, and the macros change role from transparent vocabulary to
strict checking wrappers.

Your C code stays C. The C++ compiler just *checks* it harder.

From a security perspective, Needful is straightforward to evaluate: whether
release builds use C or C++, if `NEEDFUL_CPP_ENHANCED` is off you are running
zero additional runtime code. Running checked builds in CI then adds static
analysis pressure that finds real bugs.

## How It Works (No Surprises)

Needful has three practical build modes:

| Build mode | What you include | Dependency profile | Typical purpose |
|---|---|---|---|
| C (default) | `needful.h` | Single header + standard C headers only | Production/release |
| C++ (unenhanced) | `needful.h` | Single header + standard C/C++ headers only | Compatibility check pass |
| C++ enhanced (`NEEDFUL_CPP_ENHANCED=1`) | `needful.h` + `needful-enhanced/` companion tree | Extra C++ metaprogramming headers (for example `type_traits`) | Strong static-analysis pass in CI |

So "zero dependencies" means: in shipping builds (C, or C++ without
enhancement), Needful adds no runtime library and no companion-tree
requirement. The extra dependencies are only for the optional enhanced check
mode, which is exactly where the heavier C++ machinery lives.

---

## Getting Started

**Step 1.** Drop [`needful.h`][needful-h-raw] into your project and
`#include` it. All macros expand to trivial C - your build won't even notice.

**Step 2.** When ready, add the [`needful-enhanced/`][needful-enhanced-repo]
enhancement files alongside `needful.h`. *(Put this directory in your
.gitignore, don't commit it.)*  `#define NEEDFUL_CPP_ENHANCED  1` and build as
[C++11 or later][cpp11-baseline].  The macros grow teeth: type mismatches
become compile errors.

If the C++ compiler rejects your C before any Needful check runs, that is
expected on a first attempt: see [Building C as C++](/c-as-cpp).

For CI to enforce those checks reliably, run the checked build with warnings
as errors (`-Werror` / `/WX`).

What that changes in practice:

| Feature area | Without `-Werror` / `/WX` | With `-Werror` / `/WX` |
|---|---|---|
| `Need(T)`, `Option(T)`, `Sink/Init/Contra`, `known(...)` type constraints | Still meaningful: these are mostly hard type errors | Same guarantees, plus warning-based checks below become enforceable |
| Comment macros (`possibly`, `dont`, etc.) | Still meaningful: stale/invalid expressions fail compilation | Same |
| `Fallible(T)` / must-use checks | Often degrades: compilers typically emit a warning, then exit 0 | Enforced in CI as intended |
| `Result(T)` dropped-value/must-use style checks (where implemented via nodiscard-style diagnostics) | Can degrade to warnings depending on compiler/mode | Enforced as failures |

So yes, Needful is still useful without warnings-as-errors, but part of the
"must use / must handle" promise becomes advisory instead of mandatory.

**Step 3.** Run both modes in CI: C build for production, C++ enhanced build to
catch bugs. No code changes needed between them!

---

## What You Get

| Construct | What It Does |
|---|---|
| [`Need(T)`](/need) | Non-null/non-zero type; blocks boolean coercion |
| [`Option(T)`](/option) | Rust-like optional with same size as `T` |
| [`Fallible(T)`](/fallible) | Must-use Option(T) for return types; `FallibleVar(T)` elsewhere |
| [`Result(T)`](/result) | Multiplexed error + return value; auto-propagation via `return_if_failed` |
| [`cast()` family](/cast) | Visible, hookable, semantically-named casts |
| [`nocast`](/nocast) | Restores C's implicit `void*` and enum conversions in a C++ build |
| [Comment macros](/comments) | `possibly()`, `dont()`, `heeded()` -- comments that fail the build when they go stale |
| [`Contra(T)` / `Sink(T)` / `Init(T)`](/contra) | Contravariant output parameter markers |
| [`known(T, expr)`](/known) | Zero-cost compile-time type assertion, for macro authors |

Two of these are worth a second look, because their names undersell them:

- **[`nocast`](/nocast)** is what makes "just compile my C as C++" survive
  contact with a real codebase. Without it, step 2 above buries you in errors
  about `void*` returns from `malloc()` and `int`-to-enum assignments that C
  was perfectly happy with. The incompatibilities it does not cover, and the
  fix for each, are in [Building C as C++](/c-as-cpp).

- **[Comment macros](/comments)** are the only part of Needful with no
  equivalent in Rust, C++, or anything else -- and the only part that asks for
  no type changes at all. If you want to try one thing, try these.

---

## Design Goals

- **No magic.** [The C definitions are written out in full][needful-h-raw] so
  you can see how trivial they are. Adding Needful to a C project is a
  low-impact proposition.

- **Gradual adoption.** Use just `nocast` today. Add `Option` next month.
  Enable `NEEDFUL_CPP_ENHANCED` when you're ready. Nothing breaks.

- **Codebase documentation.** The comment macros (`possibly`, `definitely`,
  `impossible`, `unnecessary`, `dont`) replace prose comments with
  compile-checked expressions that keep themselves up-to-date.

- **Documentation that compiles.** The same idea, turned on this site. Every
  example here that demonstrates a checked behavior is extracted and built in
  CI on GCC, Clang, and MSVC -- the positive ones must compile *and run*, and
  the negative ones must **fail** to compile, for the stated reason. A sample
  that drifts out of date does not quietly mislead you; it turns the build
  red. The links between these pages are checked the same way.

---

## Who This Is For

Needful is most useful when all of these are true:

- You have a C codebase where nullability, error propagation, and output
  parameter misuse produce expensive bugs.

- You can run an occasional checked C++ build in CI, even if release artifacts
  are still built as C.

- You want stronger static checking without introducing runtime dependencies or
  rewriting the codebase into another language.

If your primary objective is memory safety, this is the wrong tool to lead
with. If your objective is stronger value and API discipline in existing C,
this is exactly the niche.

---

## The Weak Points

Most projects show you a softball. Here are the hardballs, up front, so you
find them now instead of three weeks in.

**It is not a memory-safety solution.** Needful is a *type discipline* layer.
It will not stop a use-after-free, a buffer overrun, a data race, or an
integer overflow. What it catches is the class of bug where a value's *meaning*
was misunderstood: a maybe-null treated as never-null, an error return dropped
on the floor, an output parameter given the wrong end of a type hierarchy.
That is a real and valuable class. It is not the one that gets CVEs assigned.

**Enforcement is not uniform, and one common configuration checks nothing.**
`Fallible(T)` makes three promises and no single build delivers all three;
MSVC in C mode below C23 delivers *none* of them. The full matrix is
[documented honestly](/fallible#enforcement) rather than buried -- read it
before you decide which build is your real gate.

**Warnings must be errors or you get nothing.** Most of what Needful rejects
arrives as a warning, and the compiler then exits zero. Without `-Werror` /
`/WX`, CI stays green over exactly the mistakes you added Needful to catch.

**The checked build needs a second repository.** We say "one header, no
dependencies," and that is true of the shipping build. The *checking* build
wants `needful-enhanced/` cloned next to it and version-matched. It is a
deliberate trade -- but it is the point where the simple story stops being
simple.

**Your C may not parse as C++ yet.** C is not a subset of C++. A `goto` over
an initialization, a designated initializer written out of order, a variable
named `new`: each one stops the checked build before Needful has looked at
anything. [`nocast`](/nocast) covers the two most common breaks, and
[Building C as C++](/c-as-cpp) has a workaround for most of the rest. Bringing
an existing codebase across is still real work, and a few constructs have no
workaround at all.

**It is a dialect.** There are a couple hundred macros. The vocabulary you
actually need is more like twenty-five, but the cast family alone has fifteen
spellings with lenient and rigid variants. Expect to spend time on taxonomy,
and expect a new contributor to ask what `unwrap` is.

**The prefix keywords are a trick, and tricks leak.** `unwrap foo` is really
`g_unwrap_helper + foo`, which means precedence surprises (an entire
[page](/precedence) exists for them) and tooling -- formatters, highlighters,
code search -- that is mildly wrong forever. The saving grace is that a
misparse is *always* a compile error and never a wrong answer.

**Threading is explicit configuration, not magic.** `Result(T)` default hooks
are single-threaded and should be replaced with thread-local storage in
threaded programs. Corruption helpers (`NEEDFUL_DOES_CORRUPTIONS`) are
thread-safe by default in enhanced C++ builds (atomics), with optional lock
hooks and explicit single-thread mode for constrained environments.

If none of those are dealbreakers, the rest of this site is the good parts.

---

## Frequently Asked Questions

Common gotchas and design rationale are collected in the [FAQ](/faq).
Threaded integration details are collected in [Threading](/threading).

[needful-h-raw]: https://raw.githubusercontent.com/metaeducation/needful/refs/heads/main/needful.h
[needful-repo]: https://github.com/metaeducation/needful/
[needful-enhanced-repo]: https://github.com/metaeducation/needful-enhanced/
[cpp11-baseline]: https://needful.metaeducation.com/faq#cpp11-baseline
