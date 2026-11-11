---
layout: default
title: "Building C as C++"
nav_order: 61
permalink: /c-as-cpp
---

# Building C as C++ -- What Breaks, and the Workaround for Each

The checked build asks a C++ compiler to read your C. C is not a subset of
C++, so the first attempt on an existing codebase usually fails before Needful
has checked anything.

This page lists the C constructs a C++ compiler rejects and gives a fix for
each. [`nocast`](/nocast) covers the two most common ones. The rest need a
small source change, and most of those changes are improvements to the C as
well.

The results here were checked with GCC 13 and Clang 18 at C++11, C++17 and
C++20. Notes about MSVC were not re-verified for this page.

## The Checked Build Is a Lint Pass {#lint-pass}

Three facts make this easier than it first looks.

**It never ships, so it never has to link.** You are using the C++ compiler as
a static analyzer. Ask it to stop after checking:

```sh
g++ -x c++ -std=c++17 -fsyntax-only -Werror -DNEEDFUL_CPP_ENHANCED=1 file.c
```

(`/Zs` is the MSVC spelling of `-fsyntax-only`.)

**It does not have to cover every file.** Start with one translation unit.
A file that leans on something with no workaround can stay out of the checked
build. It gets no checking, and nothing else is affected. The headers that a
checked file includes do have to parse as C++.

