---
layout: default
title: Setup
nav_order: 11
permalink: /setup
---

# Setting Up `NEEDFUL_CPP_ENHANCED`

`needful.h` works standalone with no configuration. The `NEEDFUL_CPP_ENHANCED`
flag is an *opt-in* that activates compile-time type enforcement by pulling in
C++ wrapper types from a companion directory, `needful-enhanced/`.

This page explains how to add that companion directory to a project.

## What Goes Where

Needful expects `needful-enhanced/` to sit **next to** (not inside) `needful.h`
in your include path. The header includes the `.hpp` files via a relative path:

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

`needful-enhanced/` is a separate git repository. Add it next to `needful.h`
however is appropriate for your project:

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

> **Note:** `needful.h` itself is currently maintained in the
> [Ren-C repository](https://github.com/metaeducation/ren-c/blob/master/src/include/needful.h).
> A standalone distribution is planned; for now, fetch it from there.

## Enabling the Enhancements

Once `needful-enhanced/` is in place, define the flag **before** including the
header:

```c
#define NEEDFUL_CPP_ENHANCED  1
#include "needful.h"
```

The file must be compiled as **C++11 or later**. Needful will `#error` if you
set the flag in a C build.

## Customizing Runtime Assertions

Needful's internal runtime invariant checks go through `NEEDFUL_ASSERT(expr)`.
If you do nothing, `needful.h` falls back to the platform `assert()` by
including `<assert.h>` itself.

If your project uses a custom assert implementation, define `NEEDFUL_ASSERT`
before including `needful.h`:

```c
#define NEEDFUL_ASSERT(expr)  my_project_assert(expr)
#define NEEDFUL_CPP_ENHANCED  1
#include "needful.h"
```

If you already replace `assert()` globally via a project header, you can route
Needful through that too:

```c
#include "assert-fix.h"
#define NEEDFUL_ASSERT(expr)  assert(expr)
#define NEEDFUL_CPP_ENHANCED  1
#include "needful.h"
```

This only affects Needful's own runtime checks. Your code can still use
whatever assertion mechanism it wants.

## `.gitignore` Considerations

If `needful-enhanced/` is cloned into a directory already tracked by your
project's git, ignore it so your repo doesn't try to track a foreign tree:

```gitignore
# needful-enhanced/ is a separate repo cloned for development-time checks
src/include/needful-enhanced/
```

This is why the directory typically appears `.gitignore`d when you encounter it
on a developer filesystem — the project intentionally does not commit it.

## Running Both Modes in CI

The typical pattern is a C production build and a C++ checking build:

```yaml
# C build (production)
- run: gcc -o app main.c

# C++ build (type-checking)
- run: g++ -x c++ -std=c++11 -DNEEDFUL_CPP_ENHANCED=1 -o app main.c
```

No source changes are needed between the two. Every `needful` construct is a
transparent no-op in the C build and a type-enforced wrapper in the C++ build.

## Verifying the Setup

A quick smoke test — if this compiles and runs, the enhancement layer is
working:

```cpp
#define NEEDFUL_CPP_ENHANCED  1
#include <assert.h>
#include "needful.h"

int main() {
    int x = 42;
    Need(int) n = x;
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
