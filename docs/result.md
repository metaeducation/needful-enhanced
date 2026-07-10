---
layout: default
title: Result(T)
nav_order: 4
permalink: /result
---

# `Result(T)` — Cooperative Error Propagation

`Result(T)` multiplexes an error and a return value using thread-local state,
giving C code a Rust-inspired error handling style without exceptions or
`setjmp`/`longjmp`.

## The Pattern

Instead of this:

```c
Error* Some_Func(int* result, int x) {
    if (x < 304)
        return fail("the value is too small");
    *result = x + 20;
    return nullptr;
}

Error* Other_Func(int* result) {
    int y;
    Error* e = Some_Func(&y, 1000);
    if (e) return e;
    // ...manual propagation repeated for every call...
}
```

You write this:

```c
Result(int) Some_Func(int x) {
    if (x < 304)
        return fail("the value is too small");
    return x + 20;
}

Result(int) Other_Func(void) {
    trap (int y = Some_Func(1000));
    assert(y == 1020);

    trap (int z = Some_Func(10));   // auto-propagates on error
    return z;
}
```

## Error Handling Vocabulary

| Macro | Meaning |
|---|---|
| `return fail(...)` | Return a failure, storing the error in thread-local state |
| `trap (stmt)` | Execute `stmt`; if it failed, propagate the failure upward |
| `require (stmt)` | Like `trap`, but panics rather than propagating |
| `assume (stmt)` | Execute `stmt`; assert no failure occurred (debug) |
| `except (Error* e) { }` | Catch a failure into a scoped variable |
| `rescue (expr)` | Evaluate `expr` and return the failure (or null if none) |
| `panic(...)` | Abort immediately; never returns |

A quick and dirty way to write `return failed;` and not have to come up with
an error might be useful in some codebases.  We don't try to define that here,
because it's open ended as to what you'd use for your error value type.

In order for these macros to work, they need to be able to test and clear
the global error state...as well as a flag as to whether the failure is
divergent or not.  Hence you have to define:

    ErrorType* Needful_Test_And_Clear_Failure()
    ErrorType* Needful_Get_Failure()
    void Needful_Set_Failure(ErrorType* error)
    void Needful_Panic_Abruptly()
    void Needful_Assert_Not_Failing()  // avoids assert() dependency

These can be functions or macros with the same signature.  They should use
thread-local state if they're to work in multi-threaded code.


## The `except` Syntax

The most ergonomic pattern — `except` attaches naturally to an expression and
allows an `else` clause:

```c
int result = Some_Func(30) except (Error* e) {
    printf("caught: %s\n", e->message);
    result = -1;
} else {
    printf("success!\n");
}
```

This is standard C99. `except` expands into a `for` loop that runs exactly
once, scoping the error variable to the block.

It bears some explanation that the trick to get except() to be able to take an
else() clause involves a for loop that runs exactly once.  It accomplishes
this using the C99 feature allowing you do declare multiple variables scoped
to a for loop *if* they are of the same type.  If we assume your error type is
a pointer, then we can declare both the error variable and a dummy pointer
`_once` in the loop, and use a pointer increment to ensure the loop only runs
once.  :-)

### How the scoping trick works

C99 allows a `for` loop to declare multiple variables in its init clause *if
they are the same type*. Since the error variable is a pointer, a dummy
`_once` sentinel of the same pointer type can be co-declared:

```c
for (Error* e = Needful_Get_Failure(), *_once = nullptr; !_once; ++_once)
    if (Needful_Test_And_Clear_Failure())
        /* { error body } */
    else
        /* { success body } */
```

`_once` starts as `nullptr`, so `!_once` is true and the body runs once.
`++_once` makes it non-null and the loop exits. This scopes `e` to the
loop body exactly like a normal `if` block — while the `if`/`else`
structure leaves the `else` clause free for the success case.

## Setup: Result Hooks

`Result(T)` needs to know how to store, retrieve, and clear the thread-local
error state. You provide this by defining hook functions or macros:

```c
ErrorType* Needful_Test_And_Clear_Failure(void);
ErrorType* Needful_Get_Failure(void);
void       Needful_Set_Failure(ErrorType* error);
void       Needful_Panic_Abruptly(ErrorType* error);
void       Needful_Assert_Not_Failing(void);
```

For quick prototyping, define `NEEDFUL_DECLARE_RESULT_HOOKS 1` **before**
including `needful.h` to get a built-in `const char*`-based implementation:

```c
#define NEEDFUL_DECLARE_RESULT_HOOKS 1
#include "needful.h"
```

> **Note:** `NEEDFUL_DECLARE_RESULT_HOOKS` defines storage and implementations
> inline. Only define it in **one** translation unit per program.

## `Result(None)` — Fallible Functions With No Return Value

C does not allow `return value;` in a `void`-returning function, which means
`Result(void)` can't work: you can't write `return fail(...)` because there's
nothing legal to return. `None` is a unit type defined precisely to fill this
gap:

```c
Result(None) Do_Something(void) {
    if (some_condition)
        return fail("something went wrong");
    return none;  // success: return the unit value
}
```

`Result(None)` expands to `None` (an enum with a single zero value) in C
builds, and to a wrapper in C++ builds.  The C++ version ensures the compiler
checks that you handled the result rather than silently discarding it.

> **Background:** Proposals to allow `return` of void expressions in C++ have
> been rejected (see [P0146R0](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0146r0.html)).
> `None` is Needful's practical workaround.

## Related

- [`Option(T)`](/option) — nullable values without an error
- [FAQ: Why does `fail(...)` disable the int-conversion warning?](/faq#int-conversion-warning)

---

## Compile-Time Tests

### Basic `trap` / `fail` / `assume` usage

<!-- doctest: positive-test -->
```cpp
#include "assert.h"

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#define NEEDFUL_DECLARE_RESULT_HOOKS  1  // use some simple default hooks
#include "needful.h"

Result(int) double_if_positive(int x) {
    if (x < 0)
        return fail("negative input");
    return x * 2;
}

Result(int) run(void) {
    trap (int a = double_if_positive(10));
    assert(a == 20);
    trap (int b = double_if_positive(5));
    assert(b == 10);
    return 42;
}

int main() {
    assume (run());
    return 0;
}
```

### Discarding a `Result(T)` without handling it is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert   <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang
// MATCH-ERROR-TEXT: cannot initialize  <- GCC alternate

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#define NEEDFUL_DECLARE_RESULT_HOOKS  1  // use some simple default hooks
#include "needful.h"

Result(int) compute(int x) {
    if (x < 0)
        return fail("negative");
    return x * 2;
}

int main() {
    int n = compute(5);  // ERROR: Result(int) is not implicitly int
    NEEDFUL_UNUSED(n);
    return 0;
}
```
