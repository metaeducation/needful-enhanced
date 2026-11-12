//
//  file: %needful-wrapping.hpp
//  summary: "Helpers for wrapped type detection and rewrapping"
//  homepage: https://needful.metaeducation.com/need
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
// Wrappers in enhanced mode are thin type-checking shells over one concrete
// implementation type.  They expose ::wrapped_type so Needful can perform
// generic operations (unwrap/rewrap/const-transfer) without per-wrapper
// specializations.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// A. For automatic rewrapping to work, wrapped_type must match the wrapper's
//    actual template parameter.  e.g. don't do this:
//
//        template<typename T>
//        struct MyWrapper {
//           using wrapped_type = T*;
//           T* pointer;
//           ...
//        };
//
//     When you didn't synchronize `wrapped_type` with the actual type then
//     the intelligence to extract the type via an explicit cast and then
//     use `needful_rewrap_type()` to build a new wrapper with a different
//     inner type won't work.
//
//     If you want to do something like this, you can hide the pointer with
//     a macro.  You'll be using a macro anyway, because Needful is for C
//     codebases, so you wouldn't have <> in source anyway!
//
//         #if defined(__cplusplus)
//             #define MyWrap(T) MyWrapper<T*>
//         #else
//             #define MyWrap(T) T*
//         #endif
//


//=//// WRAPPER CLASS DEFINITION ///////////////////////////////////////////=//
//
// Central macro so all wrappers follow the same layout and metadata contract.
//
// 1. If you have a base class which has the actual storage of the wrapped
//    type, and a derived class which just adjusts the access for it, then
//    this is a special case where the derived class may need to override
//    the wrapped_type to be in sync with its template parameter.
//

#define NEEDFUL_DECLARE_WRAPPED_FIELD(T,name) \
    static_assert(std::is_standard_layout<T>::value, \
        "Needful wrapped types must be standard-layout"); \
    using wrapped_type = T; \
    using wrapped_deep_type = needful_deeply_unwrapped_type(T); \
    T name

#define NEEDFUL_OVERRIDE_WRAPPED_FIELD_TYPE(T) /* derived class may use */ \
    static_assert(std::is_standard_layout<T>::value, \
        "Needful wrapped types must be standard-layout"); \
      using wrapped_type = T


//=//// WRAPPER CLASS DETECTION ///////////////////////////////////////////=//
//
// Detect whether T exposes wrapped_type using C++11 detection idiom (void_t).
// The primary template is false_type; specialization is selected only when
// substitution of typename T::wrapped_type succeeds.
//
// 1. Any type exposing `wrapped_type` must be standard-layout, because
//    NEEDFUL_EXTRACT_INNER relies on reinterpret_cast to the first member
//    (guaranteed only for standard-layout structs).  Enforcing this here
//    catches violations at the earliest possible point--when the wrapper
//    type is first examined--rather than at each individual cast site.
//

template<typename T, typename = void>
struct HasWrappedType : std::false_type {};

template<typename T>
struct HasWrappedType<T, void_t<typename T::wrapped_type>>
    : std::true_type
{
    static_assert(
        std::is_standard_layout<T>::value,
        "Wrapper types (with wrapped_type) must be standard-layout"
    );
};


//=//// UNWRAPPING: GET THE INNER TYPE OF A WRAPPER /////////////////////=//

template<typename T, typename = void>
struct UnwrappedType {
    using type = T;
};

template<typename T>
struct UnwrappedType<T, void_t<typename T::wrapped_type>> {
    using type = typename T::wrapped_type;
};

template<typename T, typename = void>
struct DeeplyUnwrappedType {
    using type = T;
};

template<typename T>
struct DeeplyUnwrappedType<T, void_t<typename T::wrapped_deep_type>> {
    using type = typename T::wrapped_deep_type;
};

#define needful_unwrapped_type(T) /* unwrapped IF wrapped, else T */ \
    typename needful::UnwrappedType<T>::type

#define needful_deeply_unwrapped_type(T) /* unwrapped IF wrapped, else T */ \
    typename needful::DeeplyUnwrappedType<T>::type


//=//// REWRAP AN INNER TYPE WITH THE SAME TEMPLATE ///////////////////////=//
//
// This allows you to generically "re-wrap" a type with the same template as
// the original wrapper, but with a different inner type. This is a common
// metaprogramming pattern sometimes called "rebind" in C++ libraries.
//
// 1. Wrapper here is a "template template parameter":
//
//      https://en.cppreference.com/w/cpp/language/template_parameters.html
//

template<typename WrapperType, typename NewInnerType>
struct RewrapHelper;

template<
    template<typename> class Wrapper,
    typename OldInner,
    typename NewInner
>
struct RewrapHelper<Wrapper<OldInner>, NewInner> {
    using type = Wrapper<NewInner>;
};

