---
layout: default
title: unreachable
nav_order: 11
permalink: /unreachable
---

# `needful_unreachable` — Indicate Divergence & Optimize Branches

`needful_unreachable` tells both the programmer and the compiler that a specific
code path is mathematically or logically impossible to reach.

Unlike a passive comment or a simple `assert(false)`, it acts as an aggressive
compiler hint for generating highly optimized machine code in production
binaries while maintaining complete safety during debugging.

## Variations

Because C and C++ require distinct syntactic return expressions depending on the
function type, Needful provides three variations:

```c
needful_unreachable;  // For functions returning (pointers, ints, enums)
needful_unreachable_void;  // For void functions
needful_unreachable_struct(type);  // For functions returning structs
```

## Efficacy & Compiler Behavior

The implementation changes its behavior entirely based on your build target:

| Build Context | Macro Expansion | Behavior |
|---|---|---|
| Debug (`-O0` / no `NDEBUG`) | `assert(false)` | Crashes with a core dump at the exact line of failure if an impossible state occurs. |
| Release (`-O3` / `NDEBUG`) | `__builtin_unreachable()` or `__assume(0)` | The compiler takes you at your word, completely deletes the branch, and heavily optimizes surrounding code (e.g. switch jump tables). |

## Example: Optimizing Switch Jump Tables

Consider a function evaluating an enum type that you know can only have three
states:

```c
int get_container_capacity(ContainerType type) {
    switch (type) {
        case BITSET_CONTAINER: return 1024;
        case ARRAY_CONTAINER:  return 256;
        case RUN_CONTAINER:    return 64;
        default:
            needful_unreachable;
    }
}
```

In a release build, the compiler eliminates the `default` branch entirely.
Furthermore, it optimizes the switch's internal bounds-checking because it
knows `type` will never exceed the maximum enum value.

> **Why inside the `default` block instead of after the switch?**
> If placed after the switch block, the compiler must assume the default path is
> a valid escape route and generate branching assembly to handle it. Placing it
> inside the `default` block allows the compiler to prune the entire branch from
> the final binary.

## Polymorphic Returns

To prevent compiler warnings about "control reaches end of non-void function"
in environments that do not support unreachable intrinsics (or during static
analysis), `needful_unreachable` leverages [`needful_nocast_0`](/nocast).

This allows a single macro to automatically synthesize a valid `0`, `nullptr`,
or enum zero return value regardless of the function's scalar return signature,
completely eliminating type boilerplate.

## Related

- [`nocast`](/nocast) — the `needful_nocast_0` polymorphic zero it relies on
- [`Option(T)`](/option) — for values that may legitimately be absent
- [`Result(T)`](/result) — for functions that can fail with an error
