---
layout: home
title: Needful
nav_order: 1
permalink: /
---

# Needful

**[One header.][needful-h-raw] Zero dependencies. Rust-grade type checking for C.**

Needful is a [single-file, header-only library][needful-repo] that brings
`Option(T)`, `Result(T)`, non-null `Need(T)`, type-safe casts, and executable
comment annotations to C codebases, with zero runtime cost and no tools beyond
the compiler you already have.

The key trick: most Needful constructs compile as a **transparent no-op in
C**. Flip one switch (`#define NEEDFUL_CPP_ENHANCED  1`) and build as
C++11 (or even better, C++17!)  Those same constructs light up with
compile-time type enforcement that catches real bugs.

Your C code stays C. The C++ compiler just *checks* it harder.

From a security perspective, pitching Needful to your organization should be
a no-brainer.  Whether you make your release builds with C or C++, if you
aren't using NEEDFUL_CPP_ENHANCED *you are running zero additional code*.  And
doing enhanced builds now and again gives static analysis that finds real bugs.

---

## Getting Started

**Step 1.** Drop [`needful.h`][needful-h-raw] into your project and
`#include` it. All macros expand to trivial C - your build won't even notice.

**Step 2.** When ready, add the [`needful-enhanced/`][needful-enhanced-repo]
enhancement files alongside `needful.h`. *(Put this directory in your
.gitignore, don't commit it.)*  `#define NEEDFUL_CPP_ENHANCED  1` and build as
C++11 or later.  The macros grow teeth: type mismatches become compile errors.

**Step 3.** Run both modes in CI: C build for production, C++ build to catch
bugs. No code changes needed between them!

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
| [Comment macros](/comments) | `possibly()`, `dont()`, `heeded()` — comments that fail the build when they go stale |
| [`Contra(T)` / `Sink(T)` / `Init(T)`](/contra) | Contravariant output parameter markers |
| [`known(T, expr)`](/known) | Zero-cost compile-time type assertion, for macro authors |

Two of these are worth a second look, because their names undersell them:

- **[`nocast`](/nocast)** is what makes "just compile my C as C++" survive
  contact with a real codebase. Without it, step 2 above buries you in errors
  about `void*` returns from `malloc()` and `int`-to-enum assignments that C
  was perfectly happy with.
- **[Comment macros](/comments)** are the only part of Needful with no
  equivalent in Rust, C++, or anything else — and the only part that asks for
  no type changes at all. If you want to try one thing, try these.

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
[documented honestly](/fallible#enforcement) rather than buried — read it
before you decide which build is your real gate.

**Warnings must be errors or you get nothing.** Most of what Needful rejects
arrives as a warning, and the compiler then exits zero. Without `-Werror` /
`/WX`, CI stays green over exactly the mistakes you added Needful to catch.

**The checked build needs a second repository.** We say "one header, no
dependencies," and that is true of the shipping build. The *checking* build
wants `needful-enhanced/` cloned next to it and version-matched. It is a
deliberate trade — but it is the point where the simple story stops being
simple.

**It is a dialect.** There are a couple hundred macros. The vocabulary you
actually need is more like twenty-five, but the cast family alone has fifteen
spellings with lenient and rigid variants. Expect to spend time on taxonomy,
and expect a new contributor to ask what `unwrap` is.

**The prefix keywords are a trick, and tricks leak.** `unwrap foo` is really
`g_unwrap_helper + foo`, which means precedence surprises (an entire
[page](/precedence) exists for them) and tooling — formatters, highlighters,
code search — that is mildly wrong forever. The saving grace is that a
misparse is *always* a compile error and never a wrong answer.

**`Result(T)` multiplexes through global state.** The default hooks are
single-threaded. Making them thread-local is your job, and the design assumes
one pending failure at a time.

If none of those are dealbreakers, the rest of this site is the good parts.

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

---

## Frequently Asked Questions

Common gotchas and design rationale are collected in the [FAQ](/faq).

[needful-h-raw]: https://raw.githubusercontent.com/metaeducation/needful/refs/heads/main/needful.h
[needful-repo]: https://github.com/metaeducation/needful/
[needful-enhanced-repo]: https://github.com/metaeducation/needful-enhanced/
