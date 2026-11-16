---
layout: home
title: Needful
nav_order: 1
permalink: /
---

<h1 class="needful-visually-hidden">Needful</h1>

<figure class="needful-hero">
  <img src="{{ '/assets/images/needful-banner.jpg' | relative_url }}"
       alt="Needful (C &amp; C++): Need(T), Sink(T)"
       width="1536" height="1024">
  <figcaption>Rust-grade type checking for C, at zero runtime cost.</figcaption>
</figure>

<ul class="needful-pillars">
  <li><a href="https://raw.githubusercontent.com/metaeducation/needful/refs/heads/main/needful.h">One header</a></li>
  <li><a href="{{ '/design#zero-dependency' | relative_url }}">Zero-dependency shipping builds</a></li>
  <li><span>Your C stays C</span></li>
</ul>

Needful is a [single-file, header-only library][needful-h-raw] that brings
`Option(T)`, `Result(T)`, non-null `Need(T)`, type-safe casts, and executable
comment annotations to C codebases -- without the code ceasing to be C.

## The Thirty-Second Version

Here is a bug every C programmer has shipped. A function can come back empty,
and the only thing that says so is a comment:

```c
User* Find_User(const char* name);  // returns NULL if there's no such user

void Welcome(const char* name) {
    User* user = Find_User(name);
    Send_Mail(user, "Welcome!");    // no warning, even at -Wall... and it
}                                   // crashes the first time there's no user
```

With Needful, the comment moves into the type. One word changes:

```c
Option(User*) Find_User(const char* name);
```

Your ordinary C build does not notice, because this is the *entire*
definition it sees:

```c
#define Option(T)  T
```

Same compiler, same binary, same bug. But now compile that **same file** as
C++ with Needful's checks switched on -- a second job in CI, not a port -- and
the same macro becomes a wrapper type. The line that forgot about `NULL` no
longer builds:

```text
welcome.c(4): error C2440: 'initializing': cannot convert from
    'needful::OptionWrapper<User *>' to 'User *'
```

So you fix it, in C, and both builds pass:

```c
void Welcome(const char* name) {
    Option(User*) user = Find_User(name);
    if (user)
        Send_Mail(unwrap user, "Welcome!");
}
```

`unwrap` is the same trade: it is `#define unwrap` -- nothing at all -- in the
C build, and a checked extraction in the other one.

That is the whole idea. **Ship the C. Let a C++ compiler check it harder.**
There is no runtime library, no code generator, and no tool to install beyond
a C++ compiler.

