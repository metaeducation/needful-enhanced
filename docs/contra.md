---
layout: default
title: "Contra(T) / Sink(T) / Init(T)"
nav_order: 9
permalink: /contra
---

# `Contra(T)`, `Sink(T)`, `Init(T)` — Contravariant Output Parameters

## The Core Idea: [Contravariance][variance-wiki]

While C builds don't have inheritance available, it's possible for a C++
enhanced build to conditionally declare an inheritance relationship to a
struct (as long as it doesn't add any fields!)  Needful can leverage this
relationship.

```cpp
struct Base { int bits; };

#if NEEDFUL_CPP_ENHANCED  // declare inheritance if using C++ enhanced mode
  struct Derived : Base {};  // no extra fields — same layout
#else
  typedef Base Derived;  // Derived identical to Base in C version
#endif
```

When a function writes *into* a parameter (output), the type safety rules
run backwards compared to input parameters. This is known as
[**contravariance**][variance-wiki].

For inputs, covariance applies: a function accepting `Base*` can accept
`Derived*` — passing something more specific where something less specific
is expected is safe.

For outputs, the reverse is true. A function that *writes* `Base` bits into a
slot must be given a `Base`-typed slot. Giving it a `Derived`-typed slot is
**unsafe**: the writer knows only `Base`'s layout, and may not fill the fields
that `Derived` adds.

```
Input  (covariant):     Derived* → Base*   ✓  more specific is acceptable
Output (contravariant): Derived* → Sink(Base)  ✗  more specific is REJECTED
```

This reversal is what `Sink(T)`, `Init(T)`, and `Contra(T)` enforce in C++
builds.

## The Three Wrappers: A Gradient

| Type | Use case | Corruption? | Debug Build Cost |
|---|---|---|---|
| `Contra(T)` | In/out, or pass-through: the existing value must survive | **Never**, in any build | Zero |
| `Sink(T)` | Write-only output; reader must not look at old value | Yes, deferred | Small |
| `Init(T)` | Full initialization; old value is always overwritten | Off by default (see below) | Zero |

In C, all three expand to `T*`. The C++ wrappers enforce contravariance and
optionally scramble memory in debug builds.

### `Contra(T)` — Contravariance Without Corruption

`Contra(T)` is the one to reach for when you want the contravariant type rule
but the slot's **existing value must survive**. It is not merely "the cheap
one" — the absence of corruption is the point, and it holds in *every* build,
regardless of `NEEDFUL_DOES_CORRUPTIONS`.

Two situations call for it:

**An in/out parameter.** The function reads what is there and updates it, so
scrambling the slot first would destroy the input:

```c
void accumulate(Contra(Base) io);   // reads io, then writes it back
```

**A pass-through.** The function receives an output location and hands it to
someone else without writing through it directly:

```c
void dispatch(Contra(Base) out);    // just forwarding — no direct write
```

Both are relatively rare compared to `Sink(T)` and `Init(T)`, which is why the
type exists as a deliberate opt-out rather than a default. If you find yourself
wanting `Sink(T)` semantics but with corruption disabled, `Contra(T)` is the
declaration you are looking for — say it in the signature rather than turning
the switch off globally.

### `Sink(T)` — Write-Only Output with Corruption

Use `Sink(T)` to mark output parameters where the caller must not rely on
the old contents. When `NEEDFUL_DOES_CORRUPTIONS` is enabled, the
`SinkWrapper` scrambles the pointed-to memory with `0xBD` bytes at the moment
the pointer is first used — so any code that reads from the output slot before
writing will see garbage and crash loudly.

The corruption is *deferred* to first use (not on construction) to avoid
corrupting another argument's value before it is evaluated:

```c
// If corruption happened at construction, `ptr` would be corrupt
// before being evaluated as the second argument:
if (some_function(&ptr, ptr)) { ... }
```

```c
struct Base { int bits; };

void Write_Base(Sink(Base) out) {
    Base* p = out;   // corruption fires here
    p->bits = 99;
}
```

### `Init(T)` — Full Initialization

Use `Init(T)` for functions that promise to fill every field of the output.
`Init` is semantically identical to `Sink` except it skips corruption —
there's no point scrambling bytes that will all be overwritten anyway. This
gives `Init` zero overhead even with `NEEDFUL_DOES_CORRUPTIONS` on.

To enable corruption for `Init` as well (for intensive debugging sessions):

```c
#define NEEDFUL_INIT_CORRUPTS_LIKE_SINK  1
```

This makes every `Init(T)` behave exactly as a `Sink(T)`. It is worth turning
on when you suspect an `Init_Xxx()` routine is *not* filling every field it
claims to — the promise `Init(T)` encodes is precisely the one this switch
stops taking on trust. Expect it to be slower, and expect it to find things.

Note the asymmetry this creates, and that it is intentional:

| | corrupts by default | corrupts under the switch |
|---|---|---|
| `Sink(T)` | yes | yes |
| `Init(T)` | no | **yes** |
| `Contra(T)` | no | **no** |

`Contra(T)` is the only one of the three that never corrupts under any
configuration, which is what makes it safe for in/out parameters.

## The Type Rules

For `Sink(T)`, the accepted pointer types are:
- `T*` — identity (always safe)
- `U*` where `U` is a same-layout **base class** (supertype) of `T` —
  meaning `T` derives from `U` and `sizeof(U) == sizeof(T)`

That second case is the surprising one. If `Derived : Base {}` with no
extra fields, you can call `Sink(Derived)` with a `Base*`. The function will
write a full `Derived`-worth of bits into the base-typed slot, and because the
layouts are identical, this is safe.

The rejected case is the intuitive-but-wrong one: `Derived*` → `Sink(Base)`.
Even with same sizeof, `Base` is more general than `Derived`. A function that
promises to write `Base` bits does not know about any `Derived` invariants,
and the caller's `Derived` object should not be handed off as though it were
a plain `Base` output target.

In other words, the direction of the relationship is flipped relative to
input parameters:

```
Input  (covariant):    Derived* → Base*        ✓  more specific in
Output (contravariant): Base*   → Sink(Derived) ✓  more general in
```

`Exact(T)` is the escape hatch when you want precisely `T*` with no
covariance or contravariance at all.

## Why Standard-Layout Matters

Needful's contravariance is grounded on C struct inheritance: all types in
a hierarchy must be `standard_layout` and have the same `sizeof`. This is
the only guarantee that makes a bit-for-bit write into a `Base` slot safe
when the underlying memory is allocated as a `Derived` object with no extra
fields.

The C++ build checks these properties via `static_assert` inside
`IsSameLayoutBase`.

## Indirect Encodings

Some types store an encoding pointer (getter/setter) rather than a plain value.
Writing blindly into such a slot corrupts the encoding. Needful's
`MayUseIndirectEncoding` trait marks these types, and `IsContravariant`
refuses them as `Sink`/`Init` targets unless they are themselves wrapped —
where the wrapper guarantees safe writes.

## Related

- [`Need(T)`](/need) — non-null pointers (orthogonal concern)

---

## Compile-Time Tests

### `Sink(Derived)` accepts `Base*` — the contravariant direction

A base-typed pointer can be passed where a more-specific Sink is expected,
because the writer fills exactly the same bytes (same-sizeof inheritance).

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_CONTRA_SHORTHANDS  1
#include "needful.h"

struct BaseStruct { int bits; };
typedef struct BaseStruct Base;

#if NEEDFUL_CPP_ENHANCED
  struct Derived : Base {};  // no extra fields — same layout
#else
  typedef Base Derived;  // Derived identical to Base if not enhanced
#endif

NEEDFUL_STATIC_ASSERT(sizeof(Base) == sizeof(Derived));  // same layout

void init_derived(Sink(Derived) out) {
    Derived* p = out;
    p->bits = 99;
}

int main() {
    Base b = {};
    init_derived(&b);  // Base* accepted: base is a supertype of Derived
    assert(b.bits == 99);
    return 0;
}
```

### `Sink(Base)` rejects `Derived*` — wrong direction, even with same layout

Even though `Derived` adds no fields (same sizeof), it is a *subtype* of
`Base`, not a supertype. Passing it as `Sink(Base)` is rejected.

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: could not convert                    <- GCC
// MATCH-ERROR-TEXT: no matching function                 <- GCC alternate
// MATCH-ERROR-TEXT: no instance of overloaded function   <- MSVC alternate
// MATCH-ERROR-TEXT: no viable constructor                <- Clang
// MATCH-ERROR-TEXT: cannot convert argument 1 from 'Derived *'   <- MSVC
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_CONTRA_SHORTHANDS  1
#include <cassert>
#include "needful.h"

struct Base { int bits; };

#if NEEDFUL_CPP_ENHANCED
  struct Derived : Base {};  // no extra fields — same layout
#else
  typedef Base Derived;  // Derived identical to Base if not enhanced
#endif

NEEDFUL_STATIC_ASSERT(sizeof(Base) == sizeof(Derived));  // same layout

void write_base(Sink(Base) out) {
    Base* p = out;
    p->bits = 1;
}

int main() {
    Derived d = {};
    write_base(&d);  // ERROR: Sink(Base) must not accept Derived*
    return 0;
}
```

[variance-wiki]: https://en.wikipedia.org/wiki/Type_variance
