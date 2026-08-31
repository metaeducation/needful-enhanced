---
layout: default
title: Threading
nav_order: 65
permalink: /threading
---

# Threading Model

Needful has two thread-sensitive mechanisms, and they are separate:

- `Result(T)` failure storage (project hook responsibility)
- corruption countdown state (enhanced corruption internals)

Only enhanced C++ builds are involved in the second one. Plain C mode remains
macro-only and unchanged by this page.

## 1) `Result(T)` Hook Storage

`Result(T)` assumes there is one "pending failure" slot. In real threaded
programs, that slot should be thread-local.

The built-in `NEEDFUL_DECLARE_SINGLETHREADED_RESULT_HOOKS` implementation is
intentionally single-threaded and is for prototyping/tests only.

For threaded programs, provide custom hooks over TLS storage:

```c
static _Thread_local const char* tls_failure;

const char* Needful_Test_And_Clear_Failure(void) {
    const char* e = tls_failure;
    tls_failure = NULL;
    return e;
}

#define Needful_Get_Failure()        tls_failure
#define Needful_Set_Failure(error)   (tls_failure = (error))
```

In C++, use `thread_local` instead of `_Thread_local`.

## 2) Corruption Countdown State (`NEEDFUL_DOES_CORRUPTIONS`)

When corruption is enabled in enhanced builds, pseudo-random corruption relies
on countdown state. That state is now thread-safe by default.

### Default behavior (recommended)

- `NEEDFUL_CORRUPTION_USE_ATOMICS` defaults to `1`.
- No lock hooks are required.
- Countdown updates use relaxed atomics.
- `<atomic>` is included only when this path is active.

This gives a race-free default with no integration work.

### Optional custom synchronization mode (no `<atomic>` dependency)

If your project has its own atomic/threading runtime model, define all three
macros before including `needful.h`:

```c
#define NEEDFUL_DECLARE_ATOMIC_VAR(name) NeedfulRuntimeAtomicU8 name
#define NEEDFUL_LOCK_VARIABLE(var)   Lock_Needful_Corruption()
#define NEEDFUL_UNLOCK_VARIABLE(var) Unlock_Needful_Corruption()
```

In this mode, Needful does not include `<atomic>` and relies on your
synchronization hooks to serialize updates.

The declared variable type must round-trip to/from `uint_fast8_t` while locked
(that is, readable and assignable in terms of countdown byte values).

### Optional single-thread mode

If the process is truly single-threaded, you may bypass atomics/locks:

```c
#define NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED  1
```

This is the fastest mode, and also the easiest to misuse. Only enable it when
you are certain corruption helpers cannot run concurrently.

## Validation Rules

Enhanced corruption enforces these combinations at compile time:

- defining any one of `NEEDFUL_DECLARE_ATOMIC_VAR`,
  `NEEDFUL_LOCK_VARIABLE`, or `NEEDFUL_UNLOCK_VARIABLE` requires defining all
  three.
- custom synchronization mode conflicts with atomic mode and with
  single-thread mode.
- single-thread mode conflicts with atomic mode.
- with pseudo-random corruption on, if you disable atomics and do not provide
  custom synchronization hooks, you must explicitly declare single-thread mode.

In short: the build must choose exactly one safety story.

## Dependency Footprint Notes

The enhanced layer keeps its standard-library surface intentionally small:

- compile-time traits and forwarding headers (for example `type_traits`,
  `utility`)
- low-level C interoperability headers where needed (for example `stdint.h`)
- no container-heavy dependencies such as `vector`, and no allocation
  framework assumptions

`<atomic>` is specifically demand-driven through corruption-threading mode,
not a blanket include for all enhanced builds.

## Practical Recommendation

For most teams:

1. Keep the default corruption atomic mode.
2. Implement thread-local `Result(T)` hooks.
3. Run enhanced builds in CI with warnings-as-errors.

Only reach for lock hooks if project policy requires one shared lock domain,
or if you need lock-instrumented diagnostics around corruption state changes.

---

## Compile-Time Tests

### Custom synchronization triad compiles without standard atomics

<!-- doctest: positive-test -->
```cpp
#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif

#define NEEDFUL_DOES_CORRUPTIONS  1
#define NEEDFUL_CORRUPTION_USE_ATOMICS  0

#define NEEDFUL_DECLARE_ATOMIC_VAR(name) uint_fast8_t name
#define NEEDFUL_LOCK_VARIABLE(var)       ((void)0)
#define NEEDFUL_UNLOCK_VARIABLE(var)     ((void)0)

#include "needful.h"

int main() {
    int x = 10;
    Corrupt_If_Needful(x);
    return 0;
}
```

### Defining lock hooks without NEEDFUL_DECLARE_ATOMIC_VAR is rejected

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: Define NEEDFUL_DECLARE_ATOMIC_VAR(name)
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif

#define NEEDFUL_DOES_CORRUPTIONS  1
#define NEEDFUL_CORRUPTION_USE_ATOMICS  0

#define NEEDFUL_LOCK_VARIABLE(var)   ((void)0)
#define NEEDFUL_UNLOCK_VARIABLE(var) ((void)0)

#include "needful.h"

int main() { return 0; }
```

### Single-thread mode compiles without atomics or custom hooks

<!-- doctest: positive-test -->
```cpp
#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif

#define NEEDFUL_DOES_CORRUPTIONS  1
#define NEEDFUL_CORRUPTION_USE_ATOMICS  0
#define NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED  1

#include "needful.h"

int main() {
    int x = 0;
    Corrupt_If_Needful(x);
    return 0;
}
```
