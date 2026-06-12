---
layout: default
title: known(T, expr)
nav_order: 7
permalink: /known
---

# `known(T, expr)` — Cheap Compile-Time Type Assertion

`known(T, expr)` asserts at compile time that `expr` is of type `T`, then
passes the value through unchanged. It is a zero-cost alternative to inline
functions for type-checking inside macros.

```c
int* ptr = get_ptr();
void* p = known(int*, ptr);    // OK: ptr is int*

char* wrong = get_char();
void* p = known(int*, wrong);  // ERROR (C++ build): type mismatch
```

In C, `known(T, expr)` is `(expr)` — a no-op. The C++ build uses a
`static_assert` that fires if the types don't match.

## Why Not an Inline Function?

Three reasons:

1. **Debug build cost.** Inline functions are not inlined in debug builds.
   `known()` is zero cost even in `-O0`.
2. **Const propagation.** A function must pick `const T*` or `T*`. `known()`
   passes through whatever constness the expression has (lenient) or errors
   on mismatch (rigid).
3. **Macro composability.** Writing a macro that delegates to a function for
   type checking forces split definitions. `known()` stays inside the macro.

## Lenient vs. Rigid

| Variant | Behavior |
|---|---|
| `lenient_known(T, expr)` | Accepts `const T` when `T` is specified; passes const through |
| `rigid_known(T, expr)` | Errors if `expr` has different const-ness than `T` |
| `known(T, expr)` | Alias for `lenient_known` |

```c
const int* cp = get_const();
int* mp = get_mutable();

lenient_known(int*, cp);   // OK: accepts const int* when int* specified
rigid_known(int*, cp);     // ERROR: cp is const int*, not int*
rigid_known(int*, mp);     // OK: exact match
```

## `known_not` and `known_any`

```c
known_not(int*, expr)               // asserts expr is NOT int*
known_any((int*, char*, void*), expr)  // asserts expr is one of the listed types
```

## `lenient_exactly` and `rigid_exactly`

These check that `expr` is exactly a specified type (no inheritance or
implicit conversions). `lenient_exactly(T, expr)` accepts `const T` as a
match; `rigid_exactly(T, expr)` requires the exact type including mutability.

## `known_lvalue`

```c
known_lvalue(variable)   // compile error if variable is not an lvalue
```

Useful in "evil macros" that use their arguments more than once: ensures the
caller passes a simple variable, not an expression with side effects.

## Related

- [FAQ: Why does `decltype(known(...))` give a reference type?](/faq#known-decltype)
- [`cast()` family](/cast) — type-changing operations; `known()` is type-asserting

---

## Compile-Time Tests

### Basic type assertion

<!-- doctest: positive-test -->
```cpp
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_KNOWN_SHORTHANDS  1
#include "needful.h"

int main() {
    int value = 10;
    int* ptr = &value;

    void* p = lenient_known(int*, ptr);  // OK: ptr is int*
    NEEDFUL_UNUSED(p);

    const int* cp = ptr;
    const void* q = lenient_known(int*, cp);   // OK: passes through const int*
    NEEDFUL_UNUSED(q);

    return 0;
}
```

### `rigid_known` rejects a const pointer where mutable is required

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: static assertion failed  <- GCC/Clang
// MATCH-ERROR-TEXT: static_assert            <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_KNOWN_SHORTHANDS  1
#include "needful.h"

int main() {
    const int* cp = nullptr;
    int* p = rigid_known(int*, cp);  // ERROR: const int* != int*
    NEEDFUL_UNUSED(p);
    return 0;
}
```
