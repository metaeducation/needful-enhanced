---
layout: default
title: Fallible(T)
nav_order: 4
permalink: /fallible
---

# `Fallible(T)` -- Checked Nullable Value (Must-Use)

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

## `Fallible(T)` is for return types; `FallibleVar(T)` is for everything else

```c
Fallible(int*) Find_Node(const char* name);      // return type

FallibleVar(int*) cached;                        // local, parameter, member
static int* Use(FallibleVar(int*) node) { ... }
```

They are **the same type**. They differ only in which declaration positions
they are legal in, and only in C.

The reason is how the must-use annotation behaves. In C, `Fallible(T)` carries
one -- `__attribute__((warn_unused_result))`, or C23's `[[nodiscard]]` -- and an
annotation that means "this function's result must be used" is only meaningful
on a function. Applied anywhere else, the compiler objects:

| position | what happens |
|---|---|
| return type | accepted -- the intended use |
| local, global, parameter, struct member | diagnosed (warning; an error under `-Werror` / `/WX`) |
| `typedef`, cast | hard error |

That is a **feature**, not an obstacle. It means `Fallible(T)` polices its own
position for free: a value that must be checked cannot be quietly parked in a
struct field where nothing will ever check it. But legitimate locals and
parameters do exist -- a helper that receives one, a cached result -- and they
need a spelling with no annotation on it. That is `FallibleVar(T)`.

Under `NEEDFUL_CPP_ENHANCED` there is nothing to omit: `[[nodiscard]]` rides on
the wrapper class rather than on the declaration, so no position is special and
both spellings expand identically. Keeping both defined in every mode is what
lets one source file compile as C, as C++, and as checked C++.

### The `static` restriction {#the-static-restriction}

`__attribute__` may appear anywhere among a declaration's specifiers.
`[[nodiscard]]` must come *first*. So on a compiler offering only the C23
spelling, this is a syntax error:

```c
static Fallible(int*) helper(void);   // ERROR under the C23-only path
```

and no reordering rescues it -- `Fallible(int*) static helper(void)` fails too.
Needful therefore prefers the GNU spelling wherever it exists (GCC and Clang
offer it at *every* C standard, no C23 needed), and leaves the C23-only path
**opt-in** behind `NEEDFUL_FALLIBLE_C23_MUSTUSE`. A compiler upgrade should not
spring a hard error on every `static` helper in a codebase.

If you enable it, drop the `static` from fallible-returning functions, or give
them internal linkage another way.

## Which builds actually check this {#enforcement}

`Fallible(T)` makes three separate promises, and **no single configuration
delivers all three.** Which ones you get depends on the compiler, the language
standard, and -- critically -- on whether you turned warnings into errors.

| build | must-use | position | won't decay to `T` |
|---|---|---|---|
| C -- GCC/Clang, **any** standard | ✅ | ✅ | -- |
| C -- MSVC `/std:c11`, `c17` | ✗ | ✗ | -- |
| C -- C23 + `NEEDFUL_FALLIBLE_C23_MUSTUSE` | ✅ | ✅ | -- |
| C++ -- no enhancement, GCC/Clang | ✅ | ✅ | -- |
| C++ -- no enhancement, MSVC C++17+ | ✅ | ✅ | -- |
| C++ -- `NEEDFUL_CPP_ENHANCED`, C++11/14 | ✗ | n/a | ✅ |
| C++ -- enhanced, C++11/14 + `NEEDFUL_PRECPP17_NODISCARD` | ✅ | n/a | ✅ |
| C++ -- `NEEDFUL_CPP_ENHANCED`, **C++17+** | ✅ | n/a | ✅ |

Three things in that table are worth stopping on.

