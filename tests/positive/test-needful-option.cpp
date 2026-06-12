// test-needful-option.cpp
// Tests for Option(T): type safety, unwrap/opt, none, implicit conversion
// blocking, and explicit bool conversion.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_OPTION_SHORTHANDS  1
#include "needful.h"

#define STATIC_ASSERT  NEEDFUL_STATIC_ASSERT
#define STATIC_ASSERT_SAME(T1,T2) \
    static_assert(std::is_same<T1,T2>::value, "Types are not the same")

// A minimal user-defined wrapper that works with Option(T).
// Must be bool-testable and support zero-construction for none.
template<typename T>
struct MyWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD(T, value);

    MyWrapper() = default;

    MyWrapper(const T& init) : value {init} {}

    MyWrapper(needful::Nocast0Struct) : value {0} {}

    explicit operator bool() const {
        return value != 0;
    }
};

void test_option_int() {
    Option(int) oi1 = 42;
    Option(int) oi2 = none;

    assert(oi1);
    assert(not oi2);

    // Implicit conversion to int is blocked
    STATIC_ASSERT(not needful_is_convertible_v(decltype(oi1), int));

    // unwrap and opt return the right type
    STATIC_ASSERT_SAME(decltype(unwrap oi1), int);
    STATIC_ASSERT_SAME(decltype(opt oi1), int);

    int v1 = unwrap oi1;
    assert(v1 == 42);

    int v2 = opt oi2;
    assert(v2 == 0);
}

void test_option_enum() {
    enum MyEnum { A = 0, B, C };

    Option(MyEnum) oe1 = B;
    Option(MyEnum) oe2 = none;

    assert(oe1);
    assert(not oe2);

    // Implicit conversion to MyEnum is blocked
    STATIC_ASSERT(not needful_is_convertible_v(decltype(oe1), MyEnum));

    // Explicit bool construction is allowed; implicit bool conversion is not
    STATIC_ASSERT(not needful_is_convertible_v(decltype(oe1), bool));
    STATIC_ASSERT(needful_is_constructible_v(decltype(oe1), bool));

    STATIC_ASSERT_SAME(decltype(unwrap oe1), MyEnum);
    STATIC_ASSERT_SAME(decltype(opt oe1), MyEnum);
}

void test_option_pointer() {
    Option(const char*) op1 = "abc";
    Option(const char*) op2 = none;
    Option(char*) op3 = nullptr;

    assert(op1);
    assert(not op2);
    assert(not op3);

    // Implicit conversion to const char* is blocked
    STATIC_ASSERT(not needful_is_convertible_v(decltype(op1), const char*));

    // Explicit bool construction allowed; implicit bool conversion not
    STATIC_ASSERT(not needful_is_convertible_v(decltype(op1), bool));
    STATIC_ASSERT(needful_is_constructible_v(decltype(op1), bool));

    STATIC_ASSERT_SAME(decltype(unwrap op1), const char*);
    STATIC_ASSERT_SAME(decltype(opt op1), const char*);
}

void test_option_wrapper() {
    Option(MyWrapper<int>) ow1 = MyWrapper<int>{456};
    Option(MyWrapper<int>) ow2 = none;

    assert(ow1);
    assert(not ow2);
}

void test_option_copy() {
    Option(int) oi1 = 99;
    Option(int) oi2 = oi1;
    assert(oi2);
    assert((unwrap oi2) == 99);
}

int main() {
    test_option_int();
    test_option_enum();
    test_option_pointer();
    test_option_wrapper();
    test_option_copy();
    return 0;
}
