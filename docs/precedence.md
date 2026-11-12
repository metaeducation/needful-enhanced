---
layout: default
title: Operator Precedence
nav_order: 12
permalink: /precedence
---

# Precedence -- Why `unwrap x->field` Doesn't Compile

Needful's prefix keywords -- `unwrap`, `opt`, `needed`, `nocast`, `downcast` --
are not keywords. C has no way to add one. They are ordinary C++ operators
wearing a disguise, and the disguise slips in a few predictable places.

Knowing the one rule below turns every one of those places from a puzzle into
an obvious missing pair of parentheses.

## The rule

```c
#define needful_unwrap  needful::g_unwrap_helper *
```

That is the whole trick. `unwrap` is a global object followed by a `*`
(multiplication, not dereference), so:

```c
unwrap foo    /* really is */    (g_unwrap_helper * foo)
```

Which gives the rule:

> **A prefix keyword reaches exactly as far as a multiplication would.**
> Anything that binds *tighter* than `*` grabs the operand first. Anything
> *looser* sees the unwrapped value and behaves.

Very little binds tighter than multiplication: only the postfix operators
(`->`, `.`, `[]`, calls) and the unary ones (`!`, `-`, `*`, casts). And Needful
[handles the common unary ones itself](#in-front). So the short version is:
**nearly everything just works; member access on the result needs
parentheses.**

## What works, and what needs parentheses

Every row below was compiled, not reasoned about.

| expression | | why |
|---|---|---|
| `unwrap x` | ✅ | -- |
| `unwrap x + 1` | ✅ | `+` is looser |
| `y + unwrap x` | ✅ | `+` is looser, on either side |
| `unwrap a + unwrap b` | ✅ | each keyword finishes before the `+` |
| `unwrap x * 2` | ✅ | same precedence, left-associative: `(helper * x) * 2` |
| `unwrap x << 1` | ✅ | `<<` is looser |
| `unwrap x & mask` | ✅ | `&` is looser |
| `unwrap x < 10` | ✅ | comparison is looser |
| `unwrap x == 7` | ✅ | `==` is looser |
| `unwrap x && y` | ✅ | `&&` is looser |
| `unwrap x ? a : b` | ✅ | `?:` is looser |
| `r = unwrap x` | ✅ | `=` is looser |
| `f(unwrap x)` | ✅ | an argument is its own expression |
| `2 * unwrap x` | ✅ | handled: see [operators in front](#in-front) |
| `unwrap a * unwrap b` | ✅ | same |
| `- unwrap x` | ✅ | same |
| `* unwrap p` | ✅ | same |
| `! opt x` | ✅ | same |
| `! unwrap x` | ❌ | refused: it could only be false |
| `* opt p` | ❌ | refused: `p` may be null |
| `unwrap n->field` | ❌ | `->` binds tighter: this is `unwrap (n->field)` |

The last row is the only one that's about precedence, and the fix is local:

```c
(unwrap n)->field
```

It's not a limitation so much as the honest reading: a prefix operator
applied to `n->field` applies to the field, exactly as `! n->field` and
`* n->field` do in C.

The `->` row is the one that bites in practice, because
`unwrap Find_Node()->name` is such a natural thing to type. Reach for
`(unwrap Find_Node())->name`.

### Operators in front of a keyword {#in-front}

Something written in front of a keyword can bind as tightly as the keyword's
own `*`, or tighter. Then it reaches the *helper object* before the keyword
reaches your value:

```c
2 * unwrap x    /* really is */    (2 * helper) * x
- unwrap x      /* really is */    (- helper) * x
* unwrap p      /* really is */    (* helper) * p
! opt x         /* really is */    (! helper) * x
```

Those would be "no such operator" errors. Needful makes them work instead: an
operator applied to a helper gives back an object that remembers the
operator. When the keyword's `*` then reaches your value, the keyword runs
first and the remembered operator is applied to its result. So each line means
what it looks like it means.

What's supported in front of `unwrap`, `opt` and `needed`:

| in front | `unwrap` | `needed` | `opt` |
|---|---|---|---|
| `x *`, `x /`, `x %` | ✅ | ✅ | ✅ |
| unary `-`, `~` | ✅ | ✅ | ✅ |
| unary `*` (dereference) | ✅ | ✅ | ❌ may be null |
| `!` | ❌ always false | ❌ always false | ✅ |

The three refusals are deliberate, and each has its own error message:

- **`! unwrap x` and `! needed x`** can't mean anything. `unwrap` is only
  legal on an `Option(T)` that holds a value, and a `Need(T)` always holds
  one. Neither can be null or zero, so the test could only be false. The error
  points you at `! x` or `! opt x` if you meant to test the `Option(T)`.

- **`* opt p`** dereferences a pointer in the same breath as saying it might
  be null, which is the bug `Option(T)` exists to stop. Check it, then
  `* unwrap p`.

The remembered operator is applied at the keyword's precedence, not at its
usual one. For these operators that makes no difference to the answer:
`- unwrap x * 2` is `(-x) * 2` either way, and `! opt x == y` is
`(! opt x) == y`, as plain C would have it.

(Anything else in front -- a cast, `&`, `sizeof` -- is still a compile error.
Parenthesize the keyword and its operand.)

### These are always compile errors, never wrong answers

This matters more than the inconvenience. The helper types (`UnwrapHelper`,
`OptHelper`, `NeededHelper`) are empty structs with **no conversion
operators**, so a misparse cannot quietly produce a plausible value -- outside
the handful of forms above there is simply no operator that matches, and the
build stops. A precedence surprise
in Needful costs you a compile error and a pair of parentheses, never a bug.

## Why `*`, and why `->*` for extraction

`Result(T)` extraction uses a *postfix* `->*`:

```c
return_if_failed (int v = Compute());
/* expands to */   int v = Compute() ->* g_result_extractor;
```

(`->*` is C++'s pointer-to-member operator. Almost nobody uses it, it can be
overloaded like any other binary operator, and no pointer-to-member is
involved here -- it was picked purely for where it sits in the precedence
table.)

The two have to interlock. When both appear at once --

```c
return_if_failed (Foo* foo = opt Some_Function());
/* expands to */   Foo* foo = g_opt_helper * Some_Function() ->* g_result_extractor;
```

-- the extraction must bind **tighter** than the keyword, so the `Result`
wrapper is peeled off first and `opt` then sees a plain `Option(Foo*)`.

`->*` is the tightest-binding binary operator C++ has, and `*` is the level
right below it. That makes this the tightest pair available, and tighter is
better here: the tighter a keyword binds, the more of the surrounding
expression is "looser" and so works without parentheses.

### Historical note: it used to be `+` and `%` {#history}

Until November 2026, Needful used `+` for the keywords and `%` for
extraction: the same design, one precedence level lower. If you are reading
older code, comments or discussions that say `g_unwrap_helper + foo` or
`% g_result_extractor`, that is why.

It worked, but it cost more parentheses and it drew a warning:

| expression | with `+` / `%` | with `*` / `->*` |
|---|---|---|
| `unwrap x * 2` | error | works |
| `y + unwrap x` | error | works |
| `unwrap a + unwrap b` | error | works |
| `return_if_failed (r = 2 * Compute())` | error | works |
| `unwrap x << 1` | GCC warning | works |

The warning is what prompted the change. GCC's `-Wparentheses` (part of
`-Wall`) flags an unparenthesized `+` used as the operand of a shift,
and it could not tell the keyword's `+` from a real one. So
`unwrap x << 1` stopped any GCC build using `-Werror`. GCC has no such warning
for `*`, and the tests on this page keep it that way: they use those
expressions unparenthesized, built with `-Wall -Wextra -Werror`.

The move made one thing stricter. `nocast a % b` used to mean
`nocast (a % b)`, because `%` bound tighter than the keyword's `+`. Now the
keyword takes only `a`, and the rest is a compile error. Write the
parentheses. (The same goes for `*` and `/`, and for `downcast`.)

## The extraction rule: put the call last

`->*` binds tighter than every other binary operator, so it attaches to
whatever is immediately to its left -- the last operand in the statement, and
nothing more:

```c
return_if_failed (r = Compute() * 2);   /* ERROR */
/* groups as */    r = Compute() * (2 ->* g_result_extractor);
```

> **A `Result(T)`-returning call must be the last thing in the statement handed
> to `return_if_failed` and friends.**

Arithmetic *before* the call is fine (`r = 2 * Compute()` extracts and then
multiplies). Anything after it is an error, and the fix is to do it afterwards:

```c
return_if_failed (int v = Compute());
r = v * 2;
```

## `nocast` and `downcast` follow a different rule

These look like the same prefix trick, and the `*` half is the same. But they
do not return a value -- they return a small proxy object with a *templated*
conversion operator, which needs a target type to deduce:

```c
Derived* d = downcast base_ptr();     /* OK: target type is Derived* */
r = (downcast base_ptr())->x;         /* ERROR: nothing to deduce from */
```

Parenthesizing does not help here, because the problem is not precedence.
`downcast` needs somewhere for the result to *land*: a typed variable, a typed
function parameter, or a `return` in a function with a declared return type.
Give it a named variable and use that.

For the same reason, arithmetic has to go *inside* the keyword's reach, not
after it. The proxy can't be multiplied or added to:

```c
Color c = nocast (n % 3);     /* OK: the whole expression is converted */
Color c = nocast n % 3;       /* ERROR: (nocast n) % 3, a proxy % 3 */
```

## Related

- [`Option(T)`](/option) -- where `unwrap` and `opt` are defined
- [`Need(T)`](/need) -- where `needed` is defined
- [`Result(T)`](/result) -- the `->*` extraction machinery
- [`nocast`](/nocast) -- the deduction-proxy family

---

## Compile-Time Tests

### Every ✅ row, and every recommended fix for a ❌ row

<!-- doctest: positive-test -->
```cpp
#include <assert.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#define NEEDFUL_RESULT_SHORTHANDS  1
#include "needful.h"
#include "needful-result-hooks-singlethreaded.h"  // simple default hooks

struct Node { int field; };

static Node g_node = { 42 };
static int g_five = 5;

static Option(int) Seven(void) { return 7; }
static Option(int) Three(void) { return 3; }
static Option(int*) Five_Ptr(void) { return &g_five; }
static Option(Node*) Find_Node(void) { return &g_node; }
static Option(Node*) Nothing(void) { return nullptr; }

static Result(int) Compute(void) { return 20; }

// The extraction rule: the Result(T) call is last, arithmetic comes after.
//
static Result(int) Doubles_Afterward(void) {
    return_if_failed (int v = Compute());
    return v * 2;
}

// Arithmetic *before* the call is fine.
//
static Result(int) Doubles_Before(void) {
    return_if_failed (int v = 2 * Compute());
    return v;
}

int main() {
    Option(int) x = Seven();
    int y = 1;

    // Rows that work: everything binding no tighter than `*`.
    //
    assert(unwrap x + 1 == 8);
    assert(y + unwrap x == 8);
    assert(unwrap x + unwrap Three() == 10);
    assert(unwrap x * 2 == 14);
    assert((unwrap x << 1) == 14);
    assert((unwrap x & 3) == 3);
    assert((unwrap x | 8) == 15);
    assert(unwrap x < 10);
    assert(unwrap x == 7);
    assert(unwrap x && y);
    assert((unwrap x ? 1 : 0) == 1);

    int r = unwrap x;                          // `=` is looser
    assert(r == 7);

    // Operators in front of a keyword: handled by the helpers.
    //
    assert(2 * unwrap x == 14);
    assert(unwrap x * unwrap Three() == 21);
    assert(21 / unwrap x == 3);
    assert(15 % unwrap x == 1);
    assert(1 + 2 * unwrap x * 3 == 43);
    assert(- unwrap x == -7);
    assert((~ unwrap x) == ~7);
    assert(* unwrap Five_Ptr() == 5);
    assert(* unwrap Five_Ptr() + 1 == 6);
    assert(2 * opt x == 14);

    Need(int) five = 5;
    Need(int*) five_ptr = &g_five;
    assert(2 * needed five == 10);
    assert(* needed five_ptr == 5);

    * unwrap Five_Ptr() = 6;                   // the dereference is assignable
    assert(g_five == 6);
    g_five = 5;

    // The one row that needs parentheses: postfix operators bind tighter.
    //
    assert((unwrap Find_Node())->field == 42);

    assert(opt x + 1 == 8);                    // `opt` parses identically

    Option(int) empty = none;                  // `! opt` is special-cased
    assert(! opt empty);
    assert((! opt x) == false);
    assert(! opt empty && ! opt Nothing());

    assert_not_failed (int e = Doubles_Before());
    assert(e == 40);

    assert_not_failed (int d = Doubles_Afterward());
    assert(d == 40);

    return 0;
}
```

### `! unwrap x` is a compile error, because it is always false

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: `! unwrap x` is always false

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    Option(int) x = 7;
    if (! unwrap x)  // ERROR: an unwrapped value is never zero
        return 1;
    return 0;
}
```

### `! needed x` is a compile error, because it is always false

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: `! needed x` is always false

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    int value = 42;
    Need(int*) ptr = &value;
    if (! needed ptr)  // ERROR: a Need(T) is never null
        return 1;
    return 0;
}
```

### `* opt p` is a compile error, because `p` may be null

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: `* opt p` would dereference a pointer that may be null

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

int main() {
    int value = 42;
    Option(int*) p = &value;
    int v = * opt p;  // ERROR: check it, then `* unwrap p`
    NEEDFUL_UNUSED(v);
    return 0;
}
```
