---
layout: default
title: Comment Macros
nav_order: 8
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

## Start Here

If you are evaluating Needful, this is the page to try first.

Everything else in this library has a peer somewhere. Rust has `Option` and
`Result`, C++17 has `[[nodiscard]]`, and every large C codebase eventually
grows its own cast wrappers. *Comments that fail the build when they go stale*
have no equivalent in any of them.

They are also the cheapest thing here to adopt. Every other construct asks you
to change a type signature and think about who owns what:

| | asks you to | breaks if you stop |
|---|---|---|
| `Option(T)`, `Need(T)`, `Fallible(T)` | change declarations, add `unwrap`/`opt` | yes — signatures changed |
| `Result(T)` | supply failure hooks, restructure returns | yes |
| Comment macros | nothing | no — no declaration ever changed |

You can add `possibly()` to one function this afternoon, in an existing file,
with no refactoring and no build changes, and roll the whole experiment back by
deleting the lines. (The one exception is `heeded()`, which really does
evaluate its expression — see the table below.) That makes them a good place to
start, and in practice the thing that gets people to try `Option(T)` a month
later.

## Statement-Scope Macros

These go inside function bodies:

| Macro | Meaning | Constraint |
|---|---|---|
| `possibly(cond)` | This *might* be true | `cond` must be bool-convertible |
| `definitely(cond)` | This is *always* true (not worth asserting at runtime) | `cond` must be bool-convertible |
| `impossible(cond)` | This can *never* be true | `cond` must be bool-convertible |
| `unnecessary(expr)` | This code would be redundant or pointless here | `expr` must be valid |
| `inapplicable(expr)` | This operation does not apply in this case | `expr` must be valid |
| `dont(expr)` | You might think you need to do this, but it's wrong! | `expr` must be valid |
| `cant(expr)` | Would like to do this; current limitations prevent it | `expr` must be valid |
| `heeded(expr)` | This looks stray but its side effect is intentional | expression is evaluated |

Note that `heeded()` is the one entry that is **not** a no-op: it expands to
`USED(expr)`, so the expression really runs. The others compile away.

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
STATIC_ASSERT(sizeof(int) == 4);       // C++ build: compile error if false
STATIC_ASSERT_LVALUE(variable);        // error if variable is not an lvalue
STATIC_IGNORE(expr);                   // validate expression, discard result
STATIC_FAIL("path is unreachable");    // always fails (marks unreachable paths)
```

`STATIC_FAIL()` takes a **string literal** in every build mode. Pre-C11 C has
no `_Static_assert`, so it falls back to a negative-size array typedef: still a
hard error, but the message itself is lost, since there is nowhere to put it.

`STATIC_ASSERT()` is enforced in C++ builds and in C11 or later. In pre-C11 C
it is a no-op, so do not rely on it as your only check of an invariant that
matters.

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
#define NEEDFUL_COMMENT_SHORTHANDS  1
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
