---
layout: default
title: Option(T)
nav_order: 3
permalink: /option
---

# `Option(T)` -- Explicitly Nullable Type

`Option(T)` is a Rust-inspired optional that uses `T`'s falsey state (null,
zero, or false) as its "none" sentinel. Critically, it has the **same size as
`T`** -- no extra bool, no heap allocation.

```c
Option(char*) abc = "hello";
Option(char*) xxx = none;        // same as nullptr for pointer types

if (abc)
    printf("safe to unwrap: %s\n", unwrap abc);

if (! xxx)
    printf("xxx is none, don't unwrap!\n");

char* s1 = abc;          // ERROR (C++ build): must unwrap first
char* s2 = unwrap abc;   // OK: explicit extraction
char* s3 = opt abc;      // OK: extracts even if none (use with care)
```

In C, `Option(T)` is a transparent no-op -- it expands to `T`. The C++ build
wraps the value in `OptionWrapper<T>`, which blocks implicit conversion to `T`
and requires explicit `unwrap` or `opt`.

## Shorthands

```c
#define Option(T)   // => T in C, OptionWrapper<T> in C++
#define none        // zero/null sentinel (needful_nocast_0)
#define unwrap      // extract; asserts not none in the checked build
#define opt         // extract unconditionally (unsafe)
```

## What `unwrap` Checks, and When {#unwrap-checks}

`unwrap` does two different jobs, and only one of them exists in the build
you ship:

| | compile time | run time |
|---|---|---|
| C build | nothing: `Option(T)` is `T`, and `unwrap` expands to nothing | nothing |
| Enhanced C++ build | an `Option(T)` can't be used as a `T` until you write `unwrap` or `opt` | `NEEDFUL_ASSERT()` that the value is not none |

So the protection is not that `unwrap` tests the value at run time in
production. It doesn't. The protection is that the checked build won't let you
*forget* the value is optional: every place one becomes a plain `T` is a
place somebody had to type `unwrap`, and can be found with a search. Whether
each of those is guarded by an `if` is still up to you -- and the run-time
assert is there to catch the ones that aren't, if you run your tests on the
checked build (with whatever `NEEDFUL_ASSERT` is defined as, see
[Setup](/setup)).

If you want a test that survives into the shipping build, write one:
`if (not x) ...` works identically in every mode.

## Why Not `std::optional`?

- Same size as `T` (no extra byte)
- Works with any type whose `0` state is a valid sentinel (pointers, ints,
  enums)
- No C++ dependency -- compiles as plain C
- Can be used in C structs without any C++ contamination

## The Naming of `opt`

If you are wondering why `opt` was chosen for the "raw extraction" operator:

While `unwrap` is fairly standard terminology (popularized by Rust) for
extracting a guaranteed-present value with a safety check, there isn't a
universally agreed-upon short name for "give me the raw underlying contents
even if it's empty."

`opt` is a terse signal that you are acknowledging the optional nature of the
value and explicitly choosing to pull out the potentially-null/falsey raw state.
While short identifiers like `opt` run a slight risk of shadowing local
variables in some codebases, it was chosen for brevity, so as not to overwhelm
lines that simply need to pass a nullable pointer into an older C API.
If `opt` causes collisions in your scope, `cast(T, optional_var)` is
semantically equivalent.

## Limitations

**Zero is spoken for.** The "same size as `T`" property is paid for by giving
up one value of `T`: its falsey state *is* none. That costs nothing for
pointers, where null was never a legitimate target anyway, and it is usually
what you want for an enum whose zero member means "unset". But it means there
is no difference between an `Option(int)` holding `0` and one holding nothing,
and an `Option(bool)` can't hold `false`. If zero is a real answer for your
type -- a count, an offset, an index -- then `Option(T)` is the wrong tool:
return it through a [`Result(T)`](/result), which keeps the failure somewhere
other than in the value.

**No zero, no `Option`.** Types without a natural zero state (e.g. a struct
with no "empty" representation) cannot be wrapped in `Option(T)`. For those,
use a separate boolean or `Result(T)`.

## Related

- [Operator Precedence](/precedence) -- why `unwrap x->field` needs parentheses

- [`Need(T)`](/need) -- the non-nullable counterpart
- [`Result(T)`](/result) -- for functions that can fail with an error
- [`Result(None)`](/result#result-none) -- for fallible functions that return no value on success

---

## Compile-Time Tests

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
    Option(char*) abc = (char*)"hello";
    Option(char*) xxx = none;
    assert(abc);                  // truthy: has a value
    assert(!xxx);                 // falsey: is none
    char* s = unwrap abc;         // safe: abc is truthy
    assert(s[0] == 'h');
    char* n = opt xxx;            // unconditional extract: gets nullptr
    assert(n == nullptr);
    return 0;
}
```

### Implicit unwrap to `T` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    Option(int) oi = 42;
    int x = oi;  // ERROR: must use unwrap or opt explicitly
    NEEDFUL_UNUSED(x);
    return 0;
}
```

### Implicit conversion to `bool` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    Option(int*) op = nullptr;
    bool b = op;  // ERROR: use if (op) for truthiness checks
    NEEDFUL_UNUSED(b);
    return 0;
}
```
