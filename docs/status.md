---
layout: default
title: Status(T)
nav_order: 4.5
permalink: /status
---

# `Status(T)` -- Zero Means Okay (Must-Use, No Truth Value)

A great deal of C returns a status code: `0` when things went fine, and
something else when they didn't. POSIX calls, ODBC's `SQLRETURN`, and a
thousand in-house `#define OK 0` / `#define ERR -1` conventions all work this
way.

`Status(T)` marks such a return value:

```c
Status(int) Flush_Buffer(Buffer* b) {
    if (b->broken)
        return E_IO;
    /* ... */
    return okay;
}
```

In a plain C build it is just a `T` (with a must-use annotation where the
compiler offers one). In the checked build it enforces three things.

## What It Enforces

**It can't be ignored.** Like [`Fallible(T)`](/fallible), calling a function
that returns a `Status(T)` and dropping the result is a diagnostic.

**It has no truth value.** This is the reason the type exists:

```c
if (Flush_Buffer(b))    /* reads as "if it worked"... */
    Report_Success();   /* ...runs when it FAILED */
```

Zero-is-okay is backwards from how C's `if` reads, and everyone has written
that bug. So in the checked build `if (status)`, `! status`, `status && x`
and `status ? a : b` do not compile. You have to say which you mean:

```c
if (is_okay(Flush_Buffer(b)))
    Report_Success();
```

**It doesn't mix with `Option(T)` or `Fallible(T)`.** Those also single out
zero, but with the opposite meaning: for them zero is `none`, the *bad* state.
A `Status(T)` function can't `return none;`, an `Option(T)` function can't
`return okay;`, and each family's macros refuse the other's values.

## What Counts as a Status

Deliberately little: **zero is okay, and anything else is a signal the caller
must look at.** It is not called "failed", because it might not be:

```c
#define C_OK      0
#define C_ERR    -1
#define C_RETRY  -2
```

`C_RETRY` isn't a failure. It's just not okay. That is why the vocabulary
below says `not_okay` and never `failed`.

## The Vocabulary

