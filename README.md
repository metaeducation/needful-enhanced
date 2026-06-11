# needful-enhanced

C++ compile-time type enforcement for [needful.h][needful-h-src].

Full documentation: **[needful.metaeducation.com][needful-site]**

---

## If you found this directory on your filesystem

This directory was placed here to enable `NEEDFUL_CPP_ENHANCED` for the
project that owns the `needful.h` next to it. That project's build setup
(a script, CMake, or a manual step) cloned it here specifically for
development-time type checking.

It is intentionally **not committed** to the parent project's repository -
you will likely find it listed in that project's `.gitignore`. It is a
separate git repository that just happens to live alongside `needful.h`.

Setting it up in a different project, or understanding what it does:
[needful.metaeducation.com/setup][needful-setup]

---

## What this repository contains

When `#define NEEDFUL_CPP_ENHANCED 1` is set, `needful.h` includes `.hpp`
files from this directory. Those files replace Needful's transparent C macros
with C++ wrapper types that enforce type safety at compile time:

- `Need(T)` blocks null assignments and boolean coercion
- `Option(T)` requires explicit unwrapping
- `Result(T)` cannot be silently discarded
- `Sink(T)` / `Init(T)` enforce contravariant output parameter rules
- The cast family calls hooks instead of compiling away
- `known()` asserts the type of an expression inside macros

In a plain C build (or a C++ build without `NEEDFUL_CPP_ENHANCED`), none of
this directory is used and none of it is needed.

## Repository layout

```
docs/          Web documentation (needful.metaeducation.com)
tests/         Positive and negative compile-time tests
tools/         extract-doctests.py — docs-as-tests infrastructure
.github/       CI across GCC, Clang, MSVC
needful-*.hpp  The enhancement headers included by needful.h
```

[needful-site]: https://needful.metaeducation.com
[needful-setup]: https://needful.metaeducation.com/setup
[needful-h-src]: https://github.com/metaeducation/needful/blob/main/needful.h
