//
//  file: %needful-asserts.hpp
//  summary: "Assertions and commentary macros for C and C++""
//  homepage: https://needful.metaeducation.com/comments
//
//=/////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2026 metaeducation.com
//
// Licensed under the MIT License
//
// https://en.wikipedia.org/wiki/MIT_License
//
//=/////////////////////////////////////////////////////////////////////////=//
//

// NEEDFUL_NODISCARD prefers [[nodiscard]] in C++17+ (or when explicitly
// requested by NEEDFUL_PRECPP17_NODISCARD), otherwise stays a no-op.
//
// 1. __has_cpp_attribute(nodiscard) checks recognition, not validity under
//    the active language standard.
// 2. Older GNU/SAL spellings are function-only and do not cover the class
//    use cases Needful relies on.
//
#if __cplusplus >= 201703L  // C++17 or higher, can't check attribute [1]
    #undef NEEDFUL_NODISCARD
    #define NEEDFUL_NODISCARD  [[nodiscard]]  // the C++17 way (best!)
#elif defined(_MSC_VER) && _MSVC_LANG >= 201703L
    #undef NEEDFUL_NODISCARD
    #define NEEDFUL_NODISCARD  [[nodiscard]]  // the C++17 way (best!)
#elif NEEDFUL_PRECPP17_NODISCARD  // opt-in below C++17 [3]
    #undef NEEDFUL_NODISCARD
    #define NEEDFUL_NODISCARD  [[nodiscard]]
#else
    // leave NEEDFUL_NODISCARD as a no-op [2]

#endif

// 3. NEEDFUL_PRECPP17_NODISCARD opts into compilers that accept the standard
//    attribute before C++17.  See enforcement notes in %docs/result.md.

// 1. We use __VA_ARGS__ here to avoid the need for double-parenthesization
//    at the callsite, e.g. STATIC_ASSERT((std::is_same<T, U>::value)), which
//    is what you'd have to do otherwise if you used templated stuff that
//    has commas in it that the preprocessor would misinterpret.
//
// 2. If we use std::is_convertible<> instead of is_explicitly_convertible<>
//    things like Optional(T) would not count, since the boolean conversion
//    is explicit (but automatic in things like conditions of if() statements)

#undef NEEDFUL_STATIC_ASSERT
#define NEEDFUL_STATIC_ASSERT(...) /* variadic for [1] */ \
    static_assert((__VA_ARGS__), #__VA_ARGS__) // callsite has semicolon

#undef NEEDFUL_STATIC_ASSERT_DECLTYPE_BOOL
#define NEEDFUL_STATIC_ASSERT_DECLTYPE_BOOL(...) /* variadic for [1] */ \
    static_assert( \
        needful_is_explicitly_convertible_v(decltype(__VA_ARGS__), bool), \
        "expression must be explicitly convertible to bool")  // [2]

#undef NEEDFUL_STATIC_ASSERT_DECLTYPE_VALID
#define NEEDFUL_STATIC_ASSERT_DECLTYPE_VALID(...) /* variadic for [1] */ \
    static_assert( \
        std::is_same<decltype((void)(__VA_ARGS__)), void>::value, \
        "declaration type must be valid")


// NEEDFUL_STATIC_FAIL takes a string literal in every mode (see %needful.h).
// This used to stringify its argument with `#msg`, which meant a caller had
// to pass a bare identifier to get a sensible message here and a string to
// get one in C -- mutually exclusive.  needful.h's C++ branch already does
// the right thing, so there is nothing left to override; kept as an explicit
// no-op note so the next reader does not re-add the stringify.


// 1. Double-parentheses needed to force reference qualifiers for decltype.

#undef NEEDFUL_STATIC_ASSERT_LVALUE
#define NEEDFUL_STATIC_ASSERT_LVALUE(x) \
    static_assert( \
        std::is_lvalue_reference<decltype((x))>::value, /* [1] */ \
        "must be lvalue reference")


// In order to make needful.h as palatable as possible to C programmers,
// there isn't `#ifdef __cplusplus` code in it unless it's absolutely
// required.  Hence we sacrifice C++ features unless NEEDFUL_CPP_ENHANCED
// is used, as it's not expected that Needful codebases would be built as
// C++ without the enhancements very often (if ever).
//
// Keeping true to this principle means the C++23 checks are skipped in
// needful.h and only done here as an #undef and then redefinition.

#if __cplusplus >= 202302L
    #undef needful_builtin_unreachable

    #include <utility>  /* C++23 has std::unreachable in <utility> */
    #define needful_builtin_unreachable  std::unreachable()
#endif
