---
layout: default
title: Design Rationale
nav_order: 55
permalink: /design
---

# Why `needful.h` Is Written The Way It Is

This page is for anyone **editing** Needful — including AI assistants, who
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
| 1 | **C** | just `#include "needful.h"` | transparent — mostly nothing at all |
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
to `bool`. So `Option(SomeEnum)` is perfectly ordinary — and comparing an enum
against `(void*)0` is a constraint violation in C and an error in C++.

That alone would be a portability bug. What makes it a *three-mode* bug is
this: `needful.h` suppresses `-Wint-conversion` in C builds
([why](/faq#int-conversion-warning)). So on GCC and Clang, the enum mistake
would **compile silently as C** and fail only under enhancement — exactly the
C/C++ divergence Needful exists to prevent.

So `none` gets one spelling that means the same thing three times:

| Mode | `needful_none` expands to |
|---|---|
| C | `0` |
| C++, unenhanced | `needful::Nocast0Struct{}` |
| C++, enhanced | `needful::NoneStruct{}` |

Each of those needs comparison to work for *both* pointers and enums:

- **C** — plain `0 ==` works for pointers, enums, and integers alike.
- **C++, unenhanced** — `Nocast0Struct` has a templated conversion operator, so
  `ptr == nocast_0` would already work. **Enums would not.** That is why
  `needful.h` defines `operator==`/`!=` for `Nocast0Struct` directly. The
  trailing return type is the constraint: `-> decltype(! v)` means that if `!v`
  is not valid for `T`, substitution fails and the overload drops out.
- **C++, enhanced** — `Option(T)` supplies its own `NoneStruct` operators.

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

There is almost no legitimate middle case — and the middle is exactly where
numbered footnotes live. Hence:

**`needful.h` has no numbered footnotes.** If you find yourself writing
`[1]`, the explanation has outgrown the header. Move it here and leave a link.

Two exceptions are load-bearing and stay in the header as lettered notes,
because they must be read *before* you `#include`:

- **[A]** `-Wint-conversion` is globally disabled in C mode. A user must know
  a warning was turned off on their behalf.
- **[B]** `NEEDFUL_ASSERT` / `NEEDFUL_ABORT` hooks. Actionable configuration.

### Other header conventions

- **Part 1 is the vocabulary; Part 2 is the plumbing.** A reader never has to
  reach Part 2. This works because macro bodies expand lazily — a Part 1 macro
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
  the ones where C++ *cannot* express the C spelling at all — e.g.
  `NEEDFUL_NULLPTR`, since C++ will not implicitly convert `(void*)0` to an
  arbitrary `T*`.
- **One example, at the top, and no more.** Examples in the docs are compiled
  in CI ([see the doctest system](https://github.com/metaeducation/needful-enhanced/blob/main/docs/README.md));
  examples in the header are not, so they rot. The banner carries a single
  `Option(T)` example to convey the *sense* of the library, and that is all.

---

## Things that look like bugs but are not

If you are tempted to "fix" one of these, it was deliberate:

| Looks wrong | Why it is that way |
|---|---|
| `-Wint-conversion` globally suppressed in C | `make_failure(...)` and `none` are comma expressions, and the comma operator strips `0` of its null-pointer-constant status ([detail](/faq#int-conversion-warning)) |
| `needful_is_none` has doubled parentheses | Suppresses "suggest parentheses around assignment used as truth value" — callers pass assignments by design |
| `assert_not_none` evaluates into a local first | `NEEDFUL_ASSERT` compiles away under `NDEBUG`; putting `_expr_` inside it would silently drop the caller's assignment |
| `panic_if_failed` is not `abort_if_failed` | A failure carries an error payload to report; a disengaged `Option` has nothing to report and can only `NEEDFUL_ABORT()`. A shared prefix would promise a shared exit path that does not exist |
| The GNU must-use spelling is preferred over `[[nodiscard]]` | `__attribute__` may appear anywhere among declaration specifiers; `[[nodiscard]]` must lead. That is the difference between `static Fallible(int*) f(void)` compiling and not ([detail](/fallible#the-static-restriction)) |
| Prefix keywords use `+`, extraction uses `%` | They must interlock at adjacent precedence levels; `+` and `%` are the only pair that satisfies every constraint ([detail](/precedence)) |
| `unwrap`'s tag type lives in `needful-wrapping.hpp` | Both `Need` and `Option` supply an overload, and either file may be switched off independently. Putting it in one would make `unwrap` on the other's type depend on an unrelated toggle |

---

## Before you commit a change to `needful.h`

1. **Build all three modes.** The test suite covers them; `ctest` is not
   optional here, because mode 2 has no other guard.
2. **Run the C matrix.** `needful.h` must compile as plain C at `c11`, `c17`,
   and latest — with and without `NEEDFUL_FALLIBLE_C23_MUSTUSE`.
3. **Diff the macro inventory.** If you moved sections around, extract every
   `#define` name before and after and compare the sets. A reorganization that
   silently drops a macro will still compile.
4. **Re-run the doctests.** Docs changes are code changes here. Note that if
   the extraction step cannot find Python it fails *quietly*, and the suite
   shrinks from 40-odd tests to 16 without saying so — check the count.
5. **Sync the header** to wherever it is published, and verify the copy. A
   shared-folder copy once produced a file of exactly the right length whose
   tail was NUL padding; `tests/run-header-integrity.cmake` exists because of
   it.

---

## Related

- [FAQ](/faq) — design rationale for individual constructs
- [Precedence](/precedence) — why the prefix keywords parse the way they do
- [Setup](/setup) — the full configuration switch inventory
