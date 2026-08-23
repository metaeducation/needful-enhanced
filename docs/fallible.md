---
layout: default
title: Fallible(T)
nav_order: 4
permalink: /fallible
---

# `Fallible(T)` — Checked Nullable Value (Must-Use)

`Fallible(T)` is an explicitly nullable wrapper that forces the client to handle
the potential absence of a value. It shares the same zero-overhead footprint as
`Option(T)`, but structurally enforces a **`[[nodiscard]]`** contract when built
as C++.

If a function can return a null state but has no extended error payload to
propagate, `Fallible(T)` provides a lightweight alternative to `Result(T)` while
remaining safer than a raw pointer or standard `Option(T)`.

## The Pattern

Instead of returning a raw nullable pointer or an optionally ignored wrapper:

```c
// Caller can drop this on the floor or assign directly without checking
Option(int*) Find_Node(const char* name);

// Caller MUST route the assignment through a validation macro
NeedfulFallible(int*) Find_Node_Checked(const char* name);
```

You force deterministic unpacking at the call site:

```c
int* node;

// OK: Macro validates the state and unwraps the inner type
return_if_nullptr (
    node = Find_Node_Checked("target")
);

// ERROR: Cannot implicitly assign a Fallible(T) to T* without an unwrap step
int* bypassed = Find_Node_Checked("target");
```

## Unpacking Vocabulary

Because `Fallible(T)` carries an implicit must-use constraint, you cannot access
the underlying value without routing it through one of the validation macros.
These macros strip the static analysis layer down to the underlying `T`.

| Macro | Meaning |
|---|---|
| `return_if_nullptr(assignment)` | Extracts value; if `nullptr`, exits early with `needful_none` |
| `abort_if_nullptr(assignment)` | Extracts value; if `nullptr`, aborts execution immediately |
| `tolerate_if_nullptr(assignment)` | Explicitly acknowledges that dropping the null state here is intentional |

## Mechanics: The Postfix Modulus Trick

In pure C, `Fallible(T)` transparently falls back to `T` with compiler-level
`warn_unused_result` attributes.

In C++ enhanced mode, `Fallible(T)` expands to a custom `[[nodiscard]]` wrapper.
To keep macro syntax clean and prevent errors during inline assignment, the
framework uses operator precedence. The validation macros inject a postfix
extractor token via the modulus operator (`%`):

```c
#define needful_is_nullptr(_expr_) \
    ((_expr_ needful_postfix_extract_option) == nullptr)
```

Because `%` has higher precedence than assignment (`=`) but lower precedence
than a function call, the macro safely intercepts the `Fallible(T)` return value,
peels off the static analysis wrapper, performs the nullptr check, and allows the
underlying raw type to cleanly assign to the local variable.

## Related

- [`Option(T)`](/option) — nullable values that are legal to drop/ignore
- [`Result(T)`](/result) — for functions that multiplex a full error object with a return value

---

## Compile-Time Tests

### Basic macro unpacking paths

<!-- doctest: positive-test -->
```cpp
#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_NULLPTR_SHORTHANDS  1
#include "needful.h"

NeedfulFallible(int*) get_data(bool success) {
    static int payload = 42;
    if (!success) return nullptr;
    return &payload;
}

// `return_if_nullptr` expands to `return needful_none;`, so it may only be
// used inside a function whose return type accepts `none` -- an Option(T) or
// a Result(None).  It is NOT usable in main(), or in any int-returning
// function: `none` is a distinct type in checked builds, not a plain zero.
//
NeedfulOption(int*) fetch_or_none(bool success) {
    int* value;
    return_if_nullptr (value = get_data(success));  // early-out on nullptr
    return value;
}

int main() {
    int* value = nullptr;

    // 1. tolerate path: dropping the null state here is deliberate
    tolerate_if_nullptr (value = get_data(false));
    assert(value == nullptr);

    // 2. early-return path, exercised through an Option-returning function
    assert(fetch_or_none(false) == nullptr);

    NeedfulOption(int*) got = fetch_or_none(true);
    assert(got != nullptr);
    assert(*(needful_unwrap got) == 42);

    return 0;
}
```

### Discarding a `Fallible(T)` is a compile error

<!-- doctest: negative-test -->
```cpp
// REQUIRES-STD: c++17
//
// MATCH-ERROR-TEXT: nodiscard               <- all three
// MATCH-ERROR-TEXT: ignoring return value   <- GCC, Clang
// MATCH-ERROR-TEXT: discarding return value <- MSVC (C4834)
//
// NOTE: this test needs TWO things that are easy to miss.
//
// 1. C++17.  NEEDFUL_NODISCARD is a no-op before then, because [[nodiscard]]
//    does not exist -- so below C++17 the must-use property of Fallible(T)
//    genuinely is not enforced, and this test cannot fail.  Hence
//    REQUIRES-STD above, which makes the harness skip rather than misreport.
//
// 2. Warnings-as-errors.  [[nodiscard]] produces a *warning*; the compiler
//    still exits 0.  The negative-test harness builds with -Werror / /WX so
//    that discarding a value is an actual failure.  See tests/CMakeLists.txt.

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"

NeedfulFallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    get_resource();  // ERROR: [[nodiscard]] triggered, must handle value
    return 0;
}
```

### Direct assignment to raw type without a macro is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"

NeedfulFallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    // ERROR: Fallible(int*) cannot implicitly decay to int*
    int* raw = get_resource();
    NEEDFUL_UNUSED(raw);
    return 0;
}
```
