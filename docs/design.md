---
layout: default
title: Design Rationale
nav_order: 55
permalink: /design
---

# Why `needful.h` Is Written The Way It Is

This page is for anyone **editing** Needful -- including AI assistants, who
otherwise re-derive (or re-litigate) the same reasoning every session.

It is also useful if you are just trying to understand why a construct is
spelled the way it is. Most of what looks arbitrary in `needful.h` is a
consequence of one invariant, stated first below.

---

## The governing invariant: three build modes must agree

Needful is not two configurations, it is **three**, and every construct has to
mean the same thing in all of them:

| # | Mode | How you get it | What the macros are |
|---|---|---|---|
| 1 | **C** | just `#include "needful.h"` | transparent -- mostly nothing at all |
| 2 | **C++, unenhanced** | same source built by a C++ compiler | still transparent, but C++ typing rules apply |
| 3 | **C++, enhanced** | `NEEDFUL_CPP_ENHANCED 1` + companion tree | wrapper classes with real enforcement |

Mode 2 is not hypothetical. It is what happens to any C project that builds
some translation units as C++, and it is the mode people forget.

### Mode 2 is where the bugs live

Modes 1 and 3 get all the attention: mode 1 is what ships, mode 3 is what
catches bugs. Mode 2 gets neither the C compiler's permissiveness nor the
enhanced layer's wrappers, so it is the mode most likely to be quietly wrong.

Worse, a mode-2 failure is *invisible in CI* unless something explicitly builds
that way. A construct can be broken there for months while both the C build and
the enhanced build stay green.

**When you add or change a construct, reason about mode 2 first.**

### Worked example: why `none` is not a null pointer {#worked-example}

The none-reactive macros (`return_if_none`, `abort_if_none`, ...) test with:

```c
(expr) == needful_none
```

not against `nullptr` or `(void*)0`. The reason is the three-mode invariant.

`Option(T)` and `Fallible(T)` require only that `T` be explicitly convertible
to `bool`. So `Option(SomeEnum)` is perfectly ordinary -- and comparing an enum
against `(void*)0` is a constraint violation in C and an error in C++.

That alone would be a portability bug. It was also, historically, a *three-mode*
bug: `needful.h` used to suppress `-Wint-conversion` in C builds, so on GCC and
Clang the enum mistake **compiled silently as C** and failed only under
enhancement -- exactly the C/C++ divergence Needful exists to prevent, produced
by Needful itself.

