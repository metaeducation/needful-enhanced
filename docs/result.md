---
layout: default
title: Result(T)
nav_order: 5
permalink: /result
---

# `Result(T)` — Cooperative Error Propagation

`Result(T)` multiplexes an error and a return value using thread-local state,
giving C code a Rust-inspired error handling style without exceptions or
`setjmp`/`longjmp`.

## The Pattern

Instead of this:

```c
Error* Some_Func(int* result, int x) {
    if (x < 304)
        return make_failure("the value is too small");
    *result = x + 20;
    return nullptr;
}

Error* Other_Func(int* result) {
    int y;
    Error* e = Some_Func(&y, 1000);
    if (e) return e;
    // ...manual propagation repeated for every call...
}
```

You write this:

```c
Result(int) Some_Func(int x) {
    if (x < 304)
        return make_failure("the value is too small");
    return x + 20;
}

Result(int) Other_Func(void) {
    return_if_failed (
      int y = Some_Func(1000)
    );
    assert(y == 1020);

    return_if_failed (
      int z = Some_Func(10);   // auto-propagates on error
    );
    return z;
}
```

## Error Handling Vocabulary

| Operation | Meaning |
|---|---|---|
| `return make_failure(...)` | Return a failure, storing the error in thread-local state |
| `return_if_failed (stmt)` | Execute `stmt`; if it failed, propagate the failure upward |
| `panic_if_failed (stmt)` | Like above, but panics rather than propagating |
| `assert_not_failed (stmt)` | Execute `stmt`; assert no failure occurred (debug) |
| `catch_if_failed (Error* e) { }` | Catch a failure into a scoped variable |
| `extract_failure (expr)` | Evaluate `expr` and return the failure (or null if none) |
| `panic(...)` | Abort immediately; never returns |

A quick and dirty way to write `return failed;` and not have to come up with
an error might be useful in some codebases.  We don't try to define that here,
because it's open ended as to what you'd use for your error value type.

In order for these macros to work, they need to be able to test and clear the
global error state.  Hence you have to define:

    ErrorType* Needful_Test_And_Clear_Failure()
    ErrorType* Needful_Get_Failure()
    void Needful_Set_Failure(ErrorType* error)
    void Needful_Panic_Abruptly()
    void Needful_Assert_Not_Failing()  // avoids assert() dependency

These can be functions or macros with the same signature.  They should use
thread-local state if they're to work in multi-threaded code.


## The `catch_if_failed` Syntax

The most ergonomic pattern — `needful_catch_if_failed` (shorthand:
`catch_if_failed`) — attaches naturally to an expression and allows an
`else` clause:

```c
int result = Some_Func(30) catch_if_failed (Error* e) {
    printf("caught: %s\n", e->message);
    result = -1;
} else {
    printf("success!\n");
}
```

This is standard C99. `needful_catch_if_failed` expands into a `for` loop that
runs exactly once, scoping the error variable to the block.

## Defining Project-Specific Aliases

`NEEDFUL_RESULT_SHORTHANDS` strips the `needful_` prefix, giving names like
`make_failure`, `return_if_failed`, `catch_if_failed`, etc. These are
readable and unambiguous without being keyword-like.

If your project wants to go further and use keyword-style names, define
them yourself in a project header after including `needful.h`.  Here are some
possible choices:


```c
#define fail     needful_make_failure
#define trap     needful_return_if_failed
#define require  needful_panic_if_failed
#define assume   needful_assert_not_failed
#define except   needful_catch_if_failed
#define rescue   needful_extract_failure
```

For `Fallible(T)` the same approach applies:

```c
#define maybe      needful_return_if_none
#define demand     needful_abort_if_none
#define whatever   needful_tolerate_none
```

Needful intentionally does not define keyword-grabbing names itself.

## Relationship to `Fallible(T)` {#fallible-relationship}

The two vocabularies are deliberately parallel:

| intent | `Result(T)` | `Fallible(T)` |
|---|---|---|
| signal it | `return make_failure(e)` | `return none` |
| propagate it | `return_if_failed (stmt)` | `return_if_none (stmt)` |
| die on it | `panic_if_failed (stmt)` | `abort_if_none (stmt)` |
| assert impossible | `assert_not_failed (stmt)` | `assert_not_none (stmt)` |
| handle it | `catch_if_failed (decl) { }` | — plain `if` |
| deliberately ignore | — see `extract_failure` | `tolerate_none (stmt)` |

The gaps are real rather than missing work. `Fallible(T)` needs no `catch`
because there is no error payload to catch — the absence *is* the information.
`Result(T)` needs no `tolerate` because `extract_failure` already reads and
clears the state.

Note that `panic_if_failed` is not called `abort_if_failed`, tempting as the
symmetry would be. A failure carries an error object, so it is handed to
`Needful_Panic_Abruptly()` to be reported before the process ends. A disengaged
`Option` has nothing to report, so `abort_if_none` can only call
`NEEDFUL_ABORT()`. A shared `abort_` prefix would promise a shared exit path
that does not exist.

