//
//  file: %needful-utilities.hpp
//  summary: "Utility macros for C++11 and higher"
//  homepage: https://needful.metaeducation.com/setup
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


//=//// COMPILE-TIME-ONLY DUMMY CLASS INSTANTIATION ///////////////////////=//
//
// This uses sizeof() to force the compiler to instantiate a template, in
// a way that can be used at compile-time.
//
// We take advantage of this to create dummy instances of types whose sole
// job is to do static assertions, without bringing in runtime cost.  This
// macro documents that we're doing that.
//
// Note that if for some reason one needs function call semantics for a check,
// this works with getting the size of that function's return type.
//
#define NEEDFUL_DUMMY_INSTANCE(...) \
    static_cast<void>(sizeof(__VA_ARGS__))


//=//// DUMP TYPE NAME FOR DEBUGGING //////////////////////////////////////=//
//
// Somehow, despite decades of C++ development, there is no standard way to
// print the name of a type at compile time.  This is a workaround that
// uses a static assertion to force the compiler to print the type name in
// the error message.
//

#if !defined(NDEBUG)
    template<typename T>
    struct ProbeTypeHelper {
        static_assert(sizeof(T) == 0, "See sizeof() error for probed type");
    };

    #define PROBE_DECLTYPE(...) \
        NEEDFUL_DUMMY_INSTANCE(needful::ProbeTypeHelper<decltype(__VA_ARGS__)>)

    #define PROBE_CTYPE(...) \
        NEEDFUL_DUMMY_INSTANCE(needful::ProbeTypeHelper<__VA_ARGS__>)
#endif


//=//// MATERIALIZE PRVALUE ///////////////////////////////////////////////=//
//
// A prvalue is a pure temporary with no memory location--you can't take
// its address or reinterpret_cast<> it.  GCC enforces this strictly, so
// tricks that MSVC and Clang tolerate (like binding a cross-type rvalue
// reference && to a prvalue for reinterpretation) are rejected.  :-(
//
// This macro is a workaround, that forces a prvalue to get a memory location
// via same-type const& binding.  C++ standard mandates "temporary
// materialization" for this, giving the prvalue a stack slot whose address we
// can take.  It can be used e.g. to bypass conversion operators by
// pointer-reinterpreting to the standard-layout first member.
//
#define NEEDFUL_MATERIALIZE_PRVALUE(expr) \
    (&needful_c_cast( \
        const needful::remove_reference_t<decltype(expr)>&, \
        (expr)))


//=//// VARIADIC MACRO PARENTHESES REMOVAL ////////////////////////////////=//
//
// NEEDFUL_UNPARENTHESIZE is used to remove a single layer of parentheses
// from a macro argument.  This is useful if you want to capture variadic
// arguments at a macro callsite as a single argument in parentheses.
//
// Needful uses it to transfer variadic arguments to C++ templates in a way
// that C can just ignore:
//
//     #define MY_MACRO(list,expr)  /* [1] */
//         my_template<NEEDFUL_UNPARENTHESIZE list>(expr)
//
// Note the macro isn't invoked with parentheses in the expansion--it uses
// the parentheses in the argument.  So if you say:
//
//    MY_MACRO((int, float, double), value)
//
// It expands to:
//
//    my_template<NEEDFUL_UNPARENTHESIZE (int, float, double)>(expr)
//
// Which further expands to:
//
//    my_template<int, float, double>(expr)
//
// 1. You can't put backslashes in comments, but there'd be one here.
//
#define NEEDFUL_UNPARENTHESIZE(...)  __VA_ARGS__