**If you do link it, link all of it.** Running your tests under the enhanced
build is worth doing, but enhanced and plain objects [must not be
mixed](/setup#abi).

## Quick Reference {#quick-reference}

| C construct | C++ compiler says | Fix |
|---|---|---|
| `T* p = malloc(n);` | invalid conversion from `void*` | [`nocast`](/nocast) |
| Enum set from an `int` expression, or `e++` | invalid conversion from `int` | [`nocast` or `i_cast`](#enums) |
| Identifier named `new`, `class`, `this`, `not`... | syntax error | [Rename](#keywords) |
| `{ .y = 2, .x = 1 }` | designator order | [Declaration order](#designated-initializers) |
| `{ .in.a = 1 }` | nested designator | [Nested braces](#designated-initializers) |
| `{ [RED] = "red" }` | varies by compiler | [See below](#array-designators) |
| `goto cleanup;` over `int h = ...;` | jump crosses initialization | [Scope the variable](#jumps) |
| `case 2:` after `int x = 10;` | jump to case label | [Brace the case](#jumps) |
| `&(struct P){1, 2}` | address of temporary | [Name it](#compound-literals) |
| `void f();` then `f(1, 2)` | too many arguments | [Write the prototype](#smaller-items) |
| `char* s = "text";` | string constant to `char*` | [`const char*`](#smaller-items) |
| `char tag[3] = "abc";` | initializer too long | [List the characters](#smaller-items) |
| `struct S s = { some_long };` | narrowing conversion | [`i_cast`](#smaller-items) |
| `int x; int x;` at file scope | redefinition | [Define once](#smaller-items) |
| `const int limit;` | uninitialized const | [Initialize it](#smaller-items) |
| Struct defined inside a struct | incomplete type | [Hoist it](#smaller-items) |
| `each(on_int, x)` with mismatched callback | invalid conversion | [`f_cast`](#smaller-items) |
| `int *restrict p` | syntax error | [One macro](#restrict) |
| `int a[static 4]` | syntax error | [One macro](#restrict) |
| `_Static_assert`, `_Noreturn`, `_Alignof` | not keywords in C++ | [Portable spellings](#c11) |
| `_Generic(...)` | syntax error (GCC) | [Overloads](#c11) |
| `_Atomic`, `<stdatomic.h>` | fails before C++23 (GCC) | [Isolate](#c11) |
| Variable-length arrays | extension or error | [Replace them](#no-workaround) |

## Enum Arithmetic {#enums}

C lets any `int` become an enum. C++ does not, which breaks flag enums and
increments:

```c
enum Flags f = F_A | F_B;     /* C++: invalid conversion from int */
e++;                          /* C++: no operator++ for the enum  */
```

Either say what you mean with `i_cast`, or use `nocast` with parentheses:

```c
enum Flags f = i_cast(enum Flags, F_A | F_B);
e = nocast (e | F_B);
e = i_cast(enum Flags, e + 1);
```

The parentheses after `nocast` matter. `nocast F_A | F_B` applies `nocast` to
`F_A` alone. That is a compile error and never a wrong answer; see
[Operator Precedence](/precedence).

## C++ Keywords Used as Identifiers {#keywords}

These are ordinary names in C and reserved in C++:

`new` `delete` `class` `this` `template` `typename` `namespace` `operator`
`public` `private` `protected` `virtual` `friend` `explicit` `mutable` `using`
`try` `catch` `throw` `export`

The alternative operator spellings are reserved too, and are easy to forget:
`and` `or` `not` `xor` `bitand` `bitor` `compl` `and_eq` `or_eq` `xor_eq`
`not_eq`.

In your own code, rename them (`new_`, `klass`, `self`). It is a mechanical
edit.

In a header you cannot edit, rename them for the duration of the include:

```c
#if defined(__cplusplus)
  #define class  class_
  #define new    new_
#endif
#include "vendor.h"
#if defined(__cplusplus)
  #undef class
  #undef new
#endif
```

Code that touches such a field then needs one accessor that spells it both
ways:

```c
#if defined(__cplusplus)
  #define DEV_CLASS(d)  ((d)->class_)
#else
  #define DEV_CLASS(d)  ((d)->class)
#endif
```

Field names are not part of the ABI, so this is safe for a build that only
checks. Keep the `#define` tightly around the one include. Redefining a
keyword across a standard header is undefined behavior.

## Designated Initializers {#designated-initializers}

C++ only gained designated initializers in C++20, and in a narrower form than
C99. What each compiler accepts:

| Form | GCC 13 | Clang 18 |
|---|---|---|
| `{ .x = 1, .y = 2 }` in declaration order | accepted | accepted |
| `{ .y = 2, .x = 1 }` out of order | error | warning |
| `{ .in.a = 1 }` nested designator | error | warning |
| `{ 1, .z = 3 }` mixed with positional | error at C++20 | warning |

A warning is an error in a build that uses `-Werror`, as the checked build
should. So the portable rules are:

- **Write designators in declaration order.** C does not care, so this costs
  nothing.
- **Use nested braces, not nested designators:**
  `{ .in = { .a = 1 }, .k = 2 }`.
- **Do not mix designated and positional initializers in one list.**

Below C++20, `-Wpedantic` flags even the in-order form on both compilers, and
MSVC needs `/std:c++20` to accept designators at all. **If your code uses
designated initializers, run the checked build as C++20.** Needful's enhanced
headers support it.

### Array designators {#array-designators}

`[index] = value` is not part of any C++ standard:

```c
const char *names[N_COLORS] = { [RED] = "red", [GREEN] = "green" };
```

GCC accepts it only when the indices are in order with no gaps. Clang accepts
it with a `-Wc99-designator` warning. Two ways out:

```c
/* 1. Positional, with the assumption written down and checked */
const char *names[N_COLORS] = { "red", "green", "blue" };
STATIC_ASSERT(RED == 0 && GREEN == 1 && BLUE == 2);

/* 2. Keep the designators, in index order, and pass -Wno-c99-designator
      to Clang in the checked build */
```

## Jumps Past an Initialization {#jumps}

This one is very common, because of the `goto cleanup` idiom. C allows a jump
over a declaration that has an initializer. C++ does not:

```c
if (a < 0) goto cleanup;
int h = open_it();            /* C++: jump to label crosses initialization */
close_it(h);
cleanup:
return a;
```

Give the variable a scope that ends before the label:

```c
if (a < 0) goto cleanup;
{
    int h = open_it();
    close_it(h);
}
cleanup:
return a;
```

Declaring the variable above the first `goto` and assigning it later also
works. The same rule applies to `switch`. Brace any `case` that declares a
variable:

```c
case 1: {
    int x = 10;
    return x; }
case 2:
    return 2;
```

## Compound Literals {#compound-literals}

`(struct P){1, 2}` is a C99 feature that GCC and Clang accept in C++ as an
extension, with one difference that matters: in C++ it is a temporary, so its
address cannot be taken. MSVC does not accept the syntax in C++.

```c
takep(&(struct P){1, 2});     /* C++: taking address of temporary */
```

Name the object:

```c
struct P tmp = { 1, 2 };
takep(&tmp);
```

To return a zeroed struct, use [`needful_struct_0`](/nocast).

## Smaller Items {#smaller-items}

Each of these is a one-line fix, and each leaves the C better than it was.

**Empty parameter lists.** Before C23, `void f();` means "arguments
unspecified" in C. In C++ it means "no arguments", so a call that passes any
fails. Write the real prototype, or `(void)`.

**String literals.** `char *s = "text";` discards a `const` that C++ insists
on. Declare `const char *s`. Where an old API demands `char*`, use
[`m_cast`](/cast).

**Exact-fit character arrays.** `char tag[3] = "abc";` drops the terminator in
C and is an error in C++. Write `{ 'a', 'b', 'c' }`.

**Narrowing inside braces.** `struct S s = { some_long };` narrows silently in
C. C++11 rejects narrowing in a braced initializer: Clang as an error, GCC as
a warning. Make the conversion visible with `i_cast(int, some_long)`.

**Tentative definitions.** `int counter;` twice at file scope is one object in
C and a redefinition in C++. Define it once. Use `extern` in headers.

**`const` without an initializer.** `const int limit;` is legal C and an error
in C++. Initialize it.

**Structs defined inside structs.** In C the inner tag is visible at file
scope. In C++ it is scoped to the outer struct, so a later `struct Inner x;`
names a different, incomplete type. Define the inner struct first, at file
scope.

**A typedef and a struct tag with the same name but different types.**
`typedef int Foo; struct Foo { ... };` is legal C. Rename one.

**Function pointer mismatches.** Passing `void (*)(int*)` where
`void (*)(void*)` is expected draws a warning in C and an error in C++. Fix
the callback's signature if you can: calling a function through a pointer of
the wrong type is undefined in both languages. Where a pointer is only being
stored and will be cast back before the call, use [`f_cast`](/cast).

**`register`.** It was removed in C++17. Delete it; no current compiler uses
the hint.

## `restrict` and Array Parameter Qualifiers {#restrict}

`restrict` is not a C++ keyword, but GCC, Clang and MSVC all provide
`__restrict`. One definition in a common header is enough:

```c
#if defined(__cplusplus)
  #define restrict __restrict
#endif
```

`int a[static 4]` has no C++ equivalent. Hide it the same way:

```c
#if defined(__cplusplus)
  #define AT_LEAST(n)
#else
  #define AT_LEAST(n)  static n
#endif

int sum(int n, int a[AT_LEAST(4)]);
```

## C11 Features {#c11}

**Underscore keywords.** `_Static_assert`, `_Noreturn` and `_Alignof` are not
keywords in C++. Clang accepts them anyway; GCC does not. Use the spellings
that work in both languages:

| Instead of | Write | From |
|---|---|---|
| `_Static_assert(c, "m")` | `STATIC_ASSERT(c)` or `static_assert(c, "m")` | Needful, or `<assert.h>` |
| `_Noreturn` | `NEEDFUL_NORETURN` | Needful |
| `_Alignof(T)` | `alignof(T)` | `<stdalign.h>` |

**`_Generic`.** Clang accepts it in C++ as an extension. GCC does not. C++
has overloading for the same job, so give each language its own form behind
one name:

```c
#if defined(__cplusplus)
    inline const char *type_name(int)    { return "int"; }
    inline const char *type_name(double) { return "double"; }
#else
    #define type_name(x)  _Generic((x), int: "int", double: "double")
#endif
```

This is practical for a handful of generic macros. A codebase built around
`_Generic` will find it tedious, and those files may be better left out of
the checked build.

**Atomics.** With GCC, `<stdatomic.h>` and `_Atomic` only work in C++ from
C++23. Clang accepts them earlier. If you cannot check at C++23, keep atomic
declarations in a small header and leave the files that include it out of the
checked build.

## No Workaround Offered {#no-workaround}

**Variable-length arrays.** GCC and Clang accept them in C++ as an extension.
MSVC does not accept them at all. Needful does nothing for them and does not
intend to. Replace them with a fixed bound or an allocation.

## Differences That Compile but Mean Something Else {#silent-differences}

These do not stop the build. They matter only if you *run* the C++ build, or
if a static assertion depends on them:

- `sizeof('a')` is `sizeof(int)` in C and `1` in C++.
- A `const` object at file scope has external linkage in C and internal
  linkage in C++.
- `inline` functions follow different rules about where the one definition
  lives.

A build that stops at `-fsyntax-only` is unaffected by the last two.

## The Fixes Together {#worked-example}

Every fix on this page that is standard C++ appears below, in a program that
is extracted and built by CI as part of the enhanced checks:

<!-- doctest: positive-test -->
```cpp
#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
  #define NEEDFUL_CPP_ENHANCED  1
#endif
#define NEEDFUL_DEFINE_ALL_SHORTHANDS  1
#include "needful.h"

enum Flags { F_NONE = 0, F_A = 1, F_B = 2 };

struct Inner { int a; };                 // hoisted out of Outer
struct Outer { struct Inner in; int b; };

struct Small { int n; unsigned char b; };

static void bump(void* p) {
    int* ip = cast(int*, p);
    *ip += 1;
}
static void bump_int(int* p) { *p += 1; }

typedef void (Callback)(void*);
typedef void (IntCallback)(int*);
static void each(Callback* cb, void* ctx) { cb(ctx); }

static int scoped_jump(int a) {
    if (a < 0)
        goto cleanup;
    {
        int h = a * 2;                   // scope ends before the label
        a = h;
    }
  cleanup:
    return a;
}

static int braced_case(int a) {
    switch (a) {
      case 1: {
        int x = 10;                      // braces keep `case 2` legal
        return x; }
      case 2:
        return 2;
    }
    return 0;
}

int main() {
    int* p = nocast malloc(sizeof(int));          // void* -> int*
    *p = 0;

    enum Flags f = i_cast(enum Flags, F_A | F_B); // int -> enum
    f = nocast (f | F_NONE);                      // parentheses required
    assert(f == 3);

    long big = 40;
    int v = 7;
    struct Small s = { i_cast(int, big), i_cast(unsigned char, v) };
    assert(s.n == 40 && s.b == 7);

    char tag[3] = { 'a', 'b', 'c' };              // exact fit
    const char* greeting = "hello";               // const char*
    assert(tag[2] == 'c' && greeting[0] == 'h');

    struct Outer o = { { 1 }, 2 };
    struct Inner in = o.in;                       // Inner is a real type
    assert(in.a == 1 && o.b == 2);

    each(&bump, p);
    Callback* stored = f_cast(Callback*, &bump_int);     // visible cast
    IntCallback* restored = f_cast(IntCallback*, stored);
    restored(p);                                  // called as its real type
    assert(*p == 2);

    assert(scoped_jump(-1) == -1 && scoped_jump(4) == 8);
    assert(braced_case(1) == 10 && braced_case(2) == 2);

    free(p);
    return 0;
}
```
