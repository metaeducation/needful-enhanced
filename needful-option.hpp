//
//  file: %needful-option.hpp
//  summary: "Optional Wrapper Trick for C's Boolean Coercible Types"
//  homepage: <needful homepage TBD>
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2026 metaeducation.com
//
// Licensed under the MIT License
//
// https://en.wikipedia.org/wiki/MIT_License
//
//=////////////////////////////////////////////////////////////////////////=//
//
// See %needful.h for an overview of Option(T).
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// A. By Needful convention, unadorned pointers like `Foo*` are typically
//    assumed to be non-null. Option(T) is used to make nullability explicit
//    when states are legitimately optional. This documents intent and aids
//    readability, rather than heavily littering the codebase with Need(T).
//
//    Option() may also be safely unwrapped into raw variables when logic
//    immediately branches on the truthiness of the variable:
//
//        Foo* foo = opt Some_Optional_Foo(...);
//        if (! foo)
//           return Some_Missing_Foo_Error(...);
//
//        Use_Foo(foo);  /* safe because of immediate early return */
//


//=//// none: DISENGAGED SENTINEL /////////////////////////////////////////=//
//
// `none` constructs an Option(T) in the disengaged state.
//
// If you use this with an Option(T*), then nullptr is equivalent to `none`.
//
//     Option(char*) foo = none;         /* OK */
//     Option(char*) bar = nullptr;      /* also OK */
//     Option(char*) baz = 0;            /* compile-time error */
//
// If you use it with an enum, be sure the enum was declared with a 0 value
// that is not otherwise valid for the enum:
//
//     Option(SomeEnum) foo = none;       /* OK */
//     Option(SomeEnum) bar = nullptr;    /* compile-time error */
//     Option(SomeEnum) baz = 0;          /* compile-time error */
//

#undef NeedfulNone
#define NeedfulNone  needful::NoneStruct

#undef needful_none
#define needful_none  needful::NoneStruct{}  // instantiate {} none instance


//=//// OPTION WRAPPER ////////////////////////////////////////////////////=//
//
// 1. `T` must be explicitly bool-coercible. Things like `Option(Need(T))`
//    are deliberately invalid to enforce clear boundaries. `static_assert`
//    provides clear errors for these cases.
//
// 2. Unlike std::optional, Needful's Option(T) is identical in size to T.
//    It leverages a natural empty/falsey "sentinel" state instead of a
//    separate tracking boolean, making it fully C ABI compatible and zero-cost.
//
// 3. To allow transparent 0-initialization in globals and C structures, the
//    default constructor is retained. Uninitialized locals remain so.
//
// 4. We want to avoid situations where Option(T) is implicitly assigned the
//    results of Need(T) functions, creating a misleading situation where
//    that result appears testable. So prvalue Need(T) construction is blocked.
//
// 5. Explicit c_cast() and standard conversions are allowed because they
//    indicate deliberate extraction.
//

#undef NeedfulOption
#define NeedfulOption(T)  needful::OptionWrapper<T>

template<typename>
struct IsOptionWrapper : std::false_type {};

template<typename T>
struct OptionWrapper {
    static_assert(
        needful_is_explicitly_convertible_v(T, bool),
        "T used with Option(T) must be explicitly convertible to bool"  // [1]
    );

    NEEDFUL_DECLARE_WRAPPED_FIELD (T, o);

    /* bool engaged; */  // unlike with std::optional, not needed! [2]

    OptionWrapper () = default;  // garbage, or 0 if global [3]

    OptionWrapper(NoneStruct)
        : o {}
      {}

    template <
        typename U,
        typename = enable_if_t<needful_is_convertible_v(U, T)>
    >
    OptionWrapper (U&& something)
        : o (something)  // not {something}, so narrowing conversions ok
      {}

    template <
        typename U,
        typename = enable_if_t<not needful_is_convertible_v(U, T)>
    >
    explicit OptionWrapper(const U& something)
        : o {needful_c_cast(T, something)}
      {}

    template <typename X>
    OptionWrapper (const OptionWrapper<X>& other)
        : o {other.o}  // necessary...won't use the (U something) template
      {}

    template<typename U>  // block *prvalue* Need(T) specifically [4]
    OptionWrapper(NeedWrapper<U>&&) = delete;

    template<typename U>
    explicit operator U() const {
        return needful_c_cast(U, o);  // cast() blocks removal [5]
    }

