---
layout: default
title: Need(T)
nav_order: 2
permalink: /need
---

# `Need(T)` — Required Non-Null Value

`Need(T)` marks a pointer or value as *required* — it must never be null or
zero. Its primary purpose is to prevent meaningless boolean coercion: if a
value is guaranteed non-null, checking `if (ptr)` is a bug, not a feature.

```c
int value = 42;
Need(int*) ptr = &value;

printf("%d\n", *ptr);   // safe to dereference

if (ptr) { ... }        // ERROR (C++ build): cannot coerce Need(T*) to bool
```

In C, `Need(T)` is a transparent no-op — it expands to `T` directly. The C++
build wraps the value in `NeedWrapper<T>`, which deletes the implicit bool
conversion and disallows construction from `nullptr`.

## Shorthands

```c
#define Need(T)        // => T in C, NeedWrapper<T> in C++
#define unwrap         // extract the inner value
#define needed         // assert non-null at the call site
```

## Guarantees (C++ build)

- `Need(int*) p = nullptr;` — compile error (deleted constructor)
- `bool b = p;` — compile error (no implicit bool conversion)
- `Need(int*) p = someOption;` — compile error (must explicitly unwrap first)

## When to Use

Use `Need(T*)` on any function parameter or return type where null is a
contract violation, not just an unusual case. It documents the invariant and,
in C++ builds, enforces it.

## Related

- [`Option(T)`](/option) — the nullable counterpart; `unwrap` an `Option`
  before passing to `Need`.

---

## Compile-Time Tests

These blocks are extracted and compiled as part of the CI test suite.

### Basic usage

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    int value = 42;
    Need(int*) ptr = &value;
    assert(*ptr == 42);  // safe to dereference; guaranteed non-null
    int* raw = ptr;  // implicit conversion back to T* works
    assert(raw == &value);
    return 0;
}
```

### Assigning `nullptr` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted                                   <- GCC/Clang
// MATCH-ERROR-TEXT: attempting to reference a deleted function  <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    Need(int*) ptr = nullptr;  // ERROR: NeedWrapper(nullptr_t) is deleted
    NEEDFUL_UNUSED(ptr);
    return 0;
}
```

### Coercing to `bool` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted                                   <- GCC/Clang
// MATCH-ERROR-TEXT: attempting to reference a deleted function  <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    int value = 42;
    Need(int*) ptr = &value;
    bool b = ptr;  // ERROR: operator bool() is deleted; use the pointer directly
    NEEDFUL_UNUSED(b);
    return 0;
}
```

### Need(T) can be passed where Option(T) is expected

`Need(T*)` widens safely to `Option(T*)` — the non-null guarantee is a
subset of the nullable type's domain.

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

static int* last_seen = nullptr;

void accepts_option(Option(int*) p) {
    if (p)
        last_seen = unwrap p;
}

int main() {
    int value = 99;
    Need(int*) ptr = &value;
    accepts_option(ptr);      // Need(int*) lvalue widens to Option(int*)
    assert(last_seen == &value);
    return 0;
}
```

### Option(T) cannot be passed where Need(T) is expected

The reverse direction requires an explicit `unwrap` — an `Option` may be
null, so the compiler rejects the implicit narrowing.

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: could not convert
// MATCH-ERROR-TEXT: no known conversion

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

void accepts_need(Need(int*) p) { (void)p; }

int main() {
    int value = 42;
    Option(int*) maybe_ptr = &value;
    accepts_need(maybe_ptr);  // ERROR: Option(int*) won't convert to Need(int*)
    return 0;
}
```
