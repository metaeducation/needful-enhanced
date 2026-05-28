---
layout: default
title: cast() Family
nav_order: 5
permalink: /cast
---

# `cast()` Family — Visible, Hookable Casts

Needful replaces invisible parenthesized casts with named, semantically
differentiated macros. You can spot them in code, they communicate intent,
and in C++ builds they can run validation hooks.

## Quick Selection Guide

```
PRO-TIP: #define cast as h_cast in your codebase!
```

| Cast | Use For |
|---|---|
| `h_cast(T, expr)` | Safe default; runs debug-build validation hooks |
| `raw_cast(T, expr)` | Unhooked; data not yet valid (fresh `malloc()`, uninitialized) |
| `fast_cast(T, expr)` | Unhooked; data is valid but hook overhead is unacceptable on hot path |
| `m_cast(T, expr)` | Remove const: `const T*` → `T*` |
| `i_cast(T, expr)` | Integer-to-integer: `enum E` ↔ `int` |
| `ii_cast(T, expr)` | Unwrap `Option(int)` to `int` (optimized) |
| `p_cast(T, expr)` | Pointer ↔ `intptr_t` |
| `f_cast(T, expr)` | Function pointer to function pointer |
| `v_cast(T, expr)` | `va_list*` ↔ `void*` |
| `x_cast(T, expr)` | "Xtreme" fallback; exact C semantics |

All are no-ops in C builds — they expand to `(T)(expr)`.

## Constness Behavior

By default the casts are *lenient*: if you cast `const T*` to `U*`, you get
`const U*` back rather than an error. This makes casts less verbose:

```c
void f(const Base* base) {
    const Derived* d = cast(Derived*, base);  // not const Derived*
    // C++ build gives you const Derived* and doesn't complain
}
```

*Rigid* variants (`needful_rigid_hookable_cast`, etc.) error if you omit const.

## Hooks

In C++ builds with `NEEDFUL_CPP_ENHANCED`, `h_cast()` calls a hook
function you define that can validate the data at runtime:

```cpp
template<typename To, typename From>
To NeedfulHookableCast(From from) {
    // e.g. check that From* really contains a valid To object
    return static_cast<To>(from);
}
```

Hooks fire only in debug builds; release builds see zero overhead.

## Related

- [nocast](/nocast) — bridge `void*` and enum zero without a cast
- [`downcast` / `upcast`](/cast#downcast) — inheritance-aware casting

---

## Compile-Time Tests

### Basic cast usage

<!-- doctest: positive-test -->
```cpp
#define NEEDFUL_CPP_ENHANCED  1
#include <cassert>
#include "needful.h"

struct Base    { int x; };
struct Derived { int x; int y; };

int main() {
    int i = h_cast(int, 3.7);         // narrowing: double -> int
    assert(i == 3);

    const int ci = 5;
    const int* cp = &ci;
    // m_cast removes const; must cast to Base type or same type
    int* mp = m_cast(int*, cp);
    assert(*mp == 5);

    return 0;
}
```

### `m_cast` to an unrelated type is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: upcast() cannot implicitly convert
#define NEEDFUL_CPP_ENHANCED  1
#include <cassert>
#include "needful.h"

int main() {
    struct Base    {};
    struct Derived : Base {};
    const Base* cb = nullptr;
    Derived* d = m_cast(Derived*, cb);  // ERROR: invalid m_cast downcast
    NEEDFUL_UNUSED(d);
    return 0;
}
```