//=//// TYPE TRAIT ALIAS SHIMS (FOR C++11 COMPATIBILITY) //////////////////=//
//
// Needful is supposed to work in C++11, so we don't use the C++14/17/20
// type trait aliases.  But the language is fully capable of supporting them
// as a feature--they're just weren't in the standard library.
//
// Shimming them into the std:: namespace is fraught.  So instead, we just
// define their equivalents in the needful:: namespace, which means that
// much of the code in Needful can use it without namespacing (unless it's
// a macro intended to be used outside the needful:: namespace, at which
// point it has to carry the namespace.)
//
// 1. CWG 1558: In C++11, unused parameters in alias templates were not
//    guaranteed to cause substitution failure. The make_void struct forces the
//    compiler to evaluate the parameters before resolving to void, ensuring
//    SFINAE works reliably even on older C++11 compilers (like GCC 4.8).
//
// 2. is_convertible_v<From,To> is not something you can define in C++11.
//
//      template<typename From, typename To>  // needs C++14 :-(
//      constexpr bool is_convertible_v = std::is_convertible<F, T>::value;
//
//      template<typename From, typename To>  // needs C++17 :-(
//      using is_convertible_v = std::is_convertible<From, To>::value;
//
//    Defining a macro seems worth it for the readability advantage.
//

template<typename T>  // C++14
using decay_t = typename std::decay<T>::type;

template<typename T>  // C++14
using remove_reference_t = typename std::remove_reference<T>::type;

template<typename T>  // C++14
using remove_const_t = typename std::remove_const<T>::type;

template<typename T>  // C++14
using remove_cv_t = typename std::remove_cv<T>::type;

template<typename T>  // C++14
using remove_pointer_t = typename std::remove_pointer<T>::type;

template<typename T>  // C++14
using add_pointer_t = typename std::add_pointer<T>::type;

template<bool B, typename T = void>  // C++14
using enable_if_t = typename std::enable_if<B, T>::type;

template<typename... Ts> struct make_void { typedef void type; };  // [1]

template<typename... Ts>  // C++17 (polyfill for C++11/14 builds)
using void_t = typename make_void<Ts...>::type;

template<bool B, typename T, typename F>  // C++14
using conditional_t = typename std::conditional<B, T, F>::type;

#define needful_is_convertible_v(From,To) /* macro HACK [2] */ \
    std::is_convertible<From, To>::value

template<typename From, typename To>
class is_explicitly_convertible {  // C++20
    template<typename F, typename T>
    static auto test(int) ->
        decltype(static_cast<T>(std::declval<F>()), std::true_type{});
    template<typename, typename>
    static std::false_type test(...);
  public:
    static constexpr bool value = decltype(test<From, To>(0))::value;
};

#define needful_is_explicitly_convertible_v(From,To) /* macro HACK [1] */ \
    needful::is_explicitly_convertible<From, To>::value


//=//// SAME-LAYOUT INHERITANCE CHECK /////////////////////////////////////=//
//
// Checks if Base is an ancestor of Derived in a zero-cost inheritance
// hierarchy where derivation adds no members.  Both must be standard-layout
// classes with the same size.
//
// This is the foundational invariant that makes the "check C with C++"
// pattern safe: because the memory representations are identical, casts
// between pointer levels (Derived** -> Base**) are safe even though C++
// doesn't allow them implicitly.
//
// IsContravariantLayout (in needful-contra.hpp) builds on this same
// invariant for Sink()/Init() parameter checking.
//

template<typename Base, typename Derived, typename Enable = void>
struct IsSameLayoutBase : std::false_type {};

template<typename Base, typename Derived>
struct IsSameLayoutBase<Base, Derived, enable_if_t<
    std::is_class<Base>::value
    and std::is_class<Derived>::value
    and std::is_base_of<Base, Derived>::value
>> {
    static_assert(
        std::is_standard_layout<Base>::value
        and std::is_standard_layout<Derived>::value
        and sizeof(Base) == sizeof(Derived),
        "Same-layout inheritance requires identical-sized standard layout types"
    );

    static constexpr bool value = true;
};