That suppression is gone ([why](/faq#int-conversion-warning)), so the mistake
is now diagnosed in mode 1 as well. Note what that does and does not change:
the pragma made the bug *worse*, it did not make it. Comparing an enum against
`(void*)0` is wrong on its own terms, and the reasoning below stands
independently of any diagnostic setting.

So `none` gets one spelling that means the same thing three times:

| Mode | `needful_none` expands to |
|---|---|
| C | `0` |
| C++, unenhanced | `needful::Nocast0Struct{}` |
| C++, enhanced | `needful::NoneStruct{}` |

Each of those needs comparison to work for *both* pointers and enums:

- **C** -- plain `0 ==` works for pointers, enums, and integers alike.

- **C++, unenhanced** -- `Nocast0Struct` has a templated conversion operator, so
  `ptr == nocast_0` would already work. **Enums would not.** That is why
  `needful.h` defines `operator==`/`!=` for `Nocast0Struct` directly. The
  trailing return type is the constraint: `-> decltype(! v)` means that if `!v`
  is not valid for `T`, substitution fails and the overload drops out.

- **C++, enhanced** -- `Option(T)` supplies its own `NoneStruct` operators.

Take any one of those three away and the construct works right up until
somebody declares a `Fallible(SomeEnum)`, which is why all three exist.

---

## What belongs in `needful.h`, and what belongs here

`needful.h` should be as small and punchy as it can be while still letting
someone who found it in a project's include directory understand what they are
looking at. It reads top to bottom as a narrative.

The dividing rule:

> If a note explains **why this design and not another**, it belongs in the
> docs. If it explains **what this line does**, it should be short enough to
> ride on the line itself.

There is almost no legitimate middle case -- and the middle is exactly where
numbered footnotes live. Hence:

**`needful.h` has no numbered footnotes.** If you find yourself writing
`[1]`, the explanation has outgrown the header. Move it here and leave a link.

One exception is load-bearing and stays in the header as a lettered note,
because it must be read *before* you `#include`:

- **[A]** `NEEDFUL_ASSERT` / `NEEDFUL_ABORT` hooks. Actionable configuration.


### Other header conventions

- **Part 1 is the vocabulary; Part 2 is the plumbing.** A reader never has to
  reach Part 2. This works because macro bodies expand lazily -- a Part 1 macro
  may reference a Part 2 macro defined later in the file. Only *real code*
  (the `typedef enum`, `needful_dead_end_inline`, the C++ namespace blocks, the
  default result hooks) has genuine ordering constraints, and all of it is
  placed accordingly.

- **Every vocabulary section carries a `Docs:` link.** That link is the
  mechanism that lets the header stay short without the reader losing anything.

- **The C definitions are written out in full.** This is an *auditability*
  feature, not a stylistic one: a maintainer evaluating Needful can read the
  file and confirm there is no magic. Do not factor the C definitions into
  cleverness.

- **`#ifdef __cplusplus` only where unavoidable.** C++ variations are done by
  `#undef` and re-`#define` from the companion tree. The unavoidable cases are
  the ones where C++ *cannot* express the C spelling at all -- e.g.
  `NEEDFUL_NULLPTR`, since C++ will not implicitly convert `(void*)0` to an
  arbitrary `T*`.

- **`Need(T)` leads the feature list, even though `Option(T)` is more broadly
  useful.** Position #1 does *orientation*, not persuasion: `Option(T)` is
  defined in contrast to a value that cannot be absent, so `Need(T)` is what
  establishes the axis, and introducing the pair in the other order reads
  backwards. The banner example uses `Option(T)`, so the more-useful construct
  does the persuading before the list starts. Keep the entries adjacent and
  keep the "dual" phrasing that ties them together.

- **The banner example uses `Option(T)`, and that is deliberate.** The opening
  example has *two* jobs: show a construct, and explain the C-versus-enhanced
  mechanism that the whole library rests on. It can therefore only afford to
  introduce **one** new idea. `Option(T)` is a concept the reader arrives
  already understanding, so all the novelty budget goes to the two-build-mode
  trick. A more distinctive construct (the comment macros are the tempting
  candidate) would mean arguing for something unfamiliar *while* also teaching
  the mechanism -- two new things at once, and the mechanism is the one that
  matters. One step at a time.

- **One example, at the top, and no more.** Examples in the docs are compiled
  in CI ([see the doctest system](https://github.com/metaeducation/needful-enhanced/blob/main/docs/README.md));
  examples in the header are not, so they rot. The banner carries a single
  `Option(T)` example to convey the *sense* of the library, and that is all.


### Naming and Casing Conventions

Needful employs a deliberate casing split to signal a macro's role to the reader:

- **PascalCase for Types, Declarations, and Hooks:** Constructs that act as
types, wrappers, structural annotations, or user-supplied hooks use PascalCase
(e.g., `NeedfulOption(T)`, `NeedfulSink`, `Needful_Get_Failure`,
`NeedfulResult(T)`).

- **snake_case for Operative Macros:** Constructs that execute code, evaluate assertions, or control flow use snake_case (e.g., `needful_return_if_failed`, `needful_return_fail`, `needful_cast`).

- **Internal State Variables:** Scoped dummy variables injected by macros use aggressive underscores (e.g., `_done_`) or the `NEEDFUL_UNIQUE_NAME` generator
to avoid shadowing or colliding with user scope.

Shorthands (`Option`, `return_if_failed`) preserve the exact casing of their `needful_` prefixed parents.

### The Zero-Dependency Footprint {#zero-dependency}

"Zero dependency" in Needful is scoped, and the scope matters:

- **Shipping/runtime scope:** Needful adds no runtime library dependency.
  In non-enhanced builds, the constructs are macro-level vocabulary and your
  binary does not gain a new linked component.

- **Packaging scope (default use):** C projects can ship with `needful.h`
  alone. C++ projects can also build against `needful.h` alone when
  `NEEDFUL_CPP_ENHANCED` is off.

- **Enhanced-check scope:** when `NEEDFUL_CPP_ENHANCED=1`, you are opting into
  the companion `needful-enhanced/` tree and its C++ metaprogramming headers.
  That mode is intentionally a stronger static-analysis pass, usually run in
  CI or other checking builds.

This is the architecture in one line: **single-header for shipping; companion
tree for stronger checking.**

Concrete dependency split:

| Build mode | Required Needful files | Typical standard headers introduced by Needful |
|---|---|---|
| C, unenhanced | `needful.h` | none required beyond C headers it may include (`assert.h`, `stdlib.h`) unless caller overrides hooks |
| C++, unenhanced | `needful.h` | C++ header support used directly in `needful.h` (notably `type_traits` for `nocast`) plus the same opt-out C headers |
| C++, enhanced | `needful.h` + `needful-enhanced/` | all of the above, plus enhanced-layer headers such as `utility` and `stdint.h`, and platform-conditional includes like `iso646.h` (MSVC) and optional `stdarg.h` |

Even in enhanced mode, the standard-library surface is intentionally narrow:
type traits, forwarding, and low-level utility headers. It avoids
container-heavy dependencies (for example `vector`) and does not assume a
project-wide STL allocator/runtime model.

Needful still aggressively avoids unnecessary host-project coupling. For
example:

- **No `<stdbool.h>`:** Constructs like `catch_if_failed` deliberately use
integer states (`int _done_ = 0` / `_done_ == 0`) rather than standard boolean
types.

- **Opt-out `<assert.h>` / `<stdlib.h>`:** `NEEDFUL_ASSERT` and `NEEDFUL_ABORT`
only include their standard library headers if the host project has not already
provided a definition before including `needful.h`.

### [C++11 is the Baseline][cpp11-baseline] for the Enhanced Layer

If you read the headers in `needful-enhanced/`, you will see complex SFINAE metaprogramming, recursive variadic templates, and backported traits (like `needful::void_t`) that could be trivially replaced by C++17 fold expression
 or C++20 Concepts.

**This is intentional.** Needful is designed to be retrofitted into classical C codebases inside legacy environments (older CI servers, VMs, constrained SDKs).
Forcing the host to procure a C++17/20 compiler just to run the static checking
pass limits the library's reach. The complex C++11 boilerplate in the companion
tree pays the tax so the user's infrastructure doesn't have to.

[cpp11-baseline]: https://needful.metaeducation.com/faq#cpp11-baseline

### Why Some Code Stays Unfactored {#dont-factor-hot-wrapper-paths}

In the enhanced headers, you will see local duplication that would normally be
factored into tiny helper methods.

In ordinary library code, factoring is usually cleaner. Here, it can cost real
debug-build performance in toolchains/configurations that do not inline
aggressively (or at all).

`Sink(T)` is the clearest case: conversion operators, assignment paths, and
destructor all run short corruption checks. Keeping those checks spelled out in
place avoids turning hot paths into repeated helper calls in non-optimized
builds, which are exactly the builds maintainers run during development.

So the policy is:

- In hot wrapper paths, prefer explicit local code over tiny helper factoring.
- Accept small repetition when it removes call overhead risk in debug builds.
- Put the full rationale here once, and reference this anchor from headers
  instead of repeating the explanation.

This is a performance-and-predictability choice, not a style preference.

---

## Things that look like bugs but are not

If you are tempted to "fix" one of these, it was deliberate:

| Looks wrong | Why it is that way |
|---|---|
| Signalling failure is a statement, with no `make_failure(...)` expression | An expression form has to end in a comma operator, which strips `0` of its null-pointer-constant status and makes `return` an int-to-pointer conversion. That is what forced the old `-Wint-conversion` suppression. Deleting it is what lets a C build of `needful.h` leave your diagnostics alone ([detail](/result#no-make-failure)) |
| `needful_is_none` has doubled parentheses | Suppresses "suggest parentheses around assignment used as truth value" -- callers pass assignments by design |
| `assert_not_none` evaluates into a local first | `NEEDFUL_ASSERT` compiles away under `NDEBUG`; putting `_expr_` inside it would silently drop the caller's assignment |
| `panic_if_failed` is not `abort_if_failed` | A failure carries an error payload to report; a disengaged `Option` has nothing to report and can only `NEEDFUL_ABORT()`. A shared prefix would promise a shared exit path that does not exist |
| The GNU must-use spelling is preferred over `[[nodiscard]]` | `__attribute__` may appear anywhere among declaration specifiers; `[[nodiscard]]` must lead. That is the difference between `static Fallible(int*) f(void)` compiling and not ([detail](/fallible#the-static-restriction)) |
| Prefix keywords use `+`, extraction uses `%` | They must interlock at adjacent precedence levels; `+` and `%` are the only pair that satisfies every constraint ([detail](/precedence)) |
| `Option(T)` converts to `Fallible(T)` implicitly, but the reverse needs `fallible` | They share the `none` state but not its meaning: for `Option` absence is a valid answer, for `Fallible` it's a failure. Promoting absence to failure only adds an obligation; demoting failure to absence removes one, so it must be visible -- the same reason `Result(T)` can't silently become `Option(T)` ([detail](/fallible#option-conversion)) |
| `unwrap` is rejected on a `Need(T)` (use `needed`) | If `unwrap` worked on both, then with only one wrapper enabled the other's type is a plain `T`, so `unwrap` would have to accept any plain `T`, losing the check that you only `unwrap` optionals. Owned by `Option(T)` alone, the check holds whenever `Option` is wrapped. `Need(T)` converts implicitly anyway, so `needed` is rarely required |

---

## Before you commit a change to `needful.h`

1. **Build all three modes.** The test suite covers them; `ctest` is not
   optional here, because mode 2 has no other guard.

2. **Run the C matrix.** `needful.h` must compile as plain C at `c11`, `c17`,
   and latest -- with and without `NEEDFUL_FALLIBLE_C23_MUSTUSE`.

3. **Diff the macro inventory.** If you moved sections around, extract every
   `#define` name before and after and compare the sets. A reorganization that
   silently drops a macro will still compile.

4. **Re-run the doctests.** Docs changes are code changes here. Note that if
   the extraction step cannot find Python it fails *quietly*, and the suite
   shrinks from 40-odd tests to 16 without saying so -- check the count.

5. **Mind the three lints**, which cover conventions no compiler enforces:
   `comment-style` (needful.h's own comments are slash-star; `//` only *inside*
   them), `doc-links` (every `Docs:` URL and every `](/page#anchor)` resolves),
   and `no-tabs`. They are ordinary CTest entries, so a full run catches them --
   but they are the ones most likely to be broken by an edit that "obviously"
   could not break anything.

6. **Sync the header** to wherever it is published, and verify the copy. A
   shared-folder copy once produced a file of exactly the right length whose
   tail was NUL padding; `tests/run-header-integrity.cmake` exists because of
   it.

---

## Related

- **`OPEN-QUESTIONS.md`** (repo root, not published) -- the things that are
  *not* settled. Check it before changing something that looks arbitrary: it
  may be arbitrary on purpose, pending a decision. Record a position there
  rather than enacting it.

- [FAQ](/faq) -- design rationale for individual constructs
- [Precedence](/precedence) -- why the prefix keywords parse the way they do
- [Setup](/setup) -- the full configuration switch inventory
- [Threading](/threading) -- thread model for Result hooks and corruption state
- [C++ Modernization Notes](/cpp-modernization) -- what C++14/17/20 would
  simplify if the baseline ever changes
