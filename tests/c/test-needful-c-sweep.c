/*
**  file: %test-needful-c-sweep.c
**  summary: "Compile and exercise every needful.h macro as plain C"
**
**=/////////////////////////////////////////////////////////////////////////=//
**
** Needful's central promise is that a C codebase adopting it stays C--the
** C++ enhancements are an optional checking layer, not a requirement.  Every
** other test in this suite compiles as C++ with NEEDFUL_CPP_ENHANCED=1, so
** nothing was verifying the half of the promise that actually ships.
**
** This test names every public macro at least once in a plain C translation
** unit, and runs the ones with runtime behavior.  It is deliberately breadth-
** first rather than deep: the C definitions are transparent by design, so the
** interesting failure mode is "this macro does not compile as C at all"--
** which is exactly what a sweep catches and a targeted test does not.
**
** It found five real defects when first written (a corruption assertion that
** could never fire, an unreachable form that was a syntax error in C, two
** macros that silently depended on optional shorthand flags, and a macro pair
** that existed only in the C++ build).
**
**=//// NOTES /////////////////////////////////////////////////////////////=//
**
** A. NEEDFUL_DECLARE_RESULT_HOOKS defines storage and function bodies, so it
**    may only appear in one translation unit.  That is fine here because this
**    test is a single file, but it is why this cannot simply be #included
**    into a larger fixture.
**
** B. Corruption is turned on so that Corrupt_If_Needful() and
**    Assert_Corrupted_If_Needful() are real code rather than no-ops.  The
**    companion test %test-corrupt-assert-fires.c covers the failure
**    direction, which cannot be checked from inside a passing process.
*/

#define NEEDFUL_DEFINE_ALL_SHORTHANDS  1
#define NEEDFUL_DECLARE_RESULT_HOOKS   1  /* one TU only, see [A] */
#define NEEDFUL_DOES_CORRUPTIONS       1  /* see [B] */
#include "needful.h"

#include <stdio.h>
#include <stdint.h>

typedef enum { COLOR_0 = 0, COLOR_RED = 1, COLOR_BLUE = 2 } Color;
typedef struct { int a; int b; } Pair;

static int g_value = 42;
static int g_checks = 0;

