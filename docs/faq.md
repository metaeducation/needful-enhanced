---
layout: default
title: FAQ
nav_order: 50
permalink: /faq
---

# Frequently Asked Questions

---

## Why does Needful disable the `-Wint-conversion` warning? {#int-conversion-warning}

In C, bare `0` is a "null pointer constant" (NPC) and converts implicitly to
any pointer type. But the comma operator strips that status: `(expr, 0)` is
just an integer expression that happens to evaluate to zero. Assigning it to
a pointer is technically an int-to-pointer conversion, which GCC and Clang
flag with `-Wint-conversion`:

```c
SomeType* f(void) { return 0; }          // fine: 0 is a NPC
SomeType* f(void) { return (expr, 0); }  // warned: not a NPC
```

This matters because `make_failure(...)` expands to a comma expression ending in
`NEEDFUL_RESULT_0`, and `none` can do the same. Any function returning
`Result(SomeType*)` or `Option(SomeType*)` would trigger the warning on every
`return make_failure(...)` or `return none`.

Needful disables the warning globally in C mode. This is safe:

- The only code producing these conversions is Needful's own macros.
- The C++ enhanced build catches any real type mistakes.
- `nocast` provides the targeted int-to-pointer behavior in C++ when
  building without the full enhancements.

If you want the warning back for non-Needful code, set
`#define NEEDFUL_DISABLE_INT_WARNING 0` and add your own pragma push/pop
around call sites that use `make_failure()` or `none`.

---

## The copyright date says "2015-2026"; but I only heard of it in 2026.  Why?

The code for Needful had humble beginnings in another project.  And some of its
first "published" ideas were crudely patched into the Roaring Bitmaps project
in 2020.

Factoring the library out and producing documentation and tests for it was
facilitated by the age of AI.  But the library itself was not AI generated!
*(...though some of the trickier edge cases in the template metaprogramming
were hunted down rather impressively by Claude...)*

Now that it has been unleashed on the world, I'm hopeful that both humans and
AIs will iterate and improve upon it (especially documentation and tests!)
There's still work to do on the versioning procedures that sync `needful.h`
with the `needful-enhanced/` repository, so that's a current priority.

---

## Why does `decltype(known(T, expr))` give a reference type? {#known-decltype}

`known(T, expr)` in C++ builds expands to a comma expression:
`(dummy_check, (expr))`. The `decltype` of a local variable inside a comma
expression yields `T&` (a reference), not `T`.

This means patterns like:

```cpp
STATIC_ASSERT_SAME(decltype(known(int*, ptr)), int*);  // FAILS: gets int*&
```

...will fail unexpectedly. Instead, check types via assignment to a
typed variable:

```cpp
int* check = known(int*, ptr);   // compile error if types don't match
(void)check;
```

---

## What's the difference between `definitely` and `STATIC_ASSERT`? {#definitely-vs-static-assert}

`definitely(cond)` is a statement-scope macro for documenting programmer
beliefs at runtime. It compiles to a no-op (it does *not* assert at runtime —
use a real `assert()` if you want a runtime check). The C++ build verifies the
expression is bool-convertible and well-formed.

`STATIC_ASSERT(cond)` is a compile-time assertion. It fires immediately during
compilation if `cond` is false. Use it for things that are true by construction
(e.g. `sizeof` checks, type properties).

```c
definitely(i >= 0);                    // documents a belief; no runtime cost
STATIC_ASSERT(sizeof(int) == 4);       // fails at compile time if wrong
```

---

## Why is there no `nocast_0` shorthand (only `needful_nocast_0`)? {#nocast-0-no-shorthand}

`nocast_0` is plumbing — it's used internally by `make_failure(...)` and `none`
to produce a generic zero. Exposing it as a short name would imply it's a
common user-facing API, which it isn't. Users working with options and results
should use `none` (for `Option` and `Fallible`) or `make_failure(...)` (for
`Result`).

The asymmetry with `nocast` (which *does* have an unprefixed shorthand) is
intentional: `nocast` appears at user call sites regularly (e.g.
`nocast malloc(...)`), while `needful_nocast_0` almost never does.

