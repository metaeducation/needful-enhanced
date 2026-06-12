// test-needful-const.cpp
// Tests for needful constify/unconstify/merge_const type metaprogramming.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#define NEEDFUL_CPP_ENHANCED  1
#include "needful.h"

#define STATIC_ASSERT  NEEDFUL_STATIC_ASSERT

#define STATIC_ASSERT_SAME(T1,T2) \
    static_assert(std::is_same<T1,T2>::value, "Types are not the same")

template<typename T>
struct MyWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD(T, value);
};

void test_constify() {
    STATIC_ASSERT(std::is_same<int, int>::value);  // baseline: embedded <,>

    // Fundamental types are not pointer-constified
    STATIC_ASSERT_SAME(needful_constify_t(int), int);
    STATIC_ASSERT_SAME(needful_unconstify_t(const int), int);

    // Enum types
    enum MyEnum { A, B, C };
    STATIC_ASSERT_SAME(needful_constify_t(MyEnum), MyEnum);
    STATIC_ASSERT_SAME(needful_unconstify_t(const MyEnum), MyEnum);

    // Regular class types
    struct MyClass {};
    STATIC_ASSERT_SAME(needful_constify_t(MyClass), MyClass);
    STATIC_ASSERT_SAME(needful_unconstify_t(const MyClass), MyClass);

    // Pointer types: constify adds const to pointed-to type
    STATIC_ASSERT_SAME(needful_constify_t(int*), const int*);
    STATIC_ASSERT_SAME(needful_unconstify_t(const int*), int*);

    // Template wrapper types propagate const through wrapped field
    STATIC_ASSERT_SAME(
        needful_constify_t(MyWrapper<int>), MyWrapper<const int>
    );
    STATIC_ASSERT_SAME(
        needful_constify_t(MyWrapper<MyClass>), MyWrapper<const MyClass>
    );
    STATIC_ASSERT_SAME(
        needful_unconstify_t(MyWrapper<const MyClass>), MyWrapper<MyClass>
    );
    STATIC_ASSERT_SAME(
        needful_constify_t(MyWrapper<int*>), MyWrapper<const int*>
    );
}

void test_merge_const() {
    // merge_const: if source is const, make dest const

    STATIC_ASSERT_SAME(
        needful_merge_const_t(const int*, char*), const char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(int*, char*), char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(int*, const char*), const char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(const int*, const char*), const char*
    );

    // Same through wrapper types
    STATIC_ASSERT_SAME(
        needful_merge_const_t(MyWrapper<const int*>, char*), const char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(MyWrapper<int*>, char*), char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(MyWrapper<int*>, const char*), const char*
    );
    STATIC_ASSERT_SAME(
        needful_merge_const_t(MyWrapper<const int*>, const char*), const char*
    );
}

int main() {
    test_constify();
    test_merge_const();
    return 0;
}