| | |
|---|---|
| `Status(T)` | The return type. |
| `StatusVar(T)` | The same thing for locals, parameters and struct members (see [why two spellings](/fallible#the-static-restriction)). |
| `okay` | The zero value: `return okay;`, `rc = okay;`. |
| `is_okay(expr)` | True if the status is zero. The only way to test one. |
| `status_code(expr)` | The raw `T`, for a `switch`, a log message, or `<`. |
| `return_if_not_okay (T, expr);` | If `expr` isn't okay, return its code. `T` is the code's type. |
| `abort_if_not_okay (expr);` | Crash if not okay. |
| `assert_okay (expr);` | Assert okay in debug builds (`expr` still runs under `NDEBUG`). |
| `tolerate_not_okay (expr);` | Deliberately ignore it, and say so. |

Comparing against a particular code needs nothing special:

```c
StatusVar(int) rc = Try_Lock(m);
if (rc == C_RETRY)
    Wait_A_Bit();
```

### Why `return_if_not_okay` takes a type

`return_if_none` can simply return `none`. Propagating a status means
returning *the code that came back*, so the macro has to hold it in a
temporary, and plain C has no `auto` to declare one with. So you say what
type the code is:

```c
Status(int) Save_All(Document* doc) {
    return_if_not_okay (int, Write_Header(doc));
    return_if_not_okay (int, Write_Body(doc));
    return okay;
}
```

The function's own return type has to be able to hold the code. If you need
to translate one kind of code into another, or want the code in a variable to
inspect before deciding, write it out:

```c
StatusVar(int) rc = Write_Header(doc);
if (rc == C_RETRY)
    /* ... */
```

## How the Three Families Line Up

Each type has exactly one state that it defines precisely, and that state
gets the name. The other side is just "not that".

| | [`Option`](/option) / [`Fallible`](/fallible) | `Status` | [`Result`](/result) |
|---|---|---|---|
| The named state | `none` (zero) | `okay` (zero) | `failed` |
| Returning it | `return none;` | `return okay;` | `return_fail (...)` |
| Testing | `is_none(x)` | `is_okay(x)` | `catch_if_failed` |
| Propagate | `return_if_none` | `return_if_not_okay` | `return_if_failed` |
| Die on it | `abort_if_none` | `abort_if_not_okay` | `panic_if_failed` |
| Assert | `assert_not_none` | `assert_okay` | `assert_not_failed` |
| Ignore | `tolerate_none` | `tolerate_not_okay` | `extract_failure` |

`Option` and `Status` are mirror images, which is exactly why the `Status`
macros carry a `not_` where the `Option` ones don't.

## Unannotated APIs

`is_okay()` and `status_code()` also accept a raw integer or enum, so you can
use them on functions you haven't (or can't) annotate:

```c
if (! is_okay(SQLFetch(stmt)))
    /* ... */
```

and `return_if_not_okay (SQLRETURN, SQLFetch(stmt));` works for the same reason: a
raw code converts to a `Status(T)`. What they don't accept is a `bool`, a
pointer, or an `Option(T)`. Returning `true` from a `Status(T)` function is
likewise an error.

## Limits

- **A plain C build checks only the must-use part**, and only where the
  compiler has an annotation for it. There `okay` and `none` are both `0`.
  The checked build is what catches `if (status)`. See [which builds enforce
  must-use](/fallible#enforcement); the same table applies here.
- **Below C++17 the checked build can't enforce must-use** either, since
  `[[nodiscard]]` on a class doesn't exist. The truth-value and mixing checks
  work from C++11.
- **`Status` is a common name.** X11 and GDI+ both define one. If it collides,
  leave `NEEDFUL_STATUS_SHORTHANDS` off for those files and write
  `NeedfulStatus(T)`.

## Related

- [`Fallible(T)`](/fallible) -- the must-use type where zero is the failure
- [`Result(T)`](/result) -- when a failure needs to carry information
- [Setup](/setup#configuration-switches) -- `NEEDFUL_STATUS_USES_WRAPPER`,
  `NEEDFUL_STATUS_SHORTHANDS`

---

## Compile-Time Tests

### Basic usage

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#include "needful.h"

#define C_OK      0
#define C_ERR    -1
#define C_RETRY  -2

typedef enum { E_NONE = 0, E_IO = 1, E_FULL = 2 } DiskError;

static int Raw_Api(int n) { return n; }  // an API nobody annotated

static Status(int) Step(int n) {
    if (n < 0)
        return C_ERR;
    if (n == 0)
        return C_RETRY;
    return okay;
}

static Status(DiskError) Write_Block(int n) {
    if (n > 100)
        return E_FULL;
    return okay;
}

static Status(int) Run(int n) {
    return_if_not_okay (int, Step(n));
    return_if_not_okay (int, Raw_Api(0));  // raw codes work too
    return okay;
}

static Status(DiskError) Save(int n) {
    return_if_not_okay (DiskError, Write_Block(n));
    return okay;
}

int main() {
    StatusVar(int) rc = Run(1);
    assert(is_okay(rc));

    rc = Run(-1);  // the code is propagated, not flattened
    assert(! is_okay(rc));
    assert(rc == C_ERR);
    assert(rc != C_OK);

    assert(Run(0) == C_RETRY);  // not okay, and not a failure either

    switch (status_code(Run(0))) {  // the raw code, for a switch
      case C_RETRY:
        break;
      default:
        assert(0);
    }
    assert(status_code(rc) < 0);

    StatusVar(DiskError) e = Save(500);  // enum codes
    assert(e == E_FULL);
    assert(is_okay(Save(5)));

    assert(is_okay(Raw_Api(0)));  // unannotated API

    assert_okay (Run(5));
    assert_okay (rc = Run(5));  // the assignment happens even under NDEBUG
    assert(is_okay(rc));
    abort_if_not_okay (Run(5));
    tolerate_not_okay (Run(-1));

    rc = okay;
    assert(rc == okay);
    return 0;
}
```

### A `Status(T)` has no truth value

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: has no truth value

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#include "needful.h"

static Status(int) Step(void) { return okay; }

int main() {
    if (Step())  // ERROR: reads as "if it worked", means "if it didn't"
        return 1;
    return 0;
}
```

### Negating a `Status(T)` is a compile error too

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: has no truth value

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#include "needful.h"

static Status(int) Step(void) { return okay; }

int main() {
    StatusVar(int) rc = Step();
    if (! rc)  // ERROR: say is_okay(rc)
        return 0;
    return 1;
}
```

### Discarding a `Status(T)` is a compile error

<!-- doctest: negative-test -->
```cpp
// REQUIRES-STD: c++17
//
// MATCH-ERROR-TEXT: nodiscard               <- all three
// MATCH-ERROR-TEXT: ignoring return value   <- GCC, Clang
// MATCH-ERROR-TEXT: discarding return value <- MSVC (C4834)

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#include "needful.h"

static Status(int) Step(void) { return okay; }

int main() {
    Step();  // ERROR: the status must be looked at
    return 0;
}
```

### `return none;` from a `Status(T)` function is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: deleted

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

static Status(int) Step(void) {
    return none;  // ERROR: `none` is Option(T)'s word; this is `okay`
}

int main() {
    return is_okay(Step()) ? 0 : 1;
}
```

### `return okay;` from an `Option(T)` function is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: could not convert       <- GCC
// MATCH-ERROR-TEXT: no viable conversion    <- Clang
// MATCH-ERROR-TEXT: cannot convert          <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

static Option(int*) Find(void) {
    return okay;  // ERROR: an Option(T)'s zero state is `none`
}

int main() {
    return Find() ? 1 : 0;
}
```

### `return true;` from a `Status(T)` function is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: could not convert       <- GCC
// MATCH-ERROR-TEXT: no viable conversion    <- Clang
// MATCH-ERROR-TEXT: cannot convert          <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_STATUS_SHORTHANDS  1
#include "needful.h"

static Status(int) Step(void) {
    return true;  // ERROR: true is 1, which is "not okay"
}

int main() {
    return is_okay(Step()) ? 0 : 1;
}
```
