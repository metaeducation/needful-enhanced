---
layout: default
title: Setup
nav_order: 11
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
- run: g++ -x c++ -std=c++11 -DNEEDFUL_CPP_ENHANCED=1 -o app main.c
```

No source changes are needed between the two. In the C build, Needful stays a
transparent macro layer. In the checked C++ build, the same source gets the
extra type enforcement.

## Verifying the Setup

A quick smoke test — if this compiles and runs, the enhancement layer is
working:

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