**The two families do not interoperate**, and cannot be merged into one.
See [the FAQ](/faq#result-fallible-unification) — this comes up more than once,
and the reason is more interesting than "nobody got around to it."

It bears some explanation that the trick to get except() to be able to take an
else() clause involves a for loop that runs exactly once.  It accomplishes
this using the C99 feature allowing you do declare multiple variables scoped
to a for loop *if* they are of the same type.  If we assume your error type is
a pointer, then we can declare both the error variable and a dummy pointer
`_once` in the loop, and use a pointer increment to ensure the loop only runs
once.  :-)

### How the scoping trick works

C99 allows a `for` loop to declare multiple variables in its init clause *if
they are the same type*. Since the error variable is a pointer, a dummy
`_once` sentinel of the same pointer type can be co-declared:

```c
for (Error* e = Needful_Get_Failure(), *_once = nullptr; !_once; ++_once)
    if (Needful_Test_And_Clear_Failure())
        /* { error body } */
    else
        /* { success body } */
```

`_once` starts as `nullptr`, so `!_once` is true and the body runs once.
`++_once` makes it non-null and the loop exits. This scopes `e` to the
loop body exactly like a normal `if` block — while the `if`/`else`
structure leaves the `else` clause free for the success case.

## Which builds actually check this {#enforcement}

