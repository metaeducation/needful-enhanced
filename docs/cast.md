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

In C++ builds with `NEEDFUL_CPP_ENHANCED`, `h_cast()` dispatches through a
`CastHook<From, To>` template you partially specialize to validate data at
runtime. For example, to verify that a `Number*` really contains a `Float`
before downcasting:

```cpp
template<typename F>
struct CastHook<const F*, const Float*> {
    static void Validate_Bits(const F* p) {
        if (not p)
            return;  // null is allowed; nothing to validate
        assert(raw_cast(const Number*, p)->is_float);
    }
};
```

Hooks fire only when `NEEDFUL_CAST_CALLS_HOOKS` is defined (typically in debug
builds); release builds see zero overhead.

### Null pointer casts and the hook

Null pointer casts are passed through to the hook rather than short-circuited
in the cast machinery. This is intentional for two reasons:

**Policy belongs in the hook.** Most hooks return early on null (there are no
bits to validate), but a hook can also `crash` or assert — making null casts to
that type a hard error in debug builds. If the machinery silently skipped null,
hooks would lose that ability.

**C++11/MSVC compatibility.** Intercepting null in the generic cast machinery
requires an `if` whose condition depends on a type trait
(`std::is_pointer<T>::value`) — and MSVC raises warning C4127
("conditional expression is constant") on exactly that pattern, even when
wrapped in helper structs. With `/WX` (warnings-as-errors), that is a build
failure. The clean C++ solution would be `if constexpr`, but that requires
C++17. Working around it in C++11 while keeping the check in the machinery
means disguising the constant as a runtime value (e.g. via an overloaded
function returning `const void*`) — but that introduces a real function call
in unoptimized debug builds, precisely where cast performance matters most.
Since hooks receive a concrete pointer argument, the null check there is a
plain runtime branch: no warning, no workaround overhead.

The conventional null guard is:

```cpp
if (not p)
    return;  // null is allowed; nothing to validate
```

Omit or replace it with a hard error to make null casts illegal for that type.

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
