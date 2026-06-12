---
layout: default
title: nocast
nav_order: 9
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

## Related

- [`cast()` family](/cast) — for casts between known types
- [FAQ: Why does Needful disable the int-conversion warning?](/faq#int-conversion-warning)

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