//=//// DEEP POINTER CONVERTIBILITY TRAIT /////////////////////////////////=//
//
// Standard std::is_convertible<Derived**, Base**> is false because C++
// doesn't allow covariant pointer-to-pointer conversions.  But in the
// needful type system, these conversions are safe: derivation adds no
// members, so the pointer representations are identical at every level.
//
// This trait recursively strips matching pointer layers from both types,
// then delegates to std::is_convertible at the innermost pointer level.
// The recursion only fires when BOTH sides are still pointers after
// stripping one layer, ensuring mismatched pointer depths are rejected.
//
//   IsDeepPointerConvertible<Derived*, Base*>      => true  (leaf check)
//   IsDeepPointerConvertible<Derived**, Base**>    => true  (recurse once)
//   IsDeepPointerConvertible<Derived***, Base***>  => true  (recurse twice)
//   IsDeepPointerConvertible<Derived**, Base*>     => false (depth mismatch)
//
// No separate layout assertion is needed here: std::is_convertible at the
// leaf level already requires a valid inheritance relationship, and the
// layout invariant (standard-layout, same sizeof) is enforced by
// IsSameLayoutBase wherever class types participate in contravariant casts.
//

template<typename From, typename To, typename Enable = void>
struct IsDeepPointerConvertible : std::is_convertible<From, To> {};

template<typename From, typename To>
struct IsDeepPointerConvertible<From*, To*, enable_if_t<
    std::is_pointer<From>::value and std::is_pointer<To>::value
>> : IsDeepPointerConvertible<From, To> {};

#define needful_is_deep_pointer_convertible_v(From,To) \
    needful::IsDeepPointerConvertible<From, To>::value

#define needful_is_constructible_v(From,To) /* macro HACK [1] */ \
    std::is_constructible<From, To>::value


//=//// is_function_pointer TRAIT /////////////////////////////////////////=//
//
// The C++ standard defines std::is_function<> but has no equivalent test
// for function pointers.  Define a helper in the needful namespace.
//

template<typename T>
struct is_function_pointer : std::false_type {};

template<typename Ret, typename... Args>
struct is_function_pointer<Ret (*)(Args...)> : std::true_type {};


//=//// AlwaysFalse TRAIT /////////////////////////////////////////////////=//
//
// AlwaysFalse<T> is a template that always yields false, but is dependent
// on T.  This works around the problem of static_assert()s inside of SFINAE'd
// functions, which would fail even if the SFINAE conditions were not met:
//
//    static_assert(false, "Always fails, even if not SFINAE'd");
//    static_assert(AlwaysFalse<T>::value, "Only fails if SFINAE'd");
//

template<typename T>  // T is ignored, just here to make it a template
struct AlwaysFalse : std::false_type {};  // for SFINAE static_assert [2]


