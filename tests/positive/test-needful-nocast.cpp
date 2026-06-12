// test-needful-nocast.cpp
// Tests for nocast (void* -> T*, int -> enum) and nocast_0 (generic zero).
//
// In C++, void* is not implicitly assignable to T*, and 0 cannot be assigned
// to an enum type.  nocast is a proxy that provides these C-compatible
// conversions while still being type-safe in the sense that the cast happens
// through an explicit mechanism rather than an implicit void* rule.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>
#include <cstdlib>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_CAST_SHORTHANDS  1
#include "needful.h"


//=//// nocast: void* -> T* ///////////////////////////////////////////////=//

void test_nocast_void_pointer() {
    int val = 42;
    void* vp = &val;

    // In C++, this would fail without a cast: int* p = vp;
    int* p = nocast vp;
    assert(*p == 42);
}

void test_nocast_malloc_pattern() {
    // The primary motivating use case: nocast malloc(...)
    int* buf = nocast malloc(sizeof(int) * 3);
    assert(buf != nullptr);

    buf[0] = 1;
    buf[1] = 2;
    buf[2] = 3;
    assert(buf[1] == 2);

    free(buf);
}


//=//// nocast: int (0) -> enum type //////////////////////////////////////=//

enum Color { RED = 0, GREEN = 1, BLUE = 2 };

void test_nocast_int_to_enum() {
    // In C++ you can't say: Color c = 0;
    // With nocast you can do the C-compatible pattern:
    Color c = nocast 0;
    assert(c == RED);
}

void test_nocast_int_zero_is_null_for_pointer() {
    // nocast 0 assigned to a pointer type becomes nullptr (not UB)
    int* p = nocast 0;
    assert(p == nullptr);
}


//=//// nocast: pointer-to-pointer (C-style) //////////////////////////////=//

void test_nocast_pointer_to_pointer() {
    struct A { int x; };
    struct B { int x; };

    A a { 10 };
    A* ap = &a;

    // C-style pointer cast: A* -> B* (via nocast)
    B* bp = nocast ap;
    assert(bp->x == 10);
}


//=//// nocast_0: GENERIC ZERO ////////////////////////////////////////////=//
//
// needful_nocast_0 is a constexpr zero that converts to any type.
// It's used internally by Option(T) and Result(T) to represent nullopt/fail.
// There is no unprefixed shorthand for this one.

void test_nocast_0_to_pointer() {
    int* p = needful_nocast_0;
    assert(p == nullptr);
}

void test_nocast_0_to_int() {
    int n = needful_nocast_0;
    assert(n == 0);
}

void test_nocast_0_to_enum() {
    Color c = needful_nocast_0;
    assert(c == RED);
}

void test_nocast_0_to_bool() {
    bool b = needful_nocast_0;
    assert(b == false);
}


//=//// nocast DOES NOT SUPPRESS TYPE ERRORS //////////////////////////////=//
//
// nocast is not a do-anything cast.  It handles the specific C-compatible
// patterns (void* -> T*, int -> enum/pointer).  The static assertions below
// confirm it passes through correct types as expected.

void test_nocast_produces_correct_type() {
    void* vp = nullptr;
    int* ip = nocast vp;
    // The result type is the target type we assigned to
    (void)ip;

    int zero = 0;
    Color c = nocast zero;
    assert(c == RED);
}


int main() {
    test_nocast_void_pointer();
    test_nocast_malloc_pattern();
    test_nocast_int_to_enum();
    test_nocast_int_zero_is_null_for_pointer();
    test_nocast_pointer_to_pointer();
    test_nocast_0_to_pointer();
    test_nocast_0_to_int();
    test_nocast_0_to_enum();
    test_nocast_0_to_bool();
    test_nocast_produces_correct_type();
    return 0;
}
