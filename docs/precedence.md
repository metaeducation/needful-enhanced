---
layout: default
title: Operator Precedence
nav_order: 12
permalink: /precedence
---

# Precedence — Why `unwrap x->field` Doesn't Compile

Needful's prefix keywords — `unwrap`, `opt`, `needed`, `nocast`, `downcast` —
are not keywords. C has no way to add one. They are ordinary C++ operators
wearing a disguise, and the disguise slips in a few predictable places.

Knowing the one rule below turns every one of those places from a puzzle into
an obvious missing pair of parentheses.

## The rule

```c
#define needful_unwrap  needful::g_unwrap_helper +
```

That is the whole trick. `unwrap` is a global object followed by a `+`, so:

```c
unwrap foo    /* really is */    (g_unwrap_helper + foo)
```

Which gives the rule:

> **A prefix keyword reaches exactly as far as a `+` would.**
> Anything that binds *tighter* than `+` grabs the operand first — and gets the
> wrapper instead of the unwrapped value. Anything *looser* sees the unwrapped
> value and behaves.

You do not have to memorize C++'s precedence table to use this. Ask one
question: *would a `+` there have grabbed what I wanted?*

## What works, and what needs parentheses

Every row below was compiled, not reasoned about.

| expression | | why |
|---|---|---|
| `unwrap x` | ✅ | — |
| `unwrap x + 1` | ✅ | same precedence, left-associative: `(helper + x) + 1` |
| `unwrap x << 1` | ✅ | `<<` is looser |
| `unwrap x < 10` | ✅ | comparison is looser |
| `unwrap x == 7` | ✅ | `==` is looser |
| `unwrap x && y` | ✅ | `&&` is looser |
| `unwrap x ? a : b` | ✅ | `?:` is looser |
| `r = unwrap x` | ✅ | `=` is looser |
| `f(unwrap x)` | ✅ | an argument is its own expression |
| `unwrap x * 2` | ❌ | `*` binds tighter — tries `Option * 2` |
| `! unwrap x` | ❌ | prefix `!` binds tighter — applies to the *helper* |
| `* unwrap p` | ❌ | prefix `*` binds tighter, same reason |
| `unwrap n->field` | ❌ | `->` binds tightest of all |
| `unwrap a + unwrap b` | ❌ | left-assoc chains: `((helper + a) + helper) + b` |

The fix is always the same, and always local:

```c
(unwrap x) * 2
! (unwrap x)
* (unwrap p)
(unwrap n)->field
(unwrap a) + (unwrap b)
```

The `->` row is the one that bites in practice, because
`unwrap Find_Node()->name` is such a natural thing to type. Reach for
`(unwrap Find_Node())->name`.

### These are always compile errors, never wrong answers

This matters more than the inconvenience. The helper types (`UnwrapHelper`,
`OptHelper`, `NeededHelper`) are empty structs with **no conversion
operators**, so a misparse cannot quietly produce a plausible value — there is
simply no `operator+` that matches, and the build stops. A precedence surprise
in Needful costs you a compile error and a pair of parentheses, never a bug.

## Why `+`, and why `%` for extraction

`Result(T)` extraction uses a *postfix* `%`:

```c
return_if_failed (int v = Compute());
/* expands to */   int v = Compute() % g_result_extractor;
```

The two have to interlock. When both appear at once —

```c
return_if_failed (Foo* foo = opt Some_Function());
/* expands to */   Foo* foo = g_opt_helper + Some_Function() % g_result_extractor;
```

— the `%` must bind **tighter** than the `+`, so the `Result` wrapper is peeled
off first and `opt` then sees a plain `Option(Foo*)`. In C++ precedence, `%` is
multiplicative (tighter) and `+` is additive (looser), so this works out.

Those two are *adjacent* levels, with nothing in between. Since extraction must
beat the keywords, and both must stay looser than `==` and `=`, this is the only
adjacent pair in the language that satisfies every constraint.

`<<` is sometimes suggested for the keyword instead. It is worse, and not only
for the shift-versus-comparison warnings it draws: `<<` binds *looser* than `+`,
so `unwrap x + 1` would parse as `helper << (x + 1)` and fail. The `+` spelling
is what makes that case work.

## The extraction rule: put the call last

Because `%` sits at multiplicative precedence, it ties with `*`, `/`, and `%`
in the surrounding expression, and left-associativity resolves the tie the
wrong way:

```c
return_if_failed (r = Compute() * 2);   /* ERROR */
/* groups as */    r = (Compute() * 2) % g_result_extractor;
```

> **A `Result(T)`-returning call must be the last thing in the statement handed
> to `return_if_failed` and friends.**

This is not much of a restriction in practice — the vocabulary is designed
around one call per statement — but it explains the error when you hit it. Do
the arithmetic afterwards:

```c
return_if_failed (int v = Compute());
r = v * 2;
```

## `nocast` and `downcast` follow a different rule

These look like the same prefix trick, and the `+` half is the same. But they
do not return a value — they return a small proxy object with a *templated*
conversion operator, which needs a target type to deduce:

```c
Derived* d = downcast base_ptr();     /* OK: target type is Derived* */
r = (downcast base_ptr())->x;         /* ERROR: nothing to deduce from */
```

Parenthesizing does not help here, because the problem is not precedence.
`downcast` needs somewhere for the result to *land*: a typed variable, a typed
function parameter, or a `return` in a function with a declared return type.
Give it a named variable and use that.

## Related

- [`Option(T)`](/option) — where `unwrap` and `opt` are defined
- [`Need(T)`](/need) — where `needed` is defined
- [`Result(T)`](/result) — the `%` extraction machinery
- [`nocast`](/nocast) — the deduction-proxy family

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
#define NEEDFUL_DECLARE_RESULT_HOOKS  1  // use some simple default hooks
#include "needful.h"

struct Node { int field; };

static Node g_node = { 42 };
static int g_five = 5;

static Option(int) Seven(void) { return 7; }
static Option(int) Three(void) { return 3; }
static Option(int*) Five_Ptr(void) { return &g_five; }
static Option(Node*) Find_Node(void) { return &g_node; }

static Result(int) Compute(void) { return 20; }

// The extraction rule: the Result(T) call is last, arithmetic comes after.
//
static Result(int) Doubles_Afterward(void) {
    return_if_failed (int v = Compute());
    return v * 2;
}

int main() {
    Option(int) x = Seven();
    int y = 1;

    // Rows that work: everything binding looser than `+` sees a plain int.
    //
    assert(unwrap x + 1 == 8);
    assert((unwrap x << 1) == 14);
    assert(unwrap x < 10);
    assert(unwrap x == 7);
    assert(unwrap x && y);
    assert((unwrap x ? 1 : 0) == 1);

    int r = unwrap x;                          // `=` is looser
    assert(r == 7);

    // Rows that need parentheses: everything binding tighter than `+`.
    //
    assert((unwrap x) * 2 == 14);
    assert((! (unwrap x)) == 0);
    assert(* (unwrap Five_Ptr()) == 5);
    assert((unwrap Find_Node())->field == 42);
    assert((unwrap x) + (unwrap Three()) == 10);

    assert(opt x + 1 == 8);                    // `opt` parses identically

    assert_not_failed (int d = Doubles_Afterward());
    assert(d == 40);

    return 0;
}
```