//=//// VOID WAYPOINT: SOLICIT IMPLICIT CONVERSION, THEN REINTERPRET /////=//
//
// For the general, non-Needful-specific writeup of this idiom -- including
// why it's a *different* fix than the "implicit_cast<T>() via non-deduced
// context" trick, and when each one applies -- see:
//
//   https://ae1020.github.io/implicit-cast-vs-waypoint-cast/
//
// (That article calls this the "void waypoint", hence the macro's name --
// the `_cast` suffix flags it as a cast, alongside c_cast/m_cast/etc.)
//
// ContraWrapper/SinkWrapper constructors are templates that accept any U
// certified compatible by IsContravariant -- U is a BASE-typed pointer or
// wrapper (Sink/Init deliberately accept less-constrained BASE classes as
// input; see IsContravariant/IsSameLayoutBase in needful-contra.hpp).  We
// need the more-constrained T* (e.g. Derived*) out of it:
//
//    this->p = needful_void_waypoint_cast(T*, u);
//
// This is a downcast (Base* -> Derived*), which is *never* implicit in
// C++ -- there's no standard or user-defined conversion the compiler will
// pick on its own, whether U is a raw pointer or a wrapper class.  That
// rules out fixing this with the non-deduced-context `implicit_cast<T>()`
// trick used elsewhere for the opposite (upcast) problem: forcing
// copy-initialization doesn't help when there's no implicit conversion to
// force in the first place -- it just fails to compile.
//
// So a direct static_cast<T*>(u) is used instead... except when U is a
// wrapper whose implicit `operator U*()` has a side effect that must run
// (e.g. SinkWrapper's corruption-poisoning, see [1]).  A direct cast
// performs direct-initialization, which happily calls a *templated explicit*
// `operator X*()` if the wrapper has one -- skipping the implicit operator,
// and its side effect, entirely.  needful_void_waypoint_cast(T,expr) is:
//
//    (T*)(void*)u
//
// Step 1, (void*)u, converts u to void*.  For a raw pointer U this is a
// trivial pointer-to-void conversion, with nothing to go wrong.
//
// For a wrapper U, it's tempting to think (void*)u *solicits* whatever
// implicit `operator X*()` the wrapper has, because "implicit beats
// explicit."  That's not actually how the standard ranks conversion-function
// candidates for direct-initialization -- explicitness only decides whether
// a candidate is in the set at all, not how candidates are ranked once
// they're in it.  If the wrapper *also* has a templated `explicit operator
// U*()` (as ContraWrapper/SinkWrapper do, for generic coercion), deducing
// U=void gives that operator an exact-match (Identity) return type of
// void*, which beats the implicit operator's return type needing an extra
// standard conversion to reach void* (ranked Conversion).  So (void*)u can
// silently call the *explicit* operator instead of the implicit one --
// see the linked article's "Case 2" for the full mechanics, confirmed by
// compiling a minimal reproduction.
//
// This macro does NOT protect against that on its own.  It only guarantees
// safety for U types it's been vouched safe for, via NeedfulVoidWaypointSafe
// below -- see that trait for what "safe" means and how ContraWrapper /
// SinkWrapper / InitWrapper satisfy it today.
//
// Step 2, (T*)(void* value), is a bare *reinterpretation* of the address --
// not a conversion the type system endorses.  This is safe here -- in both
// directions, regardless of how many bases are in the hierarchy --
// specifically *because* IsSameLayoutBase requires both classes to be
// `std::is_standard_layout` with identical `sizeof`.  C++'s standard-layout
// rules only allow ONE class anywhere in the hierarchy to have non-static
// data members; combined with equal sizeof (no fields added in derivation),
// that pins the data-holding base at offset 0 by construction.  There is no
// valid Needful-layout-compatible hierarchy where this reinterpret would
// need an offset adjustment it doesn't get -- but that's a fact this macro
// leans on, not one it checks itself.
//
// Because c_cast() may not be defined in all contexts where this idiom is
// needed, this is defined as its own macro.  If it turns out to be useful
// for those writing needful-powered enhancements, it may become public
// facing.
//
// 1. See SinkWrapper's `corruption_pending` handling in needful-contra.hpp.
//

//=//// VOID WAYPOINT SAFETY WHITELIST /////////////////////////////////=//
//
// Whether step 1 above lands on the implicit or the explicit-template
// operator is not something the compiler's overload resolution rules
// let us detect generically from T alone -- and whether that even matters
// depends on what the two operators *do*, which is a semantic fact about
// U's authors, not something visible in its type.  So rather than try to
// prove step 1 safe for any possible U, class types have to opt in here,
// vouching that it doesn't matter which operator (void*)u actually calls
// -- either because neither has a side effect, or because both run the
// same one.
//
// Raw pointers need no entry: they have no conversion operators to race in
// the first place, so they're safe unconditionally.
//
// ContraWrapper/SinkWrapper/InitWrapper (needful-contra.hpp) specialize
// this to true_type, each with a comment justifying why their specific
// pair of conversion operators is safe to conflate this way.
//
template<typename T>
struct NeedfulVoidWaypointSafe : std::is_pointer<T> {};

template<typename T>
struct NeedfulVoidWaypointSafeChecker {
    static_assert(
        NeedfulVoidWaypointSafe<T>::value,
        "needful_void_waypoint_cast(): T not vouched safe (see"
        " NeedfulVoidWaypointSafe in needful-utilities.hpp)"
    );
};

#define needful_void_waypoint_cast(T,expr) \
    (NEEDFUL_DUMMY_INSTANCE(needful::NeedfulVoidWaypointSafeChecker< \
        needful::remove_cv_t< \
            needful::remove_reference_t<decltype(expr)>>>), \
    (T)(void*)(expr))