**`Result(T)` is enforced only by the C++ enhanced build.** This is a sharper
statement than the equivalent one for [`Fallible(T)`](/fallible#enforcement),
and it is the single most important thing to know before adopting it.

In C, `Result(T)` is `#define NeedfulResult(T) T` and `NEEDFUL_RESULT_0` is
plain `0`. There is no annotation that could help, because the guarantees are
about a *wrapper type* rather than about a function's return value, and C has
no way to express that. So a pure-C build gives you the vocabulary, the
propagation, and the runtime assertions — but not one compile-time check.

| build | must extract the result | `return` not forgotten on `make_failure` | won't decay to `T` |
|---|---|---|---|
| C — any compiler, any standard | ✗ | ✗ | ✗ |
| C++ — no enhancement | ✗ | ✗ | ✗ |
| C++ — `NEEDFUL_CPP_ENHANCED`, C++11/14 | ✗ | ✗ | ✅ |
| C++ — enhanced, C++11/14 + `NEEDFUL_PRECPP17_NODISCARD` | ✅ | ✅ | ✅ |
| C++ — `NEEDFUL_CPP_ENHANCED`, **C++17+** | ✅ | ✅ | ✅ |

The first two rows are why Needful's pitch is *"C for production, C++ for
stronger checks"* rather than *"C is enough"*. If you never run a checked C++
build, `Result(T)` is documentation with a propagation mechanism attached.

### Why `Result(T)` can't borrow `Fallible(T)`'s C-mode trick {#no-c-mustuse}

[`Fallible(T)`](/fallible#enforcement) *does* get checking in a plain C build,
by carrying `__attribute__((warn_unused_result))` on its return type. The fair
question is why `Result(T)` does not do the same.

It comes down to what survives the preprocessor. `return_if_failed (stmt)`
expands `stmt` followed by `needful_postfix_extract_result`, and that token is
where the two builds part company:

```c
/* C++ enhanced */  side_effect(ok) % needful::g_result_extractor;
/* plain C      */  side_effect(ok);
```

Under enhancement the extractor is `operator%`, so the returned wrapper is
genuinely consumed — which is exactly how the `[[nodiscard]]` on
`ResultWrapper` gets discharged by ordinary code. In C the extractor expands to
nothing, leaving a bare expression statement. Annotating `Result(T)` in C would
therefore make the compiler object to Needful's *own correct code* — concretely
to [`Result(None)`](#result-none) calls, where no assignment consumes the value
either:

```c
return_if_failed (Do_Something());     /* would warn: nothing consumes it */
return_if_failed (int v = Compute());  /* fine -- consumed by the initializer */
```

Nor can the C extractor simply be given a body. It has to be a postfix token
that is valid both after a bare call *and* inside a declaration's initializer,
while preserving the value's type — including when `T` is a struct. That is
precisely what `operator%` provides in C++, and C has no equivalent.

A `(void)` cast is not a way out either. It does suppress C23's
`[[nodiscard]]`, but GCC's `warn_unused_result` deliberately ignores one — and
GCC/Clang is the configuration that would otherwise deliver this coverage,
since it offers the attribute at every C standard. The escape hatch exists only
where it is not needed.

**There is consequently no `ResultVar(T)`.** `FallibleVar(T)` exists because
`Fallible(T)`'s annotation is legal only on a function's return type, so locals
and parameters need an unannotated spelling. `Result(T)` carries no
declaration-position annotation in either build mode — under enhancement the
`[[nodiscard]]` rides on the wrapper class — so every position is already legal
and there is nothing to work around.

The C++11/14 row is the trap. `ResultWrapper` and `Result0Struct` are both
declared `NEEDFUL_NODISCARD`, which has no *standard* spelling before C++17 and
quietly becomes nothing — so the build still refuses to let a `Result(T)` decay
into a `T`, but stops noticing when you drop one entirely, or when you write
`make_failure(...)` and forget the `return`. That last one is the mistake the
`[[nodiscard]]` on `Result0Struct` exists to catch, and it is easy to make
because `panic(...)` sits right beside it, takes an error, and is *not* used
with `return`.

**Run the enhanced build at C++17 or higher** if you can. If you are pinned
below it, `-DNEEDFUL_PRECPP17_NODISCARD=1` recovers the whole set. Compilers
accept `[[nodiscard]]` earlier than the standard obliges them to; the switch
takes them up on it, and suppresses the `-Wc++17-extensions` complaint that GCC
and Clang raise under `-Wpedantic` so such a build stays usable.

It is opt-in, and applies to every compiler at once rather than switching
itself on wherever it happens to work — a configuration where MSVC rejects code
that GCC accepts is precisely the divergence Needful exists to avoid.

Note that for MSVC this closes the gap completely: `/std:c++11` is not a real
option there (it is an unrecognized flag, silently ignored), so C++14 is the
floor with or without a `/std:` switch — and C++14 with the switch was measured
to fire every check in the C++17 row.

### Warnings must be errors

The C++17 checks arrive as **warnings**. Without escalation they are log noise,
not enforcement:

| toolchain | minimum to see it | to make it fail |
|---|---|---|
| GCC / Clang | on by default (`-Wunused-result`) | `-Werror`, or `-Werror=unused-result` |
| MSVC | `/W1` (C4834) | `/WX`, or `/we4834` |

MSVC needs no `/Zc:__cplusplus`; the C++17 detection reads `_MSVC_LANG`.

The one check that fails a build on its own is the type separation — refusing
to convert a `Result(T)` to a `T` is a hard error, and it holds all the way
down to C++11.

## Setup: Result Hooks

`Result(T)` needs to know how to store, retrieve, and clear the thread-local
error state. You provide this by defining hook functions or macros:

```c
ErrorType* Needful_Test_And_Clear_Failure(void);
ErrorType* Needful_Get_Failure(void);
void       Needful_Set_Failure(ErrorType* error);
void       Needful_Panic_Abruptly(ErrorType* error);
void       Needful_Assert_Not_Failing(void);
```

For quick prototyping, define `NEEDFUL_DECLARE_RESULT_HOOKS 1` **before**
including `needful.h` to get a built-in `const char*`-based implementation:

```c
#define NEEDFUL_DECLARE_RESULT_HOOKS 1
#include "needful.h"
```

> **Note:** `NEEDFUL_DECLARE_RESULT_HOOKS` defines storage and implementations
> inline. Only define it in **one** translation unit per program.

## `Result(None)` — Fallible Functions With No Return Value

C does not allow `return value;` in a `void`-returning function, which means
`Result(void)` can't work: you can't write `return needful_make_failure(...)` because there's
nothing legal to return. `None` is a unit type defined precisely to fill this
gap:

```c
Result(None) Do_Something(void) {
    if (some_condition)
        return needful_make_failure("something went wrong");
    return none;  // success: return the unit value
}
```

`Result(None)` expands to `None` (an enum with a single zero value) in C
builds, and to a wrapper in C++ builds.  The C++ version ensures the compiler
checks that you handled the result rather than silently discarding it.

> **Background:** Proposals to allow `return` of void expressions in C++ have
> been rejected (see [P0146R0](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0146r0.html)).
> `None` is Needful's practical workaround.

## Related

- [`Option(T)`](/option) — nullable values without an error
- [FAQ: Why does `needful_make_failure(...)` disable the int-conversion warning?](/faq#int-conversion-warning)

---

## Compile-Time Tests

### Basic `return_if_failed` / `make_failure` / `assert_not_failed` usage

<!-- doctest: positive-test -->
```cpp
#include "assert.h"

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#define NEEDFUL_DECLARE_RESULT_HOOKS  1  // use some simple default hooks
#include "needful.h"

Result(int) double_if_positive(int x) {
    if (x < 0)
        return make_failure("negative input");
    return x * 2;
}

Result(int) run(void) {
    return_if_failed (
      int a = double_if_positive(10)
    );
    assert(a == 20);
    return_if_failed (int b = double_if_positive(5));
    assert(b == 10);
    return 42;
}

int main() {
    assert_not_failed (run());
    return 0;
}
```

### Discarding a `Result(T)` without handling it is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert   <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang
// MATCH-ERROR-TEXT: cannot initialize  <- GCC alternate

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#define NEEDFUL_DECLARE_RESULT_HOOKS  1  // use some simple default hooks
#include "needful.h"

#define fail  make_failure  // project-local keyword-style alias

Result(int) compute(int x) {
    if (x < 0)
        return fail ("negative");
    return x * 2;
}

int main() {
    int n = compute(5);  // ERROR: Result(int) is not implicitly int
    NEEDFUL_UNUSED(n);
    return 0;
}
```