    explicit operator bool() const {  // explicit exception in `if`
        return o ? true : false;  // https://stackoverflow.com/q/39995573/
    }
};

  //=//// LABORIOUS REPEATED OPERATORS ////////////////////////////////////=//

  // While the combinatorics may seem excessive with repeating the equality
  // and inequality operators, this is the way std::optional does it too.

template<typename L, typename R>
bool operator==(const OptionWrapper<L>& left, const OptionWrapper<R>& right)
  { return left.o == right.o; }

template<typename L, typename R>
bool operator==(const OptionWrapper<L>& left, R right)
  { return left.o == right; }

template<typename L, typename R>
bool operator==(L left, const OptionWrapper<R>& right)
  { return left == right.o; }

template<typename L, typename R>
bool operator!=(const OptionWrapper<L>& left, const OptionWrapper<R>& right)
  { return left.o != right.o; }

template<typename L, typename R>
bool operator!=(const OptionWrapper<L>& left, R right)
  { return left.o != right; }

template<typename L, typename R>
bool operator!=(L left, const OptionWrapper<R>& right)
  { return left != right.o; }

  //=//// CORRUPTION HELPER ///////////////////////////////////////////////=//

  // See %needful-corruption.h for motivation and explanation.

#if NEEDFUL_USES_CORRUPT_HELPER
    template<typename T>
    struct CorruptHelper<OptionWrapper<T>> {
      static void corrupt(OptionWrapper<T>& option) {
        Corrupt_If_Needful(option.o);
      }
    };
#endif

template<typename X>
struct IsOptionWrapper<OptionWrapper<X>> : std::true_type {};

template<typename X>  // Option carries engaged/disengaged semantics [8]
struct IsWrapperSemantic<OptionWrapper<X>> : std::true_type {};


//=/// UNWRAP HOOK FOR Optional(T) ////////////////////////////////////////=//
//
// Use `unwrap` when you're sure that an optional contains a value (typically
// known by doing a conditional check):
//
//    Option(Foo*) foo = ...;
//    if (foo)
//        Some_Function(unwrap foo)
//
//    /* we have `#define unwrap needful::g_unwrap_helper +` so we get... */
//
//    Option(Foo*) foo = ...;
//    if (foo)
//        Some_Function(g_unwrap_helper + foo)
//
// 1. See the definition of UnwrapHelper for mechanics of how this "keyword"
//    is accomplished (and why the `+` operator was chosen specifically).
//

template<typename T>
T operator+(  // lower precedence than % [1]
    UnwrapHelper,
    const OptionWrapper<T>& option
){
  NEEDFUL_ASSERT(option.o);  // non-null or non-zero
    return option.o;
}


//=/// OPT HELPER CLASS ///////////////////////////////////////////////////=//
//
// The operator for giving you back the raw (possibly null or 0) value from a
// wrapped Option(T) is called `opt`.
//
// 1. You can think of `opt` as unwrapping an optional to access its raw
//    potential empty state safely. See also: [A] at top of file.
//
// 2. See the definition of UnwrapHelper for mechanics of how this "keyword"
//    is accomplished (and why the `+` operator was chosen specifically).

struct OptHelper {};
constexpr OptHelper g_opt_helper = {};

#undef needful_opt
#define needful_opt \
    needful::g_opt_helper +  // lower precedence than % [2]


template<typename T>
T operator+(  // lower precedence than % [2]
    OptHelper,
    const OptionWrapper<T>& option
){
    return option.o;
}


//=/// BLOCK `needed` ON OptionWrapper ////////////////////////////////////=//
//
// The `needed` operator is only valid on Need(T), not on Option(T).  If you
// try to use `needed` on an Option(T), it's a compile-time error.  This makes
// `needed` a useful building block for macros that reject optional types.
//
template<typename T>
T operator+(
    NeededHelper,
    const OptionWrapper<T>&
){
    static_assert(
        sizeof(T) != sizeof(T),  // dependent false
        "cannot use `needed` on an Option(T) -- use `opt` or `unwrap` instead"
    );
    return *static_cast<T*>(nullptr);  // unreachable
}


//=/// BLOCK OptionWrapper() CONTRAVARIANCE ///////////////////////////////=//
//
// While OptionWrapper() is a "wrapped type", you don't want to be able to
// pass an Option(T*) to a Sink(T).  This is because Option(T*) might be
// disengaged (nullptr)... there may be no storage to write through to.  So
// it can't behave like its "wrapped type" in that situation.
//
// We specialize IsContravariant directly to always return false.
//
template<typename T, typename Target>
struct IsContravariant<OptionWrapper<T>, Target, true> : std::false_type {};
