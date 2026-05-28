---
layout: default
title: FAQ
nav_order: 10
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

This matters because `fail(...)` expands to a comma expression ending in
`NEEDFUL_RESULT_0`, and `none` can do the same. Any function returning
`Result(SomeType*)` or `Option(SomeType*)` would trigger the warning on every
`return fail(...)` or `return none`.

Needful disables the warning globally in C mode. This is safe:

- The only code producing these conversions is Needful's own macros.
- The C++ enhanced build catches any real type mistakes.
- `nocast` provides the targeted int-to-pointer behavior in C++ when
  building without the full enhancements.

If you want the warning back for non-Needful code, set
`#define NEEDFUL_DISABLE_INT_WARNING 0` and add your own pragma push/pop
around call sites that use `fail()` or `none`.

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

`nocast_0` is plumbing — it's used internally by `fail(...)` and `none` to
produce a generic zero. Exposing it as a short name would imply it's a
common user-facing API, which it isn't. Users working with options and results
should use `none` (for `Option`) or `fail(...)` (for `Result`).

The asymmetry with `nocast` (which *does* have an unprefixed shorthand) is
intentional: `nocast` appears at user call sites regularly (e.g.
`nocast malloc(...)`), while `needful_nocast_0` almost never does.

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
