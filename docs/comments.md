---
layout: default
title: Comment Macros
nav_order: 9
permalink: /comments
---

# Comment Macros — Executable Documentation

Needful provides a family of macros that replace prose comments with
compile-checked expressions. In C, they are all no-ops. In C++ builds, they
verify that the expression inside is well-formed — so if you rename a
variable, the comment breaks at compile time instead of silently going stale.

## The Problem With Prose Comments

```c
int i = Get_Integer(...);  // i may be < 0
```

If `i` is later renamed, the comment becomes wrong silently. The expression
`i < 0` is not checked by the compiler.

## The Needful Alternative

```c
int i = Get_Integer(...);
possibly(i < 0);   // if i is renamed, this becomes a compile error
```

`possibly()` is a no-op at runtime — it compiles away completely. But in C++
builds, it static-asserts that the expression has a type convertible to
`bool`. If `i` is renamed and the expression no longer compiles, CI catches it.

## Statement-Scope Macros

These go inside function bodies:

| Macro | Meaning | Constraint |
|---|---|---|
| `possibly(cond)` | This *might* be true | `cond` must be bool-convertible |
| `definitely(cond)` | This is *always* true (not worth asserting at runtime) | `cond` must be bool-convertible |
| `impossible(cond)` | This can *never* be true | `cond` must be bool-convertible |
| `unnecessary(expr)` | This code would be redundant or pointless here | `expr` must be valid |
| `dont(expr)` | You might think you need to do this, but it's wrong! | `expr` must be valid |
| `cant(expr)` | Would like to do this; current limitations prevent it | `expr` must be valid |
| `heeded(expr)` | This looks stray but its side effect is intentional | expression is evaluated |

## Global-Scope Macros

For use outside function bodies (at file or namespace scope). They are
uppercased and have slightly different expansion:

| Macro | Meaning |
|---|---|
| `POSSIBLY(cond)` | No-op at global scope |
| `DEFINITELY(cond)` | Static assertion that `cond` is true |
| `IMPOSSIBLE(cond)` | Static assertion that `cond` is false |
| `UNNECESSARY(expr)` | No-op at global scope |
| `DONT(expr)` | No-op at global scope |
| `CANT(expr)` | No-op at global scope |

## `STATIC_ASSERT` and Friends

```c
STATIC_ASSERT(sizeof(int) == 4);          // C++ build: compile error if false
STATIC_ASSERT_LVALUE(variable);           // error if variable is not an lvalue
STATIC_IGNORE(expr);                      // validate expression, discard result
STATIC_FAIL(some_identifier_message);     // always fails (marks unreachable paths)
```

## Example: Self-Documenting Loop

```c
uint32_t calculate_sum_weirdly(uint8_t* arr, int len) {
    uint32_t sum = 0;
    while (len > 0) {
        possibly(arr[len - 1] == 0);    // some elements may not change sum
        sum += arr[--len];
    }
    definitely(len == 0);               // we decremented past 0
    return sum;
}
```

## Related

- [FAQ: What's the difference between `definitely` and `STATIC_ASSERT`?](/faq#definitely-vs-static-assert)

---

## Compile-Time Tests

### Comment macros compile and are no-ops at runtime

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_COMMMENT_SHORTHANDS  1
#include "needful.h"

int find_first_nonzero(int* arr, int len) {
    for (int i = 0; i < len; ++i) {
        possibly(arr[i] == 0);  // some elements may be zero
        if (arr[i] != 0)
            return arr[i];
    }
    impossible(len > 0);  // only reached when all are zero
    return 0;
}

int main() {
    int a[] = {0, 0, 7, 3};
    assert(find_first_nonzero(a, 4) == 7);
    int b[] = {0, 0};
    assert(find_first_nonzero(b, 2) == 0);
    return 0;
}
```

### `possibly()` requires a bool-convertible expression

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: must be explicitly convertible to bool  <- needful static_assert
// MATCH-ERROR-TEXT: static assertion failed                 <- GCC/Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_COMMENT_SHORTHANDS  1
#include "needful.h"

struct NotBool { int x; };

int main() {
    NotBool nb = {5};
    possibly(nb);  // ERROR: NotBool has no operator bool
    return 0;
}
```
