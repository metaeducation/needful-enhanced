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

| Cast | Use For |
|---|---|
| `cast(T, expr)` | Safe default; runs debug-build validation hooks |
| `raw_cast(T, expr)` | Unhooked; data not yet valid (fresh `malloc()`, uninitialized) |
| `fast_cast(T, expr)` | Unhooked; data is valid but hook overhead is unacceptable on hot path |
| `m_cast(T, expr)` | Remove const: `const T*` → `T*` |
| `i_cast(T, expr)` | Integer-to-integer: `enum E` ↔ `int` |
| `ii_cast(T, expr)` | Unwrap `Option(int)` to `int` (optimized) |
| `p_cast(T, expr)` | Pointer ↔ `intptr_t` |
| `f_cast(T, expr)` | Function pointer to function pointer |
| `v_cast(T, expr)` | `va_list*` ↔ `void*` |
| `c_cast(T, expr)` | fallback; exact C semantics, stands out better than plain parentheses |

All are no-ops in C builds — they expand to `(T)(expr)`.

## Why `cast()` Runs Hooks

The most natural question a newcomer asks is: *why does the plainest name run
validation, when the built-in C cast does nothing?*

The answer is the **pit of success**: the easiest thing to type should be the
safest thing to do.  People reach for `cast` without thinking.  If that got
them raw, unchecked behavior, they would have to remember to opt *in* to
safety everywhere.  Instead, opting *out* requires an explicit, named choice:

- **`raw_cast`** — "I know this memory isn't valid yet (fresh allocation)."
- **`fast_cast`** — "I know it's valid, but I can't afford hooks on this path."
- **`c_cast`** — "I need exact C semantics and have thought about why."

Each name is a declaration of intent that stands out in code review.  An
auditor can `grep` for `raw_cast` and `fast_cast` to find every place that
skips validation, and evaluate whether each has a good reason.  If `cast` were
the raw one, every cast would look the same regardless of whether the author
had thought about it.

There is also a **long-term maintenance argument**.  As a codebase grows,
`cast()` calls accumulate far ahead of the people writing CastHook
specializations for new types.  Making `cast` hookable means those
specializations can be added incrementally, and all existing call sites
immediately benefit — no sweep of the codebase required.  If `cast` were raw,
adding a new validation rule would require auditing every cast to decide which
ones should have been `h_cast` all along.

Finally, this **reflects what the word means**.  Naming the hooked macro
`h_cast` and leaving plain `cast` as the synonym for "I didn't bother" would
teach the wrong lesson: that instrumentation is opt-in, exceptional, for
special cases only.  In Needful, instrumentation is the norm.  The special
cases are the ones that opt out.

### Gradual Adoption

If you are adding Needful to an existing codebase incrementally, it can help
to start by redefining `cast` as `raw_cast` — so the first pass is just
making old-style parentheses casts visible without changing behaviour:

```c
#define cast(T, expr)  raw_cast(T, expr)  // temporary: migration in progress
```

Once every call site is named, you can tighten them one by one: leave the
frequent/hot-path ones as `raw_cast` or `fast_cast`, and flip the rest to
plain `cast` once you are confident they are casting already-valid data.  When
the migration is complete, remove the override and let `cast` mean what it
always should have.

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

In C++ builds with `NEEDFUL_CPP_ENHANCED`, `cast()` dispatches through a
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

## `v_cast` — `va_list` ↔ `void*`

`va_list` is implementation-defined in a way that makes regular casts
unsafe across platforms. On GCC/Linux x86-64 (and others), `va_list` is a
typedef for a fixed-size *array* type. Arrays in C decay to a pointer to
their first element — so `(void*)ap` when `ap` is an array-based `va_list`
gives you a pointer *into* the array, not a pointer *to the `va_list` object
itself*. When the receiving function casts that `void*` back to `va_list*`,
it gets a `va_list*` pointing at an element, not at the `va_list` — and
code that works correctly on struct-based platforms fails silently on
array-based ones.

The fix is to always pass `&ap` (pointer to the `va_list` object) rather
than `ap` itself, and to cast *that* to `void*`:

```c
void process_args(void* vp) {
    va_list* ap = v_cast(va_list*, vp);
    // use *ap with va_arg...
}

void call_it(int first, ...) {
    va_list ap;
    va_start(ap, first);
    process_args(v_cast(void*, &ap));  // &ap, not ap
    va_end(ap);
}
```

`v_cast` enforces this in C++ builds: only `va_list* ↔ void*` is
permitted (not bare `va_list`). Needful includes `<stdarg.h>` to pull in
the `va_list` definition; suppress this with `NEEDFUL_DONT_INCLUDE_STDARG_H`.

## `c_cast_known` — Combined Cast and Type Assertion

`c_cast_known(T, expr)` is a combined form of `c_cast(T, known(T, expr))`
that sidesteps a C preprocessor limitation: when `T` contains commas (as
C++ template types do), the macro parser splits on those commas before it
knows they are part of a type name:

```c
// Fails: preprocessor sees 3 arguments to known(), not 2
cast(std::pair<int, char>*, known(std::pair<int, char>*, expr))

// Works: single macro call; T is the first argument verbatim
c_cast_known(std::pair<int, char>*, expr)
```

Lenient and rigid variants mirror the behavior of the base cast:

| Variant | Constness behavior |
|---|---|
| `c_cast_known(T, expr)` | Lenient — alias for `lenient_c_cast_known` |
| `lenient_c_cast_known(T, expr)` | Passes `const` through |
| `rigid_c_cast_known(T, expr)` | Errors on constness mismatch |

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
    int i = cast(int, 3.7);         // narrowing: double -> int
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
