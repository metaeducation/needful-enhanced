// test-needful-need.cpp
// Tests for Need(T): non-nullable, non-bool-coercible required values.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#define NEEDFUL_CPP_ENHANCED  1
#include <cassert>  // must include before needful.h when using enhancements
#include "needful.h"

#include <type_traits>

#define STATIC_ASSERT_SAME(T1,T2) \
    static_assert(std::is_same<T1,T2>::value, "Types are not the same")


//=//// Need(T) BASIC CONSTRUCTION AND ACCESS /////////////////////////////=//

void test_need_pointer_basic() {
    int value = 42;
    Need(int*) p = &value;

    // Implicit conversion to T gives back the pointer
    int* raw = p;
    assert(*raw == 42);

    // operator-> works
    struct Box { int x; };
    Box b { 99 };
    Need(Box*) bp = &b;
    assert(bp->x == 99);
}

void test_need_unwrap() {
    int value = 1020;
    Need(int*) p = &value;

    // needed extracts the contained value; implicit conversion also works
    int* raw = p;  // implicit conversion via operator T()
    assert(raw == &value);
    assert(*raw == 1020);
}


//=//// Need(T) EQUALITY AND INEQUALITY ///////////////////////////////////=//

void test_need_equality() {
    int a = 1, b = 2;
    Need(int*) pa = &a;
    Need(int*) pb = &b;
    Need(int*) pa2 = &a;

    assert(pa == pa2);
    assert(pa != pb);
    assert(pa == &a);    // NeedWrapper == raw
    assert(&a == pa);    // raw == NeedWrapper
    assert(pa != &b);
    assert(&b != pa);
}


//=//// Need(T) COPY AND ASSIGNMENT ///////////////////////////////////////=//

void test_need_copy_assign() {
    int x = 10;
    Need(int*) p1 = &x;
    Need(int*) p2 = p1;      // copy construct
    assert(p1 == p2);

    int y = 20;
    p2 = &y;                  // assign from raw (non-null)
    assert(p2 == &y);
    assert(p1 != p2);
}


//=//// Need(T) CONSTRUCTION IS BLOCKED FOR nullptr AND none ////////////=//
//
// Need() provides a runtime guarantee of non-null, but the compile-time
// blocking is specifically for nullptr_t and NoneStruct (not bool coercion,
// which goes through the implicit operator T() -> T* -> bool path).

void test_need_nullptr_blocked() {
    // Need(T*) is NOT constructible from nullptr_t (deleted constructor)
    STATIC_ASSERT(not std::is_constructible<Need(int*), std::nullptr_t>::value);

    // Need(T*) is NOT constructible from NoneStruct (deleted constructor)
    STATIC_ASSERT(not std::is_constructible<Need(int*), needful::NoneStruct>::value);

    // Need(T*) is NOT implicitly convertible to bool -- even though T* itself
    // is (pointer-to-bool is a standard conversion).  The deleted operator
    // bool() wins overload resolution over the operator T() → bool two-step.
    STATIC_ASSERT(not std::is_convertible<Need(int*), bool>::value);

    // Same for non-pointer T: Need(int) is not bool-convertible either.
    STATIC_ASSERT(not std::is_convertible<Need(int), bool>::value);
}


//=//// Need(T) WITH NON-POINTER TYPES ////////////////////////////////////=//

void test_need_integer() {
    Need(int) n = 42;
    int raw = n;
    assert(raw == 42);
    assert(n == 42);
    assert(42 == n);
}


int main() {
    test_need_pointer_basic();
    test_need_unwrap();
    test_need_equality();
    test_need_copy_assign();
    test_need_nullptr_blocked();
    test_need_integer();
    return 0;
}