**The enhanced build is not strictly stronger.** At C++11/14 it enforces the
type separation but *not* the must-use property, because `NEEDFUL_NODISCARD`
has no *standard* spelling before C++17 -- so a plain C build under GCC catches
a dropped `Fallible(T)` that the checked C++11 build lets through. If you run
the enhanced build in CI, run it at **C++17 or higher**, or set
`-DNEEDFUL_PRECPP17_NODISCARD=1`, which takes compilers up on accepting
`[[nodiscard]]` earlier than required and silences the resulting
`-Wc++17-extensions` pedantry. See
[the discussion in `Result(T)`](/result#no-c-mustuse) -- the switch is shared by
both constructs. Otherwise you have turned off a guarantee you had for free.

**Position checking does not apply under enhancement**, and does not need to:
there, `[[nodiscard]]` rides on the wrapper class rather than the declaration,
so `Fallible(T)` and `FallibleVar(T)` are the same type and every position is
legal. The positional discipline is a C-build property.

**MSVC in C mode below C23 checks nothing at all.** `_Check_return_` looks like
it should help and does not -- it is a SAL annotation, inert outside `/analyze`.
If MSVC-C is your only build, the enhanced C++ build is where your enforcement
has to come from.

### Warnings must be errors

Almost everything above arrives as a **warning**. A build that does not
escalate warnings gets a diagnostic scrolled past in a log, not enforcement:

| toolchain | minimum to see it | to make it fail |
|---|---|---|
| GCC / Clang | on by default (`-Wunused-result`, `-Wattributes`) | `-Werror`, or `-Werror=unused-result -Werror=attributes` |
| MSVC | `/W1` for C4834 (discard), `/W3` for C4869 (position) | `/WX`, or `/we4834 /we4869` |

The two hard errors -- `Fallible(T)` in a `typedef` or a cast -- are the only
diagnostics here that fail a build on their own.

MSVC needs no `/Zc:__cplusplus`: the C++17 detection reads `_MSVC_LANG`
directly.

## Discharge Vocabulary {#discharge-vocabulary}

Because `Fallible(T)` carries an implicit must-use constraint, you cannot reach
the underlying value without routing it through one of these. Each names a
different decision about the disengaged state, which is the point -- the type
makes you say which one you meant.

| Macro | Meaning |
|---|---|
| `return_if_none(assignment)` | Extracts value; if none, exits early returning `none` |
| `abort_if_none(assignment)` | Extracts value; if none, calls `NEEDFUL_ABORT()` |
| `assert_not_none(assignment)` | Extracts value; asserts in debug that it is engaged |
| `tolerate_none(assignment)` | Acknowledges that dropping the none state here is deliberate |
| `is_none(expr)` | Plain predicate, for when you want to branch yourself |
| `infallible expr` | Expression-position discharge; asserts engaged, yields `T` |
| `fallible expr` | Converts to `Option(T)`; a failure becomes an ordinary `none` |

`return_if_none` expands to `return needful_none`, so it may only appear in a
function whose return type accepts `none` -- an `Option(T)` or a `Result(None)`.

Note that `assert_not_none` and `tolerate_none` are **not** the same macro
wearing two names. The first claims the value cannot be none and checks that
claim in debug builds; the second admits it may well be none and says you have
decided not to care. Older versions of Needful only had the second, spelled
`tolerate_if_nullptr`, and it was quietly doing both jobs.

### Why `none` and not `nullptr` {#the-none-comparison}

These used to be spelled `return_if_nullptr`, `abort_if_nullptr`, and
`tolerate_if_nullptr`, and they tested with `== nullptr`. That was wrong in a
way worth recording, because the bug it caused is exactly the kind Needful
exists to catch.

`Fallible(T)` is built on `Option(T)`, which requires only that `T` be
explicitly convertible to `bool`. A `Fallible(SomeEnum)` is perfectly ordinary
-- and comparing an enum against `(void*)0` is a constraint violation in C and a
hard error in C++. Worse: `needful.h` used to globally suppress
`-Wint-conversion` in C builds, so on GCC and Clang that mistake compiled
**silently** as C and blew up only under C++ enhancement. A C/C++ divergence,
inside Needful's own vocabulary.

That suppression has since been [removed](/faq#int-conversion-warning) -- this
bug is a large part of why. A regression here now fails the C build too.

Testing against `none` fixes it, because `none` means "disengaged" for every
`T` that `Option(T)` accepts, not just pointers.

## `Fallible(T)` and `Option(T)` {#option-conversion}

The two share the same missing state -- `none`, which is `T`'s falsey/0 state
-- but they don't mean the same thing by it. For an `Option(T)`, absence is a
legitimate answer ("not found" is as valid as "found"). For a `Fallible(T)`,
absence means the operation **failed**: it's a `Result(T)` with no error
payload.

So the conversions are asymmetric:

| Conversion | Meaning | Spelling |
|---|---|---|
| `Option(T)` => `Fallible(T)` | "for this function, absence is a failure" | implicit |
| `Fallible(T)` => `Option(T)` | "this failure is just an ordinary absence" | `fallible expr` |

The first only *adds* an obligation, so it needs no ceremony:

```c
Fallible(Node*) Find_Node_Checked(const char* name) {
    return Find_Node(name);  // Option(Node*) => Fallible(Node*)
}
```

The second removes one, which is the kind of thing that should be visible at
the call site -- the same reason a `Result(T)` can't silently become an
`Option(T)`:

```c
Option(Node*) node = fallible Find_Node_Checked(name);  // failure => none
```

`fallible` is a compile error on a plain `Option(T)`, so it can't become a
habit that is harmless everywhere and means nothing anywhere. In C it is a
no-op, like the other prefix keywords.

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

Because `->*` binds tighter than assignment (`=`) but looser than a function
call, the macro intercepts the `Fallible(T)` return value, peels off the
wrapper, and lets the underlying raw type assign cleanly to the local variable
-- all before the comparison happens.

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

- [`Option(T)`](/option) -- nullable values that are legal to drop/ignore
- [`Result(T)`](/result) -- for functions that multiplex a full error object with a return value

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

### `Option(T)` and `Fallible(T)` conversions

Everything that is allowed: `Option(T)` becomes `Fallible(T)` implicitly
(returned, initialized, or assigned), `fallible` goes the other way, and the
shared `none` state survives both directions--for enums as well as pointers.

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
  #include <type_traits>
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

typedef enum { COLOR_0, COLOR_RED } Color;  // 0 state is none

static int value = 1020;

Option(int*) find_value(int ok) {
    return ok ? &value : nullptr;
}

Fallible(int*) find_value_checked(int ok) {
    return find_value(ok);  // Option(T) => Fallible(T): return
}

Fallible(const int*) find_const_checked(int ok) {
    return find_value_checked(ok);  // Fallible(T) => Fallible(U) still fine
}

Option(Color) find_color(int ok) {
    if (ok)
        return COLOR_RED;
    return none;
}

Fallible(Color) find_color_checked(int ok) {
    return find_color(ok);
}

Fallible(int*) always_fails(void) {
    return none;
}

int take_option(Option(int*) p) {
    return p ? 1 : 0;
}

Option(int*) find_value_tolerant(int ok) {
    return fallible find_value_checked(ok);  // Fallible(T) => Option(T)
}

int main() {
    Option(int*) found = fallible find_value_checked(1);
    assert(found == &value);

    Option(int*) missing = fallible find_value_checked(0);
    assert(missing == none);  // failure arrives as the same `none` state

    Option(int*) failed = fallible always_fails();
    assert(failed == none);

    Option(const int*) found_const = fallible find_const_checked(1);
    assert(found_const == &value);

    assert(take_option(fallible find_value_checked(1)) == 1);
    assert(find_value_tolerant(0) == none);

    FallibleVar(int*) var = find_value(1);  // Option(T) => Fallible(T): init
    assert(var == &value);
    var = find_value(0);  // Option(T) => Fallible(T): assignment
    assert(var == none);

    Option(Color) color = fallible find_color_checked(1);
    assert(color == COLOR_RED);
    Option(Color) no_color = fallible find_color_checked(0);
    assert(no_color == none);

  #if defined(__cplusplus) && NEEDFUL_CPP_ENHANCED
    static_assert(  // `fallible` gives back a plain Option(T), not a Fallible
        std::is_same<
            decltype(fallible find_value_checked(1)), Option(int*)
        >::value,
        "fallible must produce Option(T)"
    );
  #endif

    return 0;
}
```

### `Fallible(T)` => `Option(T)` without `fallible` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    Option(int*) maybe = get_resource();  // ERROR: needs `fallible`
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```

### `fallible` on an `Option(T)` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: `fallible` is only for Fallible(T)

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Option(int*) get_maybe(void) {
    return nullptr;
}

int main() {
    Option(int*) maybe = fallible get_maybe();  // ERROR: already an Option
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```

### Assigning a `Fallible(T)` to an `Option(T)` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    Option(int*) maybe = none;
    maybe = get_resource();  // ERROR: needs `fallible`
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```

### Passing a `Fallible(T)` as an `Option(T)` argument is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

void take_option(Option(int*) p) {
    NEEDFUL_UNUSED(p);
}

int main() {
    take_option(get_resource());  // ERROR: needs `fallible`
    return 0;
}
```

### Returning a `Fallible(T)` as an `Option(T)` is a compile error

This is the sneakiest leak: the function's caller would get an `Option(T)` it
is free to ignore, with no trace that the value came from something fallible.

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

Option(int*) pass_along(void) {
    return get_resource();  // ERROR: needs `fallible`
}

int main() {
    Option(int*) maybe = pass_along();
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```

### An explicit C++ cast from `Fallible(T)` to `Option(T)` is a compile error

`fallible` is the one way to spell this, so it can be searched for.

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    Option(int*) maybe = static_cast<Option(int*)>(get_resource());  // ERROR
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```

### `fallible` on a plain value is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: no match for
// MATCH-ERROR-TEXT: invalid operands to binary expression
// MATCH-ERROR-TEXT: does not define this operator

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_FALLIBLE_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

Fallible(int*) get_resource(void) {
    return nullptr;
}

int main() {
    int* raw = nullptr;
    Option(int*) maybe = fallible raw;  // ERROR: not a Fallible(T)
    NEEDFUL_UNUSED(maybe);
    return 0;
}
```