*(That is MSVC's wording; GCC and Clang say the same thing in their own.
Every example on this page is [compiled in CI](#compile-time-tests), including
the ones that are supposed to fail.)*

### An error you cannot drop on the floor

`Result(T)` carries a failure alongside a return value, and
`return_if_failed` passes it up the stack the way Rust's `?` does:

```c
Result(int) Parse_Port(const char* text) {
    int port = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9')
            return_fail ("port must be a number");
        port = port * 10 + (*text - '0');
    }
    return port;
}

Result(int) Connect(const char* text) {
    return_if_failed (
      int port = Parse_Port(text)  // if this failed, Connect() fails too
    );
    printf("connecting on %d\n", port);
    return 1;
}
```

Forget, and write `int port = Parse_Port(text);` the way C has always let
you? The C build carries on and prints `connecting on 0`. The checked build
refuses: `cannot convert from 'needful::ResultWrapper<int>' to 'int'`.
[More on `Result(T)`...](/result)

### A comment that cannot go stale

```c
void Reset(Buffer* b) {
    possibly(b->len == 0);   // may be empty already, and that's fine
    dont(Free_Buffer(b));    // tempting, but the caller still owns it
    b->len = 0;
}
```

Neither line generates any code, in any build. But they are real expressions,
so the checked build has to be able to compile them. Rename `len`, or change
what `Free_Buffer()` takes, and the comment that would have quietly become a
lie is a build error instead: `'len': is not a member of 'Buffer'`.

These ask nothing of your types or your build, which makes them the cheapest
part of Needful to try. [More on comment macros...](/comments)

### What it is not

It is a type-discipline layer, not a memory-safety layer. It catches
mis-modeled values and interface mistakes; it does not prevent use-after-free,
buffer overruns, data races, or overflow bugs. The rest of the bad news is
collected, without softening, under [The Weak Points](#the-weak-points).

---

## How It Works (No Surprises)

The trick is architectural, not magical: the same source compiles in more
than one mode, and the macros change role from transparent vocabulary to
strict checking wrappers.

Needful has three practical build modes:

| Build mode | What you include | Dependency profile | Typical purpose |
|---|---|---|---|
| C (default) | `needful.h` | Single header + standard C headers only | Production/release |
| C++ (unenhanced) | `needful.h` | Single header + standard C/C++ headers only | Compatibility check pass |
| C++ enhanced (`NEEDFUL_CPP_ENHANCED=1`) | `needful.h` + `needful-enhanced/` companion tree | Extra C++ metaprogramming headers (for example `type_traits`) | Strong static-analysis pass in CI |

So "zero dependencies" means: in shipping builds (C, or C++ without
enhancement), Needful adds no runtime library and no companion-tree
requirement. The extra dependencies are only for the optional enhanced check
mode, which is exactly where the heavier C++ machinery lives.

---

## Getting Started

**Step 1.** Drop [`needful.h`][needful-h-raw] into your project and
`#include` it. All macros expand to trivial C - your build won't even notice.

**Step 2.** When ready, add the [`needful-enhanced/`][needful-enhanced-repo]
enhancement files alongside `needful.h`. *(Put this directory in your
.gitignore, don't commit it.)*  `#define NEEDFUL_CPP_ENHANCED  1` and build as
[C++11 or later][cpp11-baseline].  The macros grow teeth: type mismatches
become compile errors.

If the C++ compiler rejects your C before any Needful check runs, that is
expected on a first attempt: see [Building C as C++](/c-as-cpp).

**Step 3.** Run both modes in CI: C build for production, C++ enhanced build to
catch bugs. No code changes needed between them:

```yaml
- run: gcc -o app main.c                                  # what you ship
- run: g++ -x c++ -std=c++17 -Werror -DNEEDFUL_CPP_ENHANCED=1 -fsyntax-only main.c
```

Two things in that second line matter more than they look. The type errors
shown at the top of this page are hard errors regardless, but the *must-use*
checks (a `Result(T)` or `Fallible(T)` that is ignored outright) arrive as
warnings, so they need `-Werror` / `/WX` to fail a build, and C++17 to exist
at all. [Setup](/setup) has the details, per compiler.

---

## What You Get

| Construct | What It Does |
|---|---|
| [`Need(T)`](/need) | Non-null/non-zero type; blocks boolean coercion |
| [`Option(T)`](/option) | Rust-like optional with same size as `T` |
| [`Fallible(T)`](/fallible) | Must-use Option(T) for return types; `FallibleVar(T)` elsewhere |
| [`Status(T)`](/status) | Zero-means-okay return code: must-use, and `if (status)` won't compile |
| [`Result(T)`](/result) | Multiplexed error + return value; auto-propagation via `return_if_failed` |
| [`cast()` family](/cast) | Visible, hookable, semantically-named casts |
| [`nocast`](/nocast) | Restores C's implicit `void*` and enum conversions in a C++ build |
| [Comment macros](/comments) | `possibly()`, `dont()`, `heeded()` -- comments that fail the build when they go stale |
| [`Contra(T)` / `Sink(T)` / `Init(T)`](/contra) | Contravariant output parameter markers |
| [`known(T, expr)`](/known) | Zero-cost compile-time type assertion, for macro authors |

Two of these are worth a second look, because their names undersell them:

- **[`nocast`](/nocast)** is what makes "just compile my C as C++" survive
  contact with a real codebase. Without it, step 2 above buries you in errors
  about `void*` returns from `malloc()` and `int`-to-enum assignments that C
  was perfectly happy with. The incompatibilities it does not cover, and the
  fix for each, are in [Building C as C++](/c-as-cpp).

- **[Comment macros](/comments)** are the only part of Needful with no
  equivalent in Rust, C++, or anything else -- and the only part that asks for
  no type changes at all. If you want to try one thing, try these.

---

## Design Goals

- **No magic.** [The C definitions are written out in full][needful-h-raw] so
  you can see how trivial they are. Adding Needful to a C project is a
  low-impact proposition.

- **Gradual adoption.** Use just `nocast` today. Add `Option` next month.
  Enable `NEEDFUL_CPP_ENHANCED` when you're ready. Nothing breaks.

- **Codebase documentation.** The comment macros (`possibly`, `definitely`,
  `impossible`, `unnecessary`, `dont`) replace prose comments with
  compile-checked expressions that keep themselves up-to-date.

- **Documentation that compiles.** The same idea, turned on this site. Every
  example here that demonstrates a checked behavior is extracted and built in
  CI on GCC, Clang, and MSVC -- the positive ones must compile *and run*, and
  the negative ones must **fail** to compile, for the stated reason. A sample
  that drifts out of date does not quietly mislead you; it turns the build
  red. The links between these pages are checked the same way.

---

## Who This Is For

Needful is most useful when all of these are true:

- You have a C codebase where nullability, error propagation, and output
  parameter misuse produce expensive bugs.

- You can run an occasional checked C++ build in CI, even if release artifacts
  are still built as C.

- You want stronger static checking without introducing runtime dependencies or
  rewriting the codebase into another language.

If your primary objective is memory safety, this is the wrong tool to lead
with. If your objective is stronger value and API discipline in existing C,
this is exactly the niche.

---

## The Weak Points

Most projects show you a softball. Here are the hardballs, up front, so you
find them now instead of three weeks in.

**It is not a memory-safety solution.** Needful is a *type discipline* layer.
It will not stop a use-after-free, a buffer overrun, a data race, or an
integer overflow. What it catches is the class of bug where a value's *meaning*
was misunderstood: a maybe-null treated as never-null, an error return dropped
on the floor, an output parameter given the wrong end of a type hierarchy.
That is a real and valuable class. It is not the one that gets CVEs assigned.

**Enforcement is not uniform, and one common configuration checks nothing.**
`Fallible(T)` makes three promises and no single build delivers all three;
MSVC in C mode below C23 delivers *none* of them. The full matrix is
[documented honestly](/fallible#enforcement) rather than buried -- read it
before you decide which build is your real gate.

**Warnings must be errors or you get nothing.** Most of what Needful rejects
arrives as a warning, and the compiler then exits zero. Without `-Werror` /
`/WX`, CI stays green over exactly the mistakes you added Needful to catch.

**The checked build needs a second repository.** We say "one header, no
dependencies," and that is true of the shipping build. The *checking* build
wants `needful-enhanced/` cloned next to it and version-matched. It is a
deliberate trade -- but it is the point where the simple story stops being
simple.

**Your C may not parse as C++ yet.** C is not a subset of C++. A `goto` over
an initialization, a designated initializer written out of order, a variable
named `new`: each one stops the checked build before Needful has looked at
anything. [`nocast`](/nocast) covers the two most common breaks, and
[Building C as C++](/c-as-cpp) has a workaround for most of the rest. Bringing
an existing codebase across is still real work, and a few constructs have no
workaround at all.

**It is a dialect.** There are a couple hundred macros. The vocabulary you
actually need is more like twenty-five, but the cast family alone has fifteen
spellings with lenient and rigid variants. Expect to spend time on taxonomy,
and expect a new contributor to ask what `unwrap` is.

**The prefix keywords are a trick, and tricks leak.** `unwrap foo` is really
`g_unwrap_helper * foo`, which means precedence surprises (an entire
[page](/precedence) exists for them) and tooling -- formatters, highlighters,
code search -- that is mildly wrong forever. The saving grace is that a
misparse is *always* a compile error and never a wrong answer.

**Threading is explicit configuration, not magic.** `Result(T)` default hooks
are single-threaded and should be replaced with thread-local storage in
threaded programs. Corruption helpers (`NEEDFUL_DOES_CORRUPTIONS`) are
thread-safe by default in enhanced C++ builds (atomics), with optional lock
hooks and explicit single-thread mode for constrained environments.

If none of those are dealbreakers, the rest of this site is the good parts.

---

## Frequently Asked Questions

Common gotchas and design rationale are collected in the [FAQ](/faq).
Threaded integration details are collected in [Threading](/threading).

---

## Compile-Time Tests

The examples from the top of this page, as the complete programs CI builds.
The ones marked as errors must *fail* to compile, for the reason given.

### The fixed `Welcome()` compiles, and survives a missing user

<!-- doctest: positive-test -->
```cpp
#include <assert.h>
#include <string.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

typedef struct { const char* name; const char* email; } User;

User g_ada = { "ada", "ada@example.com" };
int g_mails_sent = 0;

Option(User*) Find_User(const char* name) {
    if (strcmp(name, g_ada.name) == 0)
        return &g_ada;
    return none;
}

void Send_Mail(User* to, const char* text) {
    assert(to->email and text);
    ++g_mails_sent;
}

void Welcome(const char* name) {
    Option(User*) user = Find_User(name);
    if (user)
        Send_Mail(unwrap user, "Welcome!");
}

int main() {
    Welcome("ada");
    Welcome("bob");  // no such user: the unfixed version crashes here
    assert(g_mails_sent == 1);
    return 0;
}
```

### The original `Welcome()` is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

typedef struct { const char* name; const char* email; } User;

Option(User*) Find_User(const char* name);
void Send_Mail(User* to, const char* text);

void Welcome(const char* name) {
    User* user = Find_User(name);  // ERROR: might be none, must unwrap
    Send_Mail(user, "Welcome!");
}

int main() {
    return 0;
}
```

### `return_if_failed` propagates a failure

<!-- doctest: positive-test -->
```cpp
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#include "needful.h"
#include "needful-result-hooks-singlethreaded.h"  // simple default hooks

Result(int) Parse_Port(const char* text) {
    int port = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9')
            return_fail ("port must be a number");
        port = port * 10 + (*text - '0');
    }
    return port;
}

Result(int) Connect(const char* text) {
    return_if_failed (
      int port = Parse_Port(text)  // if this failed, Connect() fails too
    );
    printf("connecting on %d\n", port);
    return 1;
}

int main() {
    int connected = 0;
    int failed = 0;

    Connect("8080") catch_if_failed (const char* e) {
        NEEDFUL_UNUSED(e);
        ++failed;
    } else {
        ++connected;
    }

    Connect("http") catch_if_failed (const char* e) {
        assert(strcmp(e, "port must be a number") == 0);
        ++failed;
    } else {
        ++connected;
    }

    assert(connected == 1 and failed == 1);
    return 0;
}
```

### Taking a `Result(T)`'s value without handling failure is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: cannot convert        <- GCC, MSVC
// MATCH-ERROR-TEXT: no viable conversion  <- Clang

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_RESULT_SHORTHANDS  1
#include "needful.h"
#include "needful-result-hooks-singlethreaded.h"

Result(int) Parse_Port(const char* text);

Result(int) Connect(const char* text) {
    int port = Parse_Port(text);  // ERROR: and in C, port would just be 0
    NEEDFUL_UNUSED(port);
    return 1;
}

int main() {
    return 0;
}
```

### Comment macros compile, and generate nothing

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_COMMENT_SHORTHANDS  1
#include "needful.h"

typedef struct { int len; int capacity; char* data; } Buffer;

int g_frees = 0;

void Free_Buffer(Buffer* b) {
    NEEDFUL_UNUSED(b);
    ++g_frees;
}

void Reset(Buffer* b) {
    possibly(b->len == 0);   // may be empty already, and that's fine
    dont(Free_Buffer(b));    // tempting, but the caller still owns it
    b->len = 0;
}

int main() {
    Buffer b = { 3, 8, nullptr };
    Reset(&b);
    assert(b.len == 0);
    assert(g_frees == 0);  // dont() really didn't
    return 0;
}
```

### A comment macro naming a field that no longer exists is a compile error

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: no member named   <- GCC, Clang
// MATCH-ERROR-TEXT: is not a member   <- MSVC

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_COMMENT_SHORTHANDS  1
#include "needful.h"

typedef struct { int length; int capacity; char* data; } Buffer;  // was `len`

void Reset(Buffer* b) {
    possibly(b->len == 0);  // ERROR: the comment is stale
    b->length = 0;
}

int main() {
    return 0;
}
```

[needful-h-raw]: https://raw.githubusercontent.com/metaeducation/needful/refs/heads/main/needful.h
[needful-repo]: https://github.com/metaeducation/needful/
[needful-enhanced-repo]: https://github.com/metaeducation/needful-enhanced/
[cpp11-baseline]: https://needful.metaeducation.com/faq#cpp11-baseline
