// test-needful-casts.cpp
// Tests for needful cast macros: type correctness, macro comma safety, m_cast.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#define NEEDFUL_CPP_ENHANCED  1
#include <cassert>  // must include before needful.h when using enhancements
#include "needful.h"

#include <cstdarg>

#define STATIC_ASSERT_SAME(T1,T2) \
    static_assert(std::is_same<T1,T2>::value, "Types are not the same")

template<typename T>
struct MyWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD(T, value);
};

void test_cast_types() {
    char data[] = "some data";
    char* mdata = data;
    const char* cdata = data;

    // Casts to non-pointer types are constexpr-eligible
    constexpr int trivial = needful_lenient_hookable_cast(int, 'a');
    NEEDFUL_UNUSED(trivial);

    constexpr int u_trivial = needful_lenient_unhookable_cast(int, 'a');
    NEEDFUL_UNUSED(u_trivial);

    // m_cast strips constness
    STATIC_ASSERT_SAME(
        decltype(needful_mutable_cast(char*, cdata)),
        char*
    );

    // m_cast can also keep or add const
    STATIC_ASSERT_SAME(
        decltype(needful_mutable_cast(const char*, cdata)),
        const char*
    );
    STATIC_ASSERT_SAME(
        decltype(needful_mutable_cast(const char*, mdata)),
        const char*
    );

    // Lenient hookable cast: const propagates from source
    STATIC_ASSERT_SAME(
        decltype(needful_lenient_hookable_cast(char*, cdata)),
        const char*   // lenient: keeps source constness
    );
    STATIC_ASSERT_SAME(
        decltype(needful_lenient_hookable_cast(char*, mdata)),
        char*         // source is mutable, result is mutable
    );

    NEEDFUL_UNUSED(mdata);
    NEEDFUL_UNUSED(cdata);
}

// Macro comma safety: cast macros must not produce bare commas inside <>
// when used as arguments to other macros, since the C preprocessor does
// not treat angle brackets as delimiters.
//
#define SINGLE_ARG_MACRO(x) NEEDFUL_USED(x)
#define OUTER_MACRO(arg) SINGLE_ARG_MACRO(arg)

void test_cast_macro_commas() {
    OUTER_MACRO(needful_c_cast(int, 0));

    OUTER_MACRO(needful_lenient_hookable_cast(int, 0));
    OUTER_MACRO(needful_lenient_unhookable_cast(int, 0));

    OUTER_MACRO(needful_rigid_hookable_cast(int, 0));
    OUTER_MACRO(needful_rigid_unhookable_cast(int, 0));

    OUTER_MACRO(needful_upcast(int, 0));

    OUTER_MACRO(needful_hookable_downcast nullptr);
    OUTER_MACRO(needful_unhookable_downcast nullptr);

    intptr_t i = 0;
    int* ip = nullptr;
    void(*fp)() = nullptr;
    void *vp = nullptr;

    OUTER_MACRO(needful_integer_cast(int, 10));
    OUTER_MACRO(needful_pointer_cast(int*, i));
    OUTER_MACRO(needful_pointer_cast(intptr_t, ip));
    OUTER_MACRO(needful_function_cast(void(*)(int), fp));
    OUTER_MACRO(needful_valist_cast(va_list*, vp));

    const int* cip = nullptr;
    OUTER_MACRO(needful_mutable_cast(int*, cip));
}

#undef SINGLE_ARG_MACRO
#undef OUTER_MACRO

void test_mutability_cast_upcast() {
    struct Base {};
    struct Derived : Base {};

    // Casting away const while upcasting is legal
    const Derived* cd = nullptr;
    Base* b = needful_mutable_cast(Base*, cd);
    NEEDFUL_UNUSED(b);
}

int main() {
    test_cast_types();
    test_cast_macro_commas();
    test_mutability_cast_upcast();
    return 0;
}
