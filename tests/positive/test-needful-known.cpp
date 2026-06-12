// test-needful-known.cpp
// Tests for known(T,expr), known_not, known_any, known_lvalue, exactly,
// rigid vs lenient const passthrough, and known_literal.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_KNOWN_SHORTHANDS  1
#include "needful.h"

#include <type_traits>


//=//// known(T, expr): LENIENT CONST PASSTHROUGH /////////////////////////=//
//
// known() asserts the expression is convertible to T, but passes through the
// expression unchanged (preserving lvalue-ness and constness).
// It uses a comma expression so decltype gives a reference type for lvalues.
// The key claim is that the returned value is assignable to the right type.

void test_lenient_known_mutable() {
    int x = 10;
    int* p = &x;

    // Mutable source: result assigns to int*
    int* result = lenient_known(int*, p);
    assert(result == p);
    assert(*result == 10);
}

void test_lenient_known_const_passthrough() {
    int x = 42;
    const int* cp = &x;

    // Const source passes through; result assigns to const int*
    const int* result = lenient_known(int*, cp);
    assert(result == cp);
    assert(*result == 42);
}


//=//// rigid_known(T, expr): ENFORCES EXACT CONST ///////////////////////=//
//
// rigid_known does NOT do const passthrough; it requires exactly T.

void test_rigid_known_exact() {
    int x = 5;
    int* p = &x;

    // Mutable to mutable: fine
    int* result = rigid_known(int*, p);
    assert(result == p);
}


//=//// known_not(T, expr): TYPE IS NOT T /////////////////////////////////=//

void test_known_not() {
    int x = 7;
    void* vp = &x;

    // void* is not int*: should compile
    void* result = lenient_known_not(int*, vp);
    assert(result == vp);
}


//=//// known_any((T1,T2,...), expr) //////////////////////////////////////=//

void test_known_any() {
    int x = 3;
    int* p = &x;

    // int* is one of {int*, char*}: should compile
    int* result = known_any((int*, char*), p);
    assert(result == p);
}


//=//// known_lvalue(var): STATIC ASSERT THAT ARGUMENT IS AN LVALUE ///////=//

void test_known_lvalue() {
    int x = 42;

    // known_lvalue passes the lvalue through unchanged
    int& ref = known_lvalue(x);
    assert(ref == 42);
    assert(&ref == &x);
}


//=//// lenient_exactly / rigid_exactly ///////////////////////////////////=//
//
// lenient_exactly(T, expr): expr must be EXACTLY constify(T)
//   — designed for accepting const expressions and treating them as const T
// rigid_exactly(T, expr): expr must be EXACTLY T

void test_lenient_exactly_const() {
    int x = 100;
    const int* cp = &x;

    // cp is const int*; lenient_exactly(int*, cp) checks const int* == constify(int*) = const int*
    const int* result = lenient_exactly(int*, cp);
    assert(result == cp);
}

void test_rigid_exactly() {
    int x = 99;
    int* p = &x;

    // p is int*; rigid_exactly(int*, p) checks int* == int*
    int* result = rigid_exactly(int*, p);
    assert(result == p);
}


//=//// known() WITH WRAPPER TYPES ////////////////////////////////////////=//
//
// Verify that lenient_known() works with simple struct types that have
// a wrapped_type typedef (via NEEDFUL_DECLARE_WRAPPED_FIELD).

struct SimpleWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD(int, value);
    SimpleWrapper() : value{0} {}
    explicit SimpleWrapper(int v) : value{v} {}
    explicit operator bool() const { return value != 0; }
};

void test_known_with_wrapper() {
    SimpleWrapper w;
    SimpleWrapper* wp = &w;

    // lenient_known on a non-const wrapper pointer: result is SimpleWrapper*
    SimpleWrapper* result = lenient_known(SimpleWrapper*, wp);
    assert(result == wp);
}


int main() {
    test_lenient_known_mutable();
    test_lenient_known_const_passthrough();
    test_rigid_known_exact();
    test_known_not();
    test_known_any();
    test_known_lvalue();
    test_lenient_exactly_const();
    test_rigid_exactly();
    test_known_with_wrapper();
    return 0;
}
