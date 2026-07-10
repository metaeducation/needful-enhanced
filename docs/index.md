---
layout: home
title: Needful
nav_order: 1
permalink: /
---

# Needful

**[One header.][needful-h-raw] Zero dependencies. Rust-grade safety for C.**

Needful is a [single-file, header-only library][needful-repo] that brings
`Option(T)`, `Result(T)`, non-null `Need(T)`, type-safe casts, and executable
comment annotations to C codebases, with zero runtime cost and no tools beyond
the compiler you already have.

The key trick: every Needful construct compiles as a **transparent no-op in
C**. Flip one switch (`#define NEEDFUL_CPP_ENHANCED  1`) and build as
C++11, and those same constructs light up with compile-time type enforcement
that catches real bugs. Your C code stays C. The C++ compiler just *checks*
it harder.

From a security perspective, pitching Needful to your organization should be
a no-brainer.  Whether you make your release builds with C or C++, if you
aren't using NEEDFUL_CPP_ENHANCED *you are running zero additional code*.  And
doing enhanced builds now and again gives static analysis that finds real bugs.

---

## Getting Started

**Step 1.** Drop [`needful.h`][needful-h-raw] into your project and
`#include` it. All macros expand to trivial C - your build won't even notice.

**Step 2.** When ready, add the [`needful-enhanced/`][needful-enhanced-repo]
enhancement files alongside `needful.h` and
`#define NEEDFUL_CPP_ENHANCED  1`. Build as C++11 or later. Every macro grows
teeth: type mismatches become compile errors.

**Step 3.** Run both modes in CI: C build for production, C++ build to catch
bugs. No code changes needed between them.

---

## What You Get

| Construct | What It Does |
|---|---|
| [`Need(T)`](/need) | Non-null/non-zero type; blocks boolean coercion |
| [`Option(T)`](/option) | Rust-like optional with same size as `T` |
| [`Fallible(T)`](/fallible) | Implicitly `[[nodiscard]]` variant of Option(T) |
| [`Result(T)`](/result) | Multiplexed error + return value; auto-propagation via `trap` |
| [`cast()` family](/cast) | Visible, hookable, semantically-named casts |
| [`Contra(T)` / `Sink(T)` / `Init(T)`](/contra) | Contravariant output parameter markers |
| [`known(T, expr)`](/known) | Zero-cost compile-time type assertion inside macros |
| [Comment macros](/comments) | `possibly()`, `dont()`, `heeded()` — executable documentation |
| [`nocast`](/nocast) | Bridge `void*` and enum zero between C and C++ |

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