#define CHECK(cond) do { \
    ++g_checks; \
    if (! (cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
  } while (0)


/*=== Need(T) / Option(T) / Fallible(T) ==================================*/

Need(int*) needs_ptr(void) { return &g_value; }
Option(int*) maybe_ptr(int yes) { return yes ? &g_value : nullptr; }
Option(Color) maybe_color(int yes) { return yes ? COLOR_RED : none; }
NeedfulFallible(int*) fallible_ptr(int yes) { return yes ? &g_value : nullptr; }


/*=== unreachable family =================================================*/

int unreachable_scalar(Color c) {
    switch (c) {
      case COLOR_0: return 0;
      case COLOR_RED: return 1;
      case COLOR_BLUE: return 2;
      default: needful_unreachable;
    }
}

void unreachable_returns_void(int x) {
    if (x >= 0) return;
    needful_unreachable_void;
}

Pair unreachable_returns_struct(int x) {
    if (x >= 0) { Pair p = needful_struct_0; return p; }
    needful_unreachable_struct(Pair);
}

int dead_end_user(int x) {
    if (x >= 0) return x;
    needful_dead_end;
}


/*=== Result(T) ==========================================================*/

Result(int) double_if_positive(int x) {
    if (x < 0) return make_failure("negative input");
    return x * 2;
}

Result(int*) result_pointer(int x) {
    if (x < 0) return make_failure("negative input");
    return &g_value;
}

Result(None) result_none(int x) {
    if (x < 0) return make_failure("negative input");
    return none;
}

Result(int) propagates_success(void) {
    return_if_failed (int a = double_if_positive(10));
    return a;
}

Result(int) propagates_failure(void) {
    return_if_failed (int a = double_if_positive(-1));
    return a;  /* not reached */
}

int catches_failure(void) {
    int out = 0;
    double_if_positive(-1) catch_if_failed (const char* e) {
        USED(e);
        out = -1;
    } else {
        out = 1;
    }
    return out;
}

int catches_success(void) {
    int out = 0;
    double_if_positive(10) catch_if_failed (const char* e) {
        USED(e);
        out = -1;
    } else {
        out = 1;
    }
    return out;
}


/*=== none-reactive ======================================================*/

Option(int*) uses_return_if_none(int yes) {
    int* p;
    return_if_none (p = fallible_ptr(yes));
    return p;
}

int* uses_tolerate(int yes) {
    int* p;
    tolerate_none (p = fallible_ptr(yes));
    return p;
}

int* uses_abort_if_none(void) {
    int* p;
    abort_if_none (p = fallible_ptr(1));  /* engaged: must not abort */
    return p;
}

int* uses_assert_not_none(void) {
    int* p;
    assert_not_none (p = fallible_ptr(1));  /* must run p= even under NDEBUG */
    return p;
}

int* uses_infallible(void) {
    return infallible fallible_ptr(1);  /* expression-position discharge */
}

/* The none vocabulary must work on a Fallible(T) whose T is not a pointer.
** This is the case the old *_if_nullptr spelling could not express: an enum
** compared against (void*)0 is a constraint violation, and needful.h's own
** -Wint-conversion suppression meant GCC/Clang stayed silent about it in C
** while the C++ enhanced build hard-errored.
*/
typedef enum { SWEEP_NONE_0 = 0, SWEEP_RED = 1 } SweepColor;

Fallible(SweepColor) fallible_enum(int yes) {
    return yes ? SWEEP_RED : SWEEP_NONE_0;
}

Option(SweepColor) uses_return_if_none_enum(int yes) {
    SweepColor c;
    return_if_none (c = fallible_enum(yes));
    return c;
}

/* Fallible(T) carries a must-use annotation, which is only legal on a
** function's return type.  `static` is the case that decides whether the
** annotation can lead the declaration: the GNU spelling may sit anywhere
** among the declaration specifiers, while C23's [[nodiscard]] must come
** first, so `static Fallible(int*) f(void)` is a syntax error under a
** C23-only compiler.  That is why NEEDFUL_MUSTUSE prefers __attribute__
** where both are available, and this exercises the combination.
*/
#if defined(NEEDFUL_FALLIBLE_C23_MUSTUSE) && NEEDFUL_FALLIBLE_C23_MUSTUSE
    /* The documented cost of the C23-only path, reproduced here rather than
    ** described: `static` cannot precede an attribute that must lead the
    ** declaration, and no reordering rescues it.  Drop the `static`.
    */
    Fallible(int*) static_fallible_ptr(int yes) {
        return yes ? &g_value : nullptr;
    }
#else
    static Fallible(int*) static_fallible_ptr(int yes) {
        return yes ? &g_value : nullptr;
    }
#endif

/* FallibleVar(T) is the annotation-free spelling, for the positions where
** Fallible(T) is deliberately illegal.  Same type, different legality.
*/
FallibleVar(int*) g_fallible_slot;

struct FallibleHolder {
    FallibleVar(int*) field;
};

static int* takes_fallible_param(FallibleVar(int*) p) {
    return needful_opt p;
}

int* uses_fallible_var(int yes) {
    FallibleVar(int*) held = static_fallible_ptr(yes);
    struct FallibleHolder holder;
    holder.field = held;
    g_fallible_slot = held;
    return takes_fallible_param(holder.field);
}


/*=== Sink(T) / Init(T) / Contra(T) / Exact(T) ===========================*/

void writes_out(Sink(int) out) { *out = 7; }
void inits_out(Init(int) out) { *out = 8; }
void contra_out(Contra(int) out) { *out = 9; }
int exact_in(Exact(int) v) { return v; }


/*=== ENABLEABLE argument subsetting =====================================*/

int enableable_plain(ENABLEABLE(int, v)) { return v; }

int enableable_convertible(ENABLEABLE(int, v) ENABLE_IF_ARG_CONVERTIBLE_TO(int))
    { return v; }

int enableable_exact(ENABLEABLE(int, v) ENABLE_IF_EXACT_ARG_TYPE(int))
    { return v; }


/*=== global-scope commentary macros =====================================*/

IMPOSSIBLE(sizeof(int) == 0);
DEFINITELY(sizeof(int) >= 2);
POSSIBLY(sizeof(int) == 4);
UNNECESSARY(sizeof(int));
DONT(sizeof(int));
CANT(sizeof(int));


/*=== sweeps that only need to compile ===================================*/

static int cast_sweep(void) {
    int i = 5;
    void* vp = &i;
    const int* cip = &i;

    int* p1 = cast(int*, vp);
    int* p2 = raw_cast(int*, vp);
    int* p3 = fast_cast(int*, vp);
    int* p4 = c_cast(int*, vp);
    int* p5 = m_cast(int*, cip);
    Color c1 = i_cast(Color, 1);
    Color c2 = ii_cast(Color, 1);
    intptr_t n = p_cast(intptr_t, &i);
    int* p6 = nocast vp;
    int* p7 = needful_nocast_0;

    CHECK(p1 == &i);  CHECK(p2 == &i);  CHECK(p3 == &i);
    CHECK(p4 == &i);  CHECK(p5 == &i);  CHECK(p6 == &i);
    CHECK(c1 == COLOR_RED);  CHECK(c2 == COLOR_RED);
    CHECK(n != 0);  CHECK(p7 == nullptr);
    return 0;
}

static int known_sweep(void) {
    int i = 5;
    int* p = &i;
    const int* cp = &i;

    int* k1 = known(int*, p);
    int* k2 = rigid_known(int*, p);
    const int* k3 = lenient_known(int*, cp);
    int* k4 = known_not(char*, p);
    int* k5 = known_any((int*, char*), p);
    int* k6 = c_cast_known(int*, p);
    int* k7 = rigid_exactly(int*, p);
    int* k8 = known_literal(int*, p);
    int* k9 = known_lvalue(p);

    CHECK(k1 == &i);  CHECK(k2 == &i);  CHECK(k3 == &i);
    CHECK(k4 == &i);  CHECK(k5 == &i);  CHECK(k6 == &i);
    CHECK(k7 == &i);  CHECK(k8 == &i);  CHECK(k9 == &i);
    return 0;
}

static int comment_sweep(int i) {
    possibly(i < 0);
    impossible(i == 0x7FFFFFFF);
    definitely(i == i);
    inapplicable(i + 1);
    unnecessary(i + 1);
    dont(i + 1);
    cant(i + 1);
    heeded(i);
    STATIC_ASSERT(sizeof(int) >= 2);
    STATIC_ASSERT_LVALUE(i);
    STATIC_IGNORE(sizeof(int));
    NOOP;
    return 0;
}

static int corruption_sweep(void) {
    int x = 5;
    USED(x);
    Corrupt_If_Needful(x);
    Assert_Corrupted_If_Needful(x);  /* passes: x really is corrupt */
    return 0;
}


/*=== main ===============================================================*/

int main(void) {
    int out = 0;

    if (cast_sweep() != 0) return 1;
    if (known_sweep() != 0) return 1;
    if (comment_sweep(1) != 0) return 1;
    if (corruption_sweep() != 0) return 1;

    writes_out(&out);   CHECK(out == 7);
    inits_out(&out);    CHECK(out == 8);
    contra_out(&out);   CHECK(out == 9);
    CHECK(exact_in(11) == 11);

    CHECK(enableable_plain(1) == 1);
    CHECK(enableable_convertible(2) == 2);
    CHECK(enableable_exact(3) == 3);

    CHECK(unreachable_scalar(COLOR_RED) == 1);
    unreachable_returns_void(1);
    CHECK(unreachable_returns_struct(1).a == 0);
    CHECK(dead_end_user(4) == 4);

    CHECK(needs_ptr() == &g_value);
    CHECK(maybe_ptr(1) == &g_value);
    CHECK(maybe_ptr(0) == nullptr);
    CHECK(maybe_color(1) == COLOR_RED);
    CHECK(maybe_color(0) == COLOR_0);
    CHECK(fallible_ptr(1) == &g_value);

    assert_not_failed (int d = double_if_positive(21));
    CHECK(d == 42);

    assert_not_failed (int s = propagates_success());
    CHECK(s == 20);

    CHECK(extract_failure(propagates_failure()) != nullptr);
    CHECK(catches_failure() == -1);
    CHECK(catches_success() == 1);

    assert_not_failed (result_none(1));
    assert_not_failed (int* rp = result_pointer(1));
    CHECK(rp == &g_value);

    CHECK(uses_return_if_none(1) == &g_value);
    CHECK(uses_return_if_none(0) == nullptr);
    CHECK(uses_tolerate(0) == nullptr);
    CHECK(uses_abort_if_none() == &g_value);
    CHECK(uses_assert_not_none() == &g_value);
    CHECK(uses_infallible() == &g_value);

    CHECK(is_none(fallible_ptr(0)));
    CHECK(! is_none(fallible_ptr(1)));

    CHECK(uses_return_if_none_enum(1) == SWEEP_RED);
    CHECK(uses_return_if_none_enum(0) == SWEEP_NONE_0);
    CHECK(is_none(fallible_enum(0)));
    CHECK(! is_none(fallible_enum(1)));

    /* `none` is readable, not only writable */
    CHECK(fallible_ptr(0) == none);
    CHECK(fallible_ptr(1) != none);
    CHECK(fallible_enum(0) == none);
    CHECK(fallible_enum(1) != none);

    CHECK(static_fallible_ptr(1) == &g_value);
    CHECK(uses_fallible_var(1) == &g_value);
    CHECK(uses_fallible_var(0) == nullptr);

    printf("test-needful-c-sweep: %d checks passed\n", g_checks);
    return 0;
}