template<typename Wrapper, typename NewWrapped>
struct RewrapHelper<const Wrapper, NewWrapped> {  // const needs forwarding
    using type = const typename RewrapHelper<Wrapper, NewWrapped>::type;
};

#define needful_rewrap_type(WrapperType, NewInnerType) \
    typename needful::RewrapHelper<WrapperType, NewInnerType>::type


//=//// REWRAPPABLE DETECTION /////////////////////////////////////////////=//
//
// Not all types that expose `wrapped_type` are template instantiations that
// RewrapHelper can decompose.  For example, a plain struct like:
//
//     struct Type {
//         NEEDFUL_DECLARE_WRAPPED_FIELD(TypeEnum, h);
//         ...
//     };
//
// ...has `wrapped_type` (so HasWrappedType is true), but it is not
// `SomeTemplate<TypeEnum>`, so RewrapHelper cannot pattern-match it.
// IsRewrappable detects this so that other metaprogramming (like
// ConstifyHelper) can fall back gracefully for non-template wrappers.
//

template<typename T, typename = void>
struct IsRewrappable : std::false_type {};

template<typename T>
struct IsRewrappable<T,
    enable_if_t<HasWrappedType<T>::value,
        void_t<typename RewrapHelper<T, typename T::wrapped_type>::type>
    >
> : std::true_type {};

#define needful_is_rewrappable_v(T) \
    needful::IsRewrappable<T>::value


//=//// LEAF TYPE IN WRAPPER LAYERS ///////////////////////////////////////=//
//
// LeafPointee: Extract the innermost pointee type through wrapper layers.
//
//     LeafPointee<Slot*>::type             => Slot
//     LeafPointee<ExactWrapper<Slot*>>     => Slot
//     LeafPointee<OnStack(Init(Slot))>     => Slot
//
template<typename T, bool = HasWrappedType<T>::value>
struct LeafPointee { using type = remove_pointer_t<T>; };

template<typename T>
struct LeafPointee<T, true> : LeafPointee<typename T::wrapped_type> {};


//=//// SEMANTIC VS. NON-SEMANTIC WRAPPERS ////////////////////////////////=//
//
// Wrappers fall into two categories that matter for casting behavior:
//
// * "Semantic" wrappers carry meaning beyond the raw type--their wrapping
//   is part of the data's contract.  Option(T) tracks engaged/disengaged
//   state; Result(T) signals that an error may have occurred.  Casting
//   should preserve these wrappers: `cast(U, Result(T))` --> `Result(U)`.
//
// * "Non-semantic" wrappers describe parameter-passing conventions rather
//   than data state: Sink(T) marks a write-through parameter, Need(T) marks
//   a non-null contract, Exact(T) blocks implicit conversions, etc.  Once
//   you cast, you're done with the convention, so cast extracts the inner
//   value: `cast(U, sink_value)` --> `U`.
//
// Specialize `IsWrapperSemantic` to `true_type` for your wrapper if
// cast() should auto-preserve it.  The default is false (extract).
//

template<typename>
struct IsWrapperSemantic : std::false_type {};


//=//// BASIC TYPE DETECTION //////////////////////////////////////////////=//
//
// C++ splits its types into "scalar" (arithmetic + enum + pointer + member
// pointer + nullptr_t) and "compound" (everything else).  But scalar lumps
// pointers together with integers and enums, which is wrong for our cast
// dispatch: pointer-to-pointer casts need reinterpret_cast and CastHook
// validation, while int-to-enum or bool-to-int are fine with static_cast.
//
// IsBasicType captures the subset of types where static_cast is always
// valid and no hook dispatch is needed: fundamentals (int, bool, float...)
// and enums.  For wrapped types, it looks through the wrapper to classify
// based on the inner type (e.g. Need(int) is basic, Need(Foo*) is not).
//
// This drives SFINAE in the cast() overloads:
//   - basic -> basic:  overload 1 (constexpr static_cast, no hooks)
//   - non-basic -> basic:  overload 2 (hooks, but basic target)
//   - everything else:  overloads 3/4/5 (reinterpret_cast territory)
//

template<typename T>
struct IsBasicType {
    using Adjusted = needful_unwrapped_type(T);  // !!! deep?

    static constexpr bool value =
        std::is_fundamental<Adjusted>::value or std::is_enum<Adjusted>::value;
};


//=//// INNER EXTRACTION FROM WRAPPERS ////////////////////////////////////=//
//
// Needful wrappers are standard-layout structs whose first (and only) data
// member is the wrapped value.  C++ guarantees that a pointer to a
// standard-layout struct can be reinterpret_cast to a pointer to its first
// member (and vice versa):
//
//   https://en.cppreference.com/w/cpp/language/data_members#Standard-layout
//
// This is how the casting wrapped conversion operators (which may have
// side effects--e.g. Sink's corruption semantics) can directly access the
// inner value for casting purposes.  The static_assert on is_standard_layout
// in each overload ensures the guarantee actually holds.
//