---

## Why can't `return_if_failed` also work on a `Fallible(T)`? {#result-fallible-unification}

Because it would be *silently* wrong, not merely difficult — and that is a
sharper reason than it first appears.

The obstacle is not a missing trick. In the C build the two are already
indistinguishable: `NEEDFUL_RESULT_0` and `none` are both `needful_nocast_0`,
which is plain `0`. And in C++ you could easily give `ResultWrapper<T>` a
constructor taking `NoneStruct` and be done.

The obstacle is that **the two macros test different things.**
`return_if_failed` consults the thread-global failure state, which a
`Fallible(T)` never sets. `return_if_none` consults the returned value itself,
which a `Result(T)` never uses to signal. A merged macro would have to dispatch
on the operand's type to know which of the two to consult — and in the C build
there is no type to dispatch on, because both are transparent macros over `T`.

So the unification is impossible in C. Since Needful's whole contract is that
the C and C++ builds agree about what your program means, it must therefore not
exist in C++ either.

Consider what the failure would look like if the rule were relaxed. Write
`return_if_failed (x = some_fallible_call())` and it compiles cleanly, finds
the failure global unset, declines to return, and carries on with a disengaged
`x`. No diagnostic, in either build. Keeping the two vocabularies visibly
distinct — `failed` for the error channel, `none` for the value — is what makes
that mistake hard to write in the first place.

See [`Result(T)`](/result#fallible-relationship) for the side-by-side table of
the two vocabularies.

---

## Why is there no `STATIC_ASSERT` shim for pre-C11 C? {#static-assert-c-noop}

Needful uses C11's official `_Static_assert(cond, "message")` automatically
when compiling as C11 or newer.

The problem is pre-C11 C: there is no portable, reliable replacement. Older
tricks — such as `typedef int sa[-1]` on false conditions — only work at
file or function scope, not inside expressions, and produce cryptic errors.
Compiler and standard edge cases are too numerous to paper over cleanly.

So Needful intentionally does not provide a pre-C11 shim. In pre-C11 C,
`STATIC_ASSERT` is a no-op; in C11+ it maps to `_Static_assert`; and the C++
enhanced build enforces it as well. Run the C++ build in CI if you need
strong compile-time checking on older C toolchains.

---

## How do I use Needful in a multi-threaded program? {#thread-safety}

The `Result(T)` error state uses a global variable in the
`NEEDFUL_DECLARE_RESULT_HOOKS` built-in implementation. For multi-threaded use,
provide your own hook implementations using `thread_local` storage
(C11 `_Thread_local` or C++11 `thread_local`):

```c
static _Thread_local const char* tls_failure;

const char* Needful_Test_And_Clear_Failure(void) {
    const char* e = tls_failure;
    tls_failure = nullptr;
    return e;
}
// ... other hooks ...
```

---

## How should I configure clang-format for Needful syntax? {#clang-format-needful}

Needful uses operator-like tokens such as `downcast` and `nocast`, and often
ships `needful.h` as a vendored third-party file. A default clang-format setup
may reflow these in ways that are technically formatted but semantically less
readable.

Recommended setup:

- Treat Needful operator-like tokens as type names so unary `*` keeps
  cast-like spacing in expression contexts, instead of looking like
  multiplication.
- If you use `downcast cast(...)->member`, include `cast` in the same list so
  clang-format does not split the arrow spacing.
- Keep this list narrow (operator-like Needful tokens only). Avoid adding
  ordinary function names globally.
- For one-off expressions that still confuse the formatter, parenthesize the
  operand explicitly, e.g. `downcast (foo(...)->member)`.
- Exclude `needful.h` from formatting if that file is vendored and should
  remain upstream-identical.

Example `.clang-format` fragment:

```yaml
TypeNames:
  - nocast
  - downcast
  - raw_downcast
```

Example `.clang-format-ignore` fragment:

```text
.*xxx/include/needful.h
.*xxx/include/needful-enhanced/.*
```

Note: clang-format itself does not automatically apply `.gitignore` rules.
