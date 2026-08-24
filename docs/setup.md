---
layout: default
title: Setup
nav_order: 60
permalink: /setup
---

# Setting Up `NEEDFUL_CPP_ENHANCED`

There are only two moving parts:

- [`needful.h`][needful-h] for the normal header-only C experience
- [`needful-enhanced/`][needful-enhanced-repo] for the optional C++ checking layer

If you never define `NEEDFUL_CPP_ENHANCED`, you can stop at `needful.h`.
If you do define it, Needful expects the companion `needful-enhanced/`
directory to be present and compiled as C++11 or later.

## What Goes Where

The checked build is intentionally simple: keep `needful-enhanced/` **next to**
`needful.h` in your include tree. `needful.h` reaches the companion headers by
relative path:

```
your-project/
    include/
        needful.h               ← the single header
        needful-enhanced/       ← this repository (cloned here)
            needful-contra.hpp
            needful-casts.hpp
            ...
```

## Getting the Files

`needful-enhanced/` is a separate git repository. Put it beside `needful.h`
using whatever integration style fits your project:

**Clone directly:**
```sh
git clone https://github.com/metaeducation/needful-enhanced \
    path/to/your/include/needful-enhanced
```

**As a git submodule** (keeps it pinned to a commit):
```sh
git submodule add https://github.com/metaeducation/needful-enhanced \
    path/to/your/include/needful-enhanced
```

**Via CMake FetchContent:**
```cmake
include(FetchContent)
FetchContent_Declare(
    needful_enhanced
    GIT_REPOSITORY https://github.com/metaeducation/needful-enhanced
    GIT_TAG        main
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/include/needful-enhanced
)
FetchContent_MakeAvailable(needful_enhanced)
```

## Enabling the Enhancements

Once the companion directory is in place, opt in with one define before
including the header:

```c
#ifdef __cplusplus  // C builds alert if NEEDFUL_CPP_ENHANCED defined nonzero
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"
```

That translation unit must be compiled as **C++11 or later**. Needful will
`#error` if you set the flag in a C build.

Checked builds also verify that `needful.h` and `needful-enhanced/` agree on
a shared compatibility version. If they drift apart, inclusion fails early
with a direct mismatch error instead of falling through to opaque template
errors.

## Customizing Runtime Assertions

Needful's internal runtime invariant checks go through `NEEDFUL_ASSERT(expr)`.
If you do nothing, `needful.h` falls back to the platform `assert()` by
including `<assert.h>` itself.

If your project uses a custom assert implementation, define `NEEDFUL_ASSERT`
before including `needful.h`:

```c
#define NEEDFUL_ASSERT(expr)  my_project_assert(expr)
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"
```

If you already replace `assert()` globally via a project header, you can route
Needful through that too:

```c
#include "assert-fix.h"
#define NEEDFUL_ASSERT(expr)  assert(expr)
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"
```

This only affects Needful's own runtime checks. Your code can still use
whatever assertion mechanism it wants.

## `.gitignore` Considerations

If you clone `needful-enhanced/` into a tracked part of your tree, ignore it so
your project does not accidentally absorb a separate repository:

```gitignore
# needful-enhanced/ is a separate repo cloned for development-time checks
src/include/needful-enhanced/
```

This is the intended workflow for teams that want checked builds available
without making the companion tree part of the main repository.

## Running Both Modes in CI

The intended CI story is one normal build and one checking build:

```yaml
# C build (production)
- run: gcc -o app main.c

# C++ build (type-checking)
- run: g++ -x c++ -std=c++17 -Werror -DNEEDFUL_CPP_ENHANCED=1 -o app main.c
```

No source changes are needed between the two. In the C build, Needful stays a
transparent macro layer. In the checked C++ build, the same source gets the
extra type enforcement.

Two details in that second line are load-bearing, and leaving either out
quietly disables part of what you think you are checking:

- **`-std=c++17`, not `c++11`.** `[[nodiscard]]` is the only spelling that
  works on a *class*, and `Result(T)` and `Fallible(T)` are classes in enhanced
  builds. Below C++17 the checked build still enforces type separation, but
  drops the must-use property — so a dropped `Result(T)` sails through a build
  you believed was your strictest. If you must build at C++11 or C++14, add
  `-DNEEDFUL_PRECPP17_NODISCARD=1`, which takes compilers up on accepting the
  attribute early and silences the resulting `-Wc++17-extensions` pedantry.