#define NEEDFUL_EXTRACT_INNER(InnerType, wrapper) \
    reinterpret_cast<const InnerType&>(wrapper)


//=//// OPERATORS IN FRONT OF A PREFIX KEYWORD ////////////////////////////=//
//
// A keyword like `unwrap` is a helper object and a `*`.  So anything written
// in front of it that binds as tightly as `*` (or tighter) reaches the
// *helper* before the keyword reaches its operand:
//
//     2 * unwrap x    =>    (2 * helper) * x
//     - unwrap x      =>    (- helper) * x
//     * unwrap p      =>    (* helper) * p
//
// Rather than have those be errors, the helper plays along.  An operator
// applied to a helper gives back a "pending" object remembering what was
// asked for.  When the keyword's own `*` then reaches the operand, the
// pending object runs the keyword first and applies the operator to the
// result.  So each line above means what it looks like it means.
//
// NEEDFUL_DEFINE_KEYWORD_PREFIX_FORMS(Helper) turns this on for a keyword:
//
//     x * kw y    x / kw y    x % kw y    - kw y    ~ kw y
//
// Dereference is separate, since it's only sound for keywords whose result
// can't be null (see NEEDFUL_DEFINE_KEYWORD_DEREF_FORM), and `!` is handled
// by each keyword itself.
//
// These only cost anything (extra calls in a debug build) in expressions
// that use them, which were compile errors before.
//
// 1. The keyword's own operator is found at instantiation time through
//    argument-dependent lookup, so it doesn't have to be declared yet here.
//    That call is also what keeps every check the keyword makes: e.g. the
//    assert in `unwrap`, or the error for using it on a Need(T).
//

struct KwMul {};  struct KwDiv {};  struct KwMod {};
struct KwNeg {};  struct KwBitNot {};  struct KwDeref {};

#if defined(_MSC_VER)  // aggregate holding a reference: /setup#c4623
  #pragma warning(push)
  #pragma warning(disable: 4623)  // "default constructor implicitly deleted"
#endif

template<typename L, typename Helper, typename Op>
struct KeywordPendingBinary { const L& left; };  // only lives for the expression

#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

template<typename Helper, typename Op>
struct KeywordPendingUnary {};

template<typename L, typename H, typename W>
auto operator*(KeywordPendingBinary<L,H,KwMul> k, const W& w)
    -> decltype(k.left * (H{} * w))  // [1]
    { return k.left * (H{} * w); }

template<typename L, typename H, typename W>
auto operator*(KeywordPendingBinary<L,H,KwDiv> k, const W& w)
    -> decltype(k.left / (H{} * w))
    { return k.left / (H{} * w); }

template<typename L, typename H, typename W>
auto operator*(KeywordPendingBinary<L,H,KwMod> k, const W& w)
    -> decltype(k.left % (H{} * w))
    { return k.left % (H{} * w); }

template<typename H, typename W>
auto operator*(KeywordPendingUnary<H,KwNeg>, const W& w)
    -> decltype(-(H{} * w))
    { return -(H{} * w); }

template<typename H, typename W>
auto operator*(KeywordPendingUnary<H,KwBitNot>, const W& w)
    -> decltype(~(H{} * w))
    { return ~(H{} * w); }

template<typename H, typename W>
auto operator*(KeywordPendingUnary<H,KwDeref>, const W& w)
    -> decltype(*(H{} * w))  // a reference to the pointed-to thing
    { return *(H{} * w); }

#define NEEDFUL_DEFINE_KEYWORD_PREFIX_FORMS(Helper) \
    template<typename L> \
    KeywordPendingBinary<L,Helper,KwMul> operator*(const L& left, Helper) \
        { return KeywordPendingBinary<L,Helper,KwMul>{left}; } \
    \
    template<typename L> \
    KeywordPendingBinary<L,Helper,KwDiv> operator/(const L& left, Helper) \
        { return KeywordPendingBinary<L,Helper,KwDiv>{left}; } \
    \
    template<typename L> \
    KeywordPendingBinary<L,Helper,KwMod> operator%(const L& left, Helper) \
        { return KeywordPendingBinary<L,Helper,KwMod>{left}; } \
    \
    constexpr KeywordPendingUnary<Helper,KwNeg> operator-(Helper) \
        { return KeywordPendingUnary<Helper,KwNeg>{}; } \
    \
    constexpr KeywordPendingUnary<Helper,KwBitNot> operator~(Helper) \
        { return KeywordPendingUnary<Helper,KwBitNot>{}; } \
    \
    static_assert(true, "")  /* so the callsite takes a semicolon */

#define NEEDFUL_DEFINE_KEYWORD_DEREF_FORM(Helper) \
    constexpr KeywordPendingUnary<Helper,KwDeref> operator*(Helper) \
        { return KeywordPendingUnary<Helper,KwDeref>{}; } \
    \
    static_assert(true, "")  /* so the callsite takes a semicolon */
