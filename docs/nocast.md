---
layout: default
title: nocast
nav_order: 10
permalink: /nocast
---

# `nocast` — Frictionless C-to-C++ Bridge Cast

`nocast` solves two specific incompatibilities between C and C++ that arise
constantly when you try to compile C code as C++:

1. **`void*` assignment.** C allows `void*` → any pointer implicitly.
   C++ does not.
2. **Enum zero.** C allows `SomeEnum e = 0`. C++ does not.

Without `nocast`, solving these requires either ugly double-casts or extra
wrapper macros. With it:

```c
SomeType* ptr = nocast malloc(sizeof(SomeType));   // C: no-op; C++: proxy cast
SomeEnum  e   = nocast 0;                          // C: no-op; C++: constexpr zero
```

In C, `nocast` expands to nothing. In C++ it expands to a proxy object and an
`operator+` overload that performs the right conversion at the call site.

## `needful_nocast_0` — Generic Zero

`needful_nocast_0` is a constexpr zero that converts to any type. It is used
internally by `Option(T)` and `Result(T)` as a sentinel:

```c
Option(int*)   p = needful_nocast_0;   // same as none / nullptr
Result(int)    r = needful_nocast_0;   // used internally by fail(...)
```

> Note: Unlike `nocast`, there is **no unprefixed shorthand** for `nocast_0`.
> Use `needful_nocast_0` or use `none` (which is aliased to it for option/result
> contexts). This asymmetry is intentional: `needful_nocast_0` is plumbing, not
> a typical user-facing API.

## `downcast` and `raw_downcast`

For C codebases that model inheritance, `downcast` casts a base pointer to a
derived pointer. In C it expands via `void*`; in C++ it uses `nocast` to
accomplish the same without warnings:

```c
Derived* d = downcast base_ptr;   // base -> derived (trust the programmer)
```

`raw_downcast` is the unhooked version, suitable to use on fresh allocations.

## `needful_struct_0` — Generic Struct Zero

While `needful_nocast_0` handles scalars, pointers, and enums, C does not
allow anonymous braces `{ 0 }` to be used as a raw expression in a `return`
statement; they are only valid during variable initialization.

`needful_struct_0` bridges this gap. In C++ it leverages a templated conversion
operator to guarantee recursive zero-initialization of trivial aggregates.
In C, it expands to a layout compatible with variable initializers:

```c
// Valid initialization in both C and C++
MyStruct state = needful_struct_0;
```

Note: To return a zeroed struct from a function in plain C, a compound literal
specifying the type name is mandatory (e.g., return (MyStruct){ 0 };). For this
reason, core mechanics like `needful_unreachable_struct(T)` require you to
pass the type name explicitly to satisfy C's strict return constraints.

## Related

- [`cast()` family](/cast) — for casts between known types
- [FAQ: Why does Needful disable the int-conversion warning?](/faq#int-conversion-warning)

## Why not `#define needful_nocast_0 {}` in C++?

Because `{}` means value-initialization, and that is not what this macro is
for. For fundamental types it yields zero; for class types it calls the default
constructor.  That makes the macro depend on the target type's construction
rules instead of on a simple, generic none/zero conversion.

Needful keeps those things separate. `Option(T)` locals should behave like
C locals: no forced zeroing, no hidden initialization, no fake safety.
`needful_nocast_0` exists only for the cases where you explicitly need a
universal sentinel that can stand in for 0, nullptr, or a disengaged state.

Also, `{}` is too slippery in C++.  It can mean value-initialization, aggregate
initialization, list initialization, constructor selection, or just "the
compiler will figure it out," depending on context.  That makes it a poor fit
for a macro whose job is to mean one very specific thing: produce a generic
none/zero sentinel.

Nocast0Struct is explicit and unambiguous. It says exactly what it is, it
routes through the intended conversion machinery, and it avoids all the snakey
little special cases attached to `{}`. That keeps `needful_nocast_0` readable
in templates, macros, and overload-heavy code, where brace syntax can be
surprisingly indirect.

So the rule is simple: `{}` is a constructor syntax, not a semantic sentinel.
`Nocast0Struct` is the sentinel.

## `nocast` implementation notes

`nocast` is one of the few Needful constructs that needs a distinct definition
in C++ from C, even when you're not using NEEDFUL_CPP_ENHANCED.  Hence there
is conditional code on defined(__cplusplus) even in `needful.h`.

1. NocastConvert: two cases need special handling.  `static_cast` already
   handles (void* -> T*), (int -> enum), and same-type conversions.  The
   exception: (int -> T*) must substitute nullptr (C++ forbids implicit
   int-to-pointer conversion, even for literal 0).  Also, pointer-to-pointer
   casts (e.g. Derived** -> Base**) fail with static_cast because C++ doesn't
   support covariant multi-level pointer conversions; a C-style cast replicates
   C's behavior.

2. The choice of `+` as the operator to use is intentional due to wanting
   something with lower precedence than `%` (used in Result and Optional)

---

## Compile-Time Tests

### `nocast` bridges `void*` and enum zero

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_CAST_SHORTHANDS  1
#include "needful.h"

typedef enum { RED = 0, GREEN = 1, BLUE = 2 } Color;

int main() {
    // void* -> T*: C allows implicitly; C++ requires cast
    void* raw = nullptr;
    int* p = nocast raw;
    assert(p == nullptr);

    // enum from zero: C allows; C++ does not
    Color c = nocast 0;
    assert(c == RED);

    // int* from zero: portable across C and C++
    int* np = nocast 0;
    assert(np == nullptr);

    return 0;
}
```
