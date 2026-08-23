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

// Caller MUST route the assignment through a discharge macro
Fallible(int*) Find_Node_Checked(const char* name);
```

You force deterministic unpacking at the call site:

```c
int* node;

// OK: macro validates the state and unwraps the inner type
return_if_none (
    node = Find_Node_Checked("target")
);

// ERROR: Cannot implicitly assign a Fallible(T) to T* without an unwrap step
int* bypassed = Find_Node_Checked("target");
```

## Discharge Vocabulary

Because `Fallible(T)` carries an implicit must-use constraint, you cannot reach
the underlying value without routing it through one of these. Each names a
different decision about the disengaged state, which is the point — the type
makes you say which one you meant.

| Macro | Meaning |
|---|---|
| `return_if_none(assignment)` | Extracts value; if none, exits early returning `none` |
| `abort_if_none(assignment)` | Extracts value; if none, calls `NEEDFUL_ABORT()` |
| `assert_not_none(assignment)` | Extracts value; asserts in debug that it is engaged |
| `tolerate_none(assignment)` | Acknowledges that dropping the none state here is deliberate |
| `is_none(expr)` | Plain predicate, for when you want to branch yourself |
| `infallible expr` | Expression-position discharge; asserts engaged, yields `T` |

`return_if_none` expands to `return needful_none`, so it may only appear in a
function whose return type accepts `none` — an `Option(T)` or a `Result(None)`.

Note that `assert_not_none` and `tolerate_none` are **not** the same macro
wearing two names. The first claims the value cannot be none and checks that
claim in debug builds; the second admits it may well be none and says you have
decided not to care. Older versions of Needful only had the second, spelled
`tolerate_if_nullptr`, and it was quietly doing both jobs.

### Why `none` and not `nullptr`

These used to be spelled `return_if_nullptr`, `abort_if_nullptr`, and
`tolerate_if_nullptr`, and they tested with `== nullptr`. That was wrong in a
way worth recording, because the bug it caused is exactly the kind Needful
exists to catch.

`Fallible(T)` is built on `Option(T)`, which requires only that `T` be
explicitly convertible to `bool`. A `Fallible(SomeEnum)` is perfectly ordinary
— and comparing an enum against `(void*)0` is a constraint violation in C and a
hard error in C++. Worse: `needful.h` globally suppresses `-Wint-conversion` in
C builds (see the [FAQ](/faq#int-conversion-warning)), so on GCC and Clang that
mistake compiled **silently** as C and blew up only under C++ enhancement. A
C/C++ divergence, inside Needful's own vocabulary.

Testing against `none` fixes it, because `none` means "disengaged" for every
`T` that `Option(T)` accepts, not just pointers.

## Mechanics: The Postfix Modulus Trick

In pure C, `Fallible(T)` transparently falls back to `T`, and `none` is plain
`0`. In C++ enhanced mode, `Fallible(T)` expands to a `[[nodiscard]]` wrapper
deriving from `OptionWrapper<T>`.

To keep macro syntax clean and prevent errors during inline assignment, the
discharge macros inject a postfix extractor token via the modulus operator:

```c
#define needful_is_none(_expr_) \
    (((_expr_ needful_postfix_extract_option)) == needful_none)
```

Because `%` binds tighter than assignment (`=`) but looser than a function
call, the macro intercepts the `Fallible(T)` return value, peels off the
wrapper, and lets the underlying raw type assign cleanly to the local variable
— all before the comparison happens.

That last detail is why the comparison is against `none` rather than against
the wrapper: by the time it runs, the extractor has already removed the
wrapper, so the left-hand side is a raw `T`. `Option(T)` therefore supplies
`operator==` overloads for `NoneStruct` covering both the wrapped and unwrapped
cases, and `needful.h` supplies matching ones for `Nocast0Struct` so that the
same spelling works when building as C++ *without* the enhanced files.

Comparing against `none` works directly too, which is often what you want:

```c
Option(int*) node = Find_Node("target");
if (node == none)
    return none;
```

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
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#define NEEDFUL_NULLPTR_SHIM  1
#include "needful.h"

Fallible(int*) get_data(bool success) {
    static int payload = 42;
    if (!success) return nullptr;
    return &payload;
}

// `return_if_none` expands to `return needful_none;`, so it may only be used
// inside a function whose return type accepts `none` -- an Option(T) or a
// Result(None).  It is NOT usable in main(), or in any int-returning
// function: `none` is a distinct type in checked builds, not a plain zero.
//
Option(int*) fetch_or_none(bool success) {
    int* value;
    return_if_none (value = get_data(success));  // early-out when disengaged
    return value;
}

// The same vocabulary on a non-pointer T.  This is the case the older
// *_if_nullptr spelling could not express at all.
//
typedef enum { COLOR_0 = 0, COLOR_RED = 1 } Color;

Fallible(Color) pick_color(bool success) {
    return success ? COLOR_RED : COLOR_0;
}

Option(Color) fetch_color(bool success) {
    Color c;
    return_if_none (c = pick_color(success));
    return c;
}

int main() {
    int* value = nullptr;

    // 1. tolerate path: dropping the none state here is deliberate
    tolerate_none (value = get_data(false));
    assert(value == nullptr);

    // 2. assert path: claiming it cannot be none, checked in debug builds.
    //    Note the assignment still happens under NDEBUG.
    assert_not_none (value = get_data(true));
    assert(*value == 42);

    // 3. expression-position discharge
    int* direct = infallible get_data(true);
    assert(*direct == 42);

    // 4. early-return path, exercised through an Option-returning function
    assert(fetch_or_none(false) == nullptr);

    Option(int*) got = fetch_or_none(true);
    assert(got != none);
    assert(*(unwrap got) == 42);

    // 5. `none` reads as well as it writes
    assert(get_data(false) == none);
    assert(get_data(true) != none);
    assert(is_none(get_data(false)));

    // 6. all of the above on an enum, not just a pointer
    assert(fetch_color(true) == COLOR_RED);
    assert(fetch_color(false) == none);
    assert(pick_color(false) == none);

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
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_NULLPTR_SHIM  1
#include "needful.h"

Fallible(int*) get_resource(void) {
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
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_NULLPTR_SHIM  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    // ERROR: Fallible(int*) cannot implicitly decay to int*
    int* raw = get_resource();
    NEEDFUL_UNUSED(raw);
    return 0;
}
```
