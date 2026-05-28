---
layout: default
title: "Internals: The Template Cast Operator Problem"
nav_order: 20
permalink: /internals/template-cast-operator
---

# Internals: The Template Cast Operator Problem

*This article explains a recurring implementation quirk in Needful's wrapper
types: why certain conversions use the idiom `c_cast(T*, c_cast(void*, u))`
instead of a direct cast, and what goes wrong if you try the obvious
alternatives.*

---

## The Setup

Needful's C++ enhancement mode wraps raw pointers in types like
`ContraWrapper<T*>`, `SinkWrapper<T*>`, and `OptionWrapper<T>` to get
compile-time enforcement of type constraints.

These wrappers need to convert back to raw pointers frequently — but they
live inside *template constructors* that accept a family of compatible types:

```cpp
template<typename U, IfContravariant<U, T>* = nullptr>
ContraWrapper(const U& u) {
    this->p = c_cast(T*, c_cast(void*, u));  // ← the idiom in question
}
```

Here `U` might be `Derived*`, or `SinkWrapper<Derived>`, or
`ContraWrapper<Derived>` — anything that `IfContravariant` has certified is
layout-compatible with `T*`. The job is to get `T*` out of it.

Why not just write `static_cast<T*>(u)`?

---

## What Breaks With `static_cast<T*>(u)` Directly

### Case 1: `U` is a raw pointer (`Derived*`)

```cpp
// T = Base, U = Derived*, u is Derived*
static_cast<Base*>(u);   // works fine — Derived* -> Base* is standard upcast
```

This case actually works. `static_cast` handles upcasts along inheritance
hierarchies correctly.

### Case 2: `U` is a wrapper type (`SinkWrapper<Derived>`)

```cpp
// T = Base, U = SinkWrapper<Derived>, u is SinkWrapper<Derived>
static_cast<Base*>(u);   // does NOT work
```

`SinkWrapper<Derived>` has this conversion operator:

```cpp
operator T*() const { ... }   // T is Derived here — yields Derived*
```

But there is no `operator Base*()`. So `static_cast<Base*>(SinkWrapper<Derived>)` finds no conversion and fails.

The wrapper *does* have a templated explicit conversion:

```cpp
template<typename U>
explicit operator U*() const {
    return const_cast<U*>(reinterpret_cast<const U*>(p));
}
```

`static_cast` *can* invoke explicit conversion operators, so
`static_cast<Base*>(u)` would call `explicit operator Base*()` — but look
what that does: it `reinterpret_cast`s `Derived*` to `const Base*` then
strips const. `reinterpret_cast` between pointer types does **not** adjust for
base class offsets. For single inheritance with no added fields (Needful's
requirement for contravariant types) this happens to produce the same address
— but it bypasses the language's semantic guarantees entirely. It's also
wrong in principle and misleading to future readers.

---

## Why Going Through `void*` Is Correct

The idiom:

```cpp
this->p = c_cast(T*, c_cast(void*, u));
```

breaks into two steps:

**Step 1: `c_cast(void*, u)`** — C-style cast to `void*`.

A C-style cast attempts conversions in this order: `const_cast`, `static_cast`,
`static_cast` with const, `reinterpret_cast`, then combinations. Crucially, it
also invokes *implicit* conversion operators. `SinkWrapper<Derived>` has:

```cpp
operator Derived*() const { ... }   // implicit — no explicit keyword
```

So `(void*)u` resolves as:
1. Invoke implicit `operator Derived*()` → yields `Derived*` (with correct pointer arithmetic, including any base subobject adjustments)
2. Convert `Derived*` to `void*` — trivially valid

The pointer that comes out is the correct adjusted address.

**Step 2: `c_cast(T*, result)`** — C-style cast from `void*` to `Base*`.

`void*` → `Base*` is a standard conversion. Because step 1 produced the
*correct* `Derived*` (which for a standard-layout type with no added fields is
the same address as `Base*`), this yields the right `Base*`.

The two-step round-trip through `void*` is the idiomatic way to do
pointer-type conversions that the type system would otherwise reject, while
still going through semantically correct pointer values.

---

## Why `c_cast` Instead of a Plain C-Style Cast

`c_cast` expands to a C-style cast `(T)(expr)`. It's used rather than bare
parentheses for two reasons:

1. **Visibility.** A bare `(void*)u` in template code is easy to miss during
   review. `c_cast(void*, u)` is a visible signal that something unusual is
   happening.

2. **Searchability.** Every use of this workaround can be found by searching
   for `c_cast`. If a cleaner solution becomes possible in a later C++
   standard, all sites are easy to locate and update.

---

## Which Classes Use This and Why

| Class | Uses the workaround? | Why |
|---|---|---|
| `ContraWrapper<T*>` | Yes | Accepts any contravariant `U`; `U` may be a wrapper |
| `SinkWrapper<T*>` | Yes | Same; also needs to convert *from* other SinkWrappers |
| `InitWrapper<T*>` | Inherits from `ContraWrapper` | Constructors inherited |
| `NeedWrapper<T*>` | No | Only constructed from `T*` directly; no wrapper sources |
| `OptionWrapper<T>` | No | Wraps values, not pointer hierarchies |
| `ResultWrapper<T>` | No | Same |

The pattern only arises where a wrapper must accept another wrapper via the
contravariance relationship — i.e., wherever the source type is generic and
may be a wrapper rather than a raw pointer.

---

## Could This Be Avoided?

A few alternatives were considered:

**`reinterpret_cast<T*>(u.p)` directly** — accessing the field directly
bypasses the conversion operator entirely, which would be cleaner. But `p` is
a private field accessed via `NEEDFUL_DECLARE_WRAPPED_FIELD`, and friend
declarations for every pair of wrapper types would be required. The
`c_cast` workaround works without coupling the types together.

**A `.raw_pointer()` accessor** — adding a public method that returns the raw
pointer would work and be clean. This would be a reasonable future improvement.
The downside is API surface: it exposes an unsafe extraction path that
callers could use incorrectly. The current `operator T*()` triggers
corruption side effects on `SinkWrapper`, which a plain `.p` accessor would
bypass.

**C++23 `explicit(false)` or deducing `this`** — no new standard feature
directly solves this. The fundamental issue is that C++ cannot template the
*return type* of a conversion operator in a way that participates in implicit
conversion chains. That's a fundamental language rule, not a standard-version
limitation.

---

## Summary

The `c_cast(T*, c_cast(void*, u))` idiom exists because:

1. Wrapper types expose `operator T*()` for their specific `T`, not for base types.
2. `static_cast` cannot cross from a wrapper to an unrelated pointer type.
3. The templated `explicit operator U*()` uses `reinterpret_cast`, which is
   semantically wrong for inheritance relationships.
4. Going through `void*` forces the implicit `operator T*()` to fire first
   (giving the correct adjusted pointer), then allows a safe `void* → Base*`
   conversion.

This is tracked as an internal implementation detail. If a `.raw_pointer()`
accessor or equivalent is added in the future, all `c_cast(T*, c_cast(void*, ...))` sites in `needful-contra.hpp` are the candidates for cleanup.