- **`-Werror`.** Most of what Needful rejects arrives as a *warning*.
  Discarding a must-use value produces a diagnostic and then an exit code of
  zero, so without escalation CI stays green over exactly the mistakes you
  added Needful to catch.

Both points are covered in detail, per compiler and per standard, under
[which builds actually check this](/fallible#enforcement).

## Configuration Switches

All of these are defined **before** including `needful.h`.

| Switch | Default | Effect |
|---|---|---|
| `NEEDFUL_CPP_ENHANCED` | `0` | Pull in `needful-enhanced/`; requires C++11+ |
| `NEEDFUL_DEFINE_ALL_SHORTHANDS` | `0` | Alias every `needful_xxx` as plain `xxx` |
| `NEEDFUL_<GROUP>_SHORTHANDS` | inherits the above | Per-group aliases: `OPTION`, `CAST`, `RESULT`, `FALLIBLE`, `CONTRA`, `KNOWN`, `COMMENT`, `STATIC_ASSERT`, `USAGE` |
| `NEEDFUL_NULLPTR_SHIM` | inherits `ALL_SHORTHANDS` | Define `nullptr` in C builds |
| `NEEDFUL_ASSERT(expr)` | `assert(expr)` | Route Needful's runtime checks somewhere else |
| `NEEDFUL_ABORT()` | `abort()` | Route `abort_if_none()` somewhere else |
| `NEEDFUL_DISABLE_INT_WARNING` | `1` | Suppress `-Wint-conversion` in C ([why](/faq#int-conversion-warning)) |
| `NEEDFUL_PRECPP17_NODISCARD` | `0` | Use `[[nodiscard]]` below C++17 where compilers allow it |
| `NEEDFUL_FALLIBLE_C23_MUSTUSE` | `0` | Use C23 `[[nodiscard]]` in C; [breaks `static`](/fallible#the-static-restriction) |
| `NEEDFUL_DOES_CORRUPTIONS` | `0` | Scramble dead variables in debug builds |
| `NEEDFUL_INIT_CORRUPTS_LIKE_SINK` | `0` | Also scramble `Init(T)` slots, not just `Sink(T)` |
| `NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS` | `1` | Vary the scramble byte; set `0` for a cheaper fixed fill |
| `NEEDFUL_DONT_INCLUDE_STDARG_H` | unset | Suppress the `<stdarg.h>` include (for `v_cast()`) |
| `NEEDFUL_DECLARE_RESULT_HOOKS` | unset | Emit single-threaded default `Result(T)` hooks in this one TU |
| `NEEDFUL_<X>_USES_WRAPPER` | `NEEDFUL_CPP_ENHANCED` | Turn one wrapper family off: `NEED`, `OPTION`, `RESULT`, `CONTRAS` |
| `NEEDFUL_CAST_CALLS_HOOKS` | `NEEDFUL_CPP_ENHANCED` | Run `CastHook` validation on `cast()` |
| `NEEDFUL_FAST_CAST_IS_SLOW` | unset | Make `fast_cast()` hookable, for debugging a hot path |
| `NEEDFUL_ICAST_SLOW_BUILD` | `0` | Make `i_cast()` the checked integer cast rather than `c_cast()` |

## Verifying the Setup

A quick smoke test — if this compiles and runs, the enhancement layer is
working:

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#include "needful.h"

int main() {
    int x = 42;
    NeedfulNeed(int) n = x;
    assert(n == 42);
    return 0;
}
```

## Note on Alternative Tokens (MSVC)

The C++ code in `needful-enhanced/` makes use of C++ standard "alternative
operator representations" (like `and`, `or`, and `not` instead of `&&`, `||`,
and `!`) as a principled stance on readability.

If you are compiling a C++ checked build with **MSVC** (Microsoft Visual C++),
the compiler does not treat these as keywords by default.

While you *could* pass compiler flags like `/permissive-` or `/Za` (which
enforce broader standards compliance, but have sweeping side effects that often
break legacy Windows code), the **safest and most narrow workaround** is to
simply include the standard header `<ciso646>`, which is done automatically
when you build with NEEDFUL_CPP_ENHANCED in MSVC.

[needful-h]: https://raw.githubusercontent.com/metaeducation/needful/refs/heads/main/needful.h
[needful-enhanced-repo]: https://github.com/metaeducation/needful-enhanced/
