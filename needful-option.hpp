//
//  file: %needful-option.hpp
//  summary: "Optional Wrapper Trick for C's Boolean Coercible Types"
//  homepage: https://needful.metaeducation.com/option
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
// A. By convention, bare pointers like Foo* are treated as non-null.
//    Option(T) marks intentional nullability.  Unwrapping to a raw variable
//    is acceptable when control flow immediately proves non-nullness:
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
// 4. prvalue Need(T) construction is blocked to prevent accidental
//    conversion into a testable optional value.
//
// 5. Explicit c_cast() and standard conversions are allowed because they
//    indicate deliberate extraction.
//
// 6. A Fallible(T) may not silently become an Option(T): that would turn a
//    failure into an ordinary absence without anyone saying so.  It takes
//    the `fallible` operator.  FallibleWrapper derives from OptionWrapper, so
//    without these the derived-to-base conversion would allow it.  An exact
//    match on FallibleWrapper<X> beats that, so these deleted overloads win.
//    (Binding a `const Option(T)&` to a Fallible(T) is not blocked, as no
//    constructor is involved--but that's not a C-compatible idiom anyway.)
//

#undef NeedfulOption
#define NeedfulOption(T)  needful::OptionWrapper<T>

template<typename>
struct IsOptionWrapper : std::false_type {};

template<typename T>
struct NEEDFUL_NODISCARD FallibleWrapper;  // defined below, see [6]

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

    template<typename X>  // use `fallible` operator [6]
    OptionWrapper(const FallibleWrapper<X>&) = delete;

    template<typename X>  // use `fallible` operator [6]
    OptionWrapper& operator=(const FallibleWrapper<X>&) = delete;

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

  //=//// COMPARISON AGAINST `none` ///////////////////////////////////////=//

  // `none` needs to be readable as well as writable.  Without these it is a
  // write-only token -- you could say `return none` but not `if (x == none)`
  // -- which leaves the vocabulary half-finished.  The none-reactive macros
  // in %needful.h test with this.
  //
  // 1. These must be spelled out for NoneStruct specifically, not left to the
  //    generic `operator==(const OptionWrapper<L>&, R)` above.  That one
  //    happily deduces R = NoneStruct and then fails inside its own body on
  //    `left.o == NoneStruct`.  Being more specialized, these beat it in
  //    partial ordering.
  //
  // 2. The unwrapped overload covers the raw T left behind once the `%`
  //    extractor has run, as well as ordinary pointers and enums.  It is
  //    constrained to bool-convertible types so that it cannot match a
  //    Result(T) (never "none"), and so that a Need(T) -- whose bool coercion
  //    is deliberately blocked -- does not quietly compare equal to none.
  //
  //    A FallibleWrapper<T> binds here rather than to [1], since identity
  //    beats a derived-to-base conversion.  Both spell the same answer, so
  //    the resolution is unambiguous either way.

template<typename L>  // more specialized than (OptionWrapper<L>&, R) [1]
bool operator==(const OptionWrapper<L>& left, NoneStruct)
  { return not left.o; }

template<typename R>
bool operator==(NoneStruct, const OptionWrapper<R>& right)
  { return not right.o; }

template<typename L>
bool operator!=(const OptionWrapper<L>& left, NoneStruct)
  { return not not left.o; }

template<typename R>
bool operator!=(NoneStruct, const OptionWrapper<R>& right)
  { return not not right.o; }

template<typename T, typename =  // raw T, e.g. after the extractor [2]
    enable_if_t<needful_is_explicitly_convertible_v(T, bool)>>
bool operator==(const T& value, NoneStruct)
  { return not value; }

template<typename T, typename =
    enable_if_t<needful_is_explicitly_convertible_v(T, bool)>>
bool operator==(NoneStruct, const T& value)
  { return not value; }

template<typename T, typename =
    enable_if_t<needful_is_explicitly_convertible_v(T, bool)>>
bool operator!=(const T& value, NoneStruct)
  { return not not value; }

template<typename T, typename =
    enable_if_t<needful_is_explicitly_convertible_v(T, bool)>>
bool operator!=(NoneStruct, const T& value)
  { return not not value; }

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


//=//// FALLIBLE WRAPPER //////////////////////////////////////////////////=//
//
// Unfortunately, we can't simply do:
//
//    #define NeedfulFallible(T)  NEEDFUL_NODISCARD NeedfulOption(T)
//
// That's because the [[nodiscard]] attribute can only be in particular places
// in function declarations, and it can't be on local variables.  So we have
// to have a separate FallibleWrapper class that puts the [[nodiscard]] in
// the right place.
//

// 1. Option(T) => Fallible(T) is implicit: saying absence is a failure only
//    adds an obligation.  This can't come from inheriting OptionWrapper's
//    `OptionWrapper(const OptionWrapper<X>&)`: an inherited constructor whose
//    parameter is a reference to the base is excluded when constructing the
//    derived class ([over.match.funcs]).  MSVC without /permissive- allows it
//    anyway, but GCC and Clang don't--so it has to be declared here.
//
// 2. OptionWrapper deletes construction from FallibleWrapper<X> (so that a
//    Fallible(T) can't silently become an Option(T)), and that deleted
//    constructor would be inherited too.  Declaring one with the same
//    signature here hides it, so Fallible(T) => Fallible(U) still works.
//    (It's also an exact match, so it beats [1] for a FallibleWrapper<X>.)

template <typename T>
struct NEEDFUL_NODISCARD FallibleWrapper : public needful::OptionWrapper<T> {
    using needful::OptionWrapper<T>::OptionWrapper;  // inherit constructors

    template <typename X>  // Option(T) => Fallible(T) [1]
    FallibleWrapper (const OptionWrapper<X>& other)
        : needful::OptionWrapper<T> (other.o)
      {}

    template <typename X>  // hide inherited deleted constructor [2]
    FallibleWrapper (const FallibleWrapper<X>& other)
        : needful::OptionWrapper<T> (other.o)
      {}
};

#undef NeedfulFallible
#define NeedfulFallible(T)  needful::FallibleWrapper<T>

// In C the two spellings differ: Fallible(T) carries a must-use annotation
// that is only legal on a function's return type, and FallibleVar(T) omits it
// so locals, parameters and members have something legal to say.
//
// Here there is nothing to omit.  [[nodiscard]] rides on the wrapper class
// rather than on the declaration, so no declaration position is special and
// both spellings are the same type.  Keeping FallibleVar(T) defined (rather
// than leaving it to the C fallback) is what lets one source file compile in
// all three modes.

#undef NeedfulFallibleVar
#define NeedfulFallibleVar(T)  needful::FallibleWrapper<T>

  //=//// FallibleWrapper MUST REPEAT OptionWrapper'S TRAITS //////////////=//
  //
  // Deriving inherits constructors.  It does NOT inherit trait
  // specializations: `IsWrapperSemantic<OptionWrapper<X>>` is a specialization
  // for that exact template, and FallibleWrapper<X> is a different type that
  // merely has it as a base.  Every trait OptionWrapper claims has to be
  // claimed again here, and forgetting one fails silently in the permissive
  // direction -- the generic template answers `false`, and the property just
  // quietly does not hold for Fallible(T).
  //
  // Two of these were missing, with consequences worth naming:
  //
  // 1. Without the IsContravariant specialization, Fallible(T*) could be
  //    passed where a Sink(T) was expected.  Option(T*) is refused there on
  //    purpose -- it may be disengaged, so there may be no storage to write
  //    through -- and Fallible(T) is an Option(T) that is *more* insistent
  //    about being checked, so it must be refused at least as firmly.  It was
  //    not: SinkWrapper's converting constructor was selected, and the build
  //    only broke later, deep inside an unrelated static_assert about void
  //    waypoint safety.  Accidental rejection with a confusing message, one
  //    trait change away from being accepted outright.
  //
  // 2. Without IsWrapperSemantic, cast() treats Fallible(T) as a plain type
  //    and unwraps it rather than re-wrapping the result, so casting a
  //    Fallible(T) quietly discards the [[nodiscard]] that is its whole point.

template<typename X>
struct IsOptionWrapper<FallibleWrapper<X>> : std::true_type {};

template<typename X>  // else cast() strips the wrapper, and the nodiscard [2]
struct IsWrapperSemantic<FallibleWrapper<X>> : std::true_type {};

#if NEEDFUL_CONTRAS_USE_WRAPPER  // no Sinks to block if contras are off
    template<typename T, typename Target>  // may be disengaged: no storage [1]
    struct IsContravariant<FallibleWrapper<T>, Target, true>
        : std::false_type {};
#endif

#if NEEDFUL_USES_CORRUPT_HELPER
    template<typename T>
    struct CorruptHelper<FallibleWrapper<T>> {
      static void corrupt(FallibleWrapper<T>& f) {
        Corrupt_If_Needful(f.o);
      }
    };
#endif

// `infallible` is the expression-position discharge for a Fallible(T): it
// asserts the value is engaged and hands back the raw T.
//
// It is the same operation as `unwrap`, which also binds here (deduction to a
// base class of the argument is allowed, and FallibleWrapper derives from
// OptionWrapper).  The separate spelling exists because it names the claim
// being made at the call site -- `p = infallible Try_Alloc(n)` asserts this
// particular call cannot fail, which is more pointed than "unwrap it".
//
// There used to also be `unwrap_fallible`, a third spelling of the same
// thing.  It was never used in a doc, test, or example, and is gone.

#undef needful_infallible
#define needful_infallible  needful_unwrap


// `fallible` is the explicit Fallible(T) => Option(T) conversion.  It names
// the claim being made at the call site: this can fail, and a failure is to
// be treated as an ordinary absence (`none`) from here on.
//
//    Option(Foo*) foo = fallible Try_Find_Foo(id);
//
// It is rejected on a plain Option(T), so it can't turn into a habit that is
// harmless everywhere and means nothing anywhere.  (An exact match on the
// OptionWrapper<T> overload loses to FallibleWrapper<T>'s for a Fallible(T)
// argument, as identity beats a derived-to-base conversion.)

struct FallibleHelper {};
constexpr FallibleHelper g_fallible_helper = {};

#undef needful_fallible
#define needful_fallible \
    needful::g_fallible_helper *  // lower precedence than ->*

template<typename T>
OptionWrapper<T> operator*(
    FallibleHelper,
    const FallibleWrapper<T>& f
){
    return OptionWrapper<T> {f.o};
}

template<typename T>
OptionWrapper<T> operator*(
    FallibleHelper,
    const OptionWrapper<T>&
){
    static_assert(
        sizeof(T) != sizeof(T),  // dependent false
        "`fallible` is only for Fallible(T) => Option(T), not on an Option(T)"
    );
    return NoneStruct{};  // unreachable
}


//=//// UNWRAP HOOK FOR Optional(T) ///////////////////////////////////////=//
//
// Use `unwrap` when you're sure that an optional contains a value (typically
// known by doing a conditional check):
//
//    Option(Foo*) foo = ...;
//    if (foo)
//        Some_Function(unwrap foo)
//
//    /* we have `#define unwrap needful::g_unwrap_helper *` so we get... */
//
//    Option(Foo*) foo = ...;
//    if (foo)
//        Some_Function(g_unwrap_helper * foo)
//
// 1. Why `*` (and `->*` for Result(T) extraction), and which expressions need
//    parentheses as a result:  https://needful.metaeducation.com/precedence
//
// 2. `unwrap` is Option(T)'s vocabulary alone--Need(T) uses `needed`.  So it
//    is defined here, and with NEEDFUL_OPTION_USES_WRAPPER off it stays the
//    C build's no-op.  (See the operator table in %needful-need.hpp for why
//    sharing it with Need(T) was a bad idea.)
//

struct UnwrapHelper {};
constexpr UnwrapHelper g_unwrap_helper = {};

#undef needful_unwrap
#define needful_unwrap /* Option(T) only [2] */ \
    needful::g_unwrap_helper *  // lower precedence than ->* [1]

template<typename T>
T operator*(  // lower precedence than ->* [1]
    UnwrapHelper,
    const OptionWrapper<T>& option
){
  NEEDFUL_ASSERT(option.o);  // non-null or non-zero
    return option.o;
}

NEEDFUL_DEFINE_KEYWORD_PREFIX_FORMS(UnwrapHelper);  // `2 * unwrap x`, etc.
NEEDFUL_DEFINE_KEYWORD_DEREF_FORM(UnwrapHelper);  // `* unwrap p`

// `! unwrap x` parses as `(! helper) * x`, since prefix `!` binds tighter
// than the keyword's `*`.  That would just be a "no operator!" error.  But
// there's something better to say: an unwrapped value is never zero or null,
// so the test could only ever be false.
//
// (Returns the helper, so the rest of the expression compiles and the
// static_assert is the only error shown.)
//
template<typename Dummy = int>
UnwrapHelper operator!(UnwrapHelper) {
    static_assert(
        sizeof(Dummy) != sizeof(Dummy),  // dependent false
        "`! unwrap x` is always false: unwrap gives a non-null/non-zero value."
        "  Use `! x` or `! opt x` to test the Option(T) itself."
    );
    return UnwrapHelper{};
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
//    is accomplished (and why the `*` operator was chosen specifically).
//
// 3. `! opt x` parses as `(! helper) * x`, as prefix `!` binds tighter than
//    the keyword's `*`.  Unlike with `unwrap`, the test is meaningful.  So
//    `!` on the helper gives a second helper, whose `*` does the negation:
//
//        ! opt x   =>   (! g_opt_helper) * x   =>   NotOptHelper{} * x
//
//    The negation then happens at the keyword's precedence, not at prefix
//    precedence.  That's what you'd want: `! opt x == y` is `(! opt x) == y`.
//

struct OptHelper {};
constexpr OptHelper g_opt_helper = {};

#undef needful_opt
#define needful_opt \
    needful::g_opt_helper *  // lower precedence than ->* [2]


template<typename T>
T operator*(  // lower precedence than ->* [2]
    OptHelper,
    const OptionWrapper<T>& option
){
    return option.o;
}

NEEDFUL_DEFINE_KEYWORD_PREFIX_FORMS(OptHelper);  // `2 * opt x`, etc.

// `* opt p` is refused: `opt` is how you say "this may be null", and then
// dereferencing it in the same breath is the bug Option(T) exists to stop.
// (Returns what a legal dereference would, so that the rest of the expression
// compiles and the static_assert is the only error shown.)
//
template<typename Dummy = int>
KeywordPendingUnary<OptHelper,KwDeref> operator*(OptHelper) {
    static_assert(
        sizeof(Dummy) != sizeof(Dummy),  // dependent false
        "`* opt p` would dereference a pointer that may be null."
        "  Check it, then use `* unwrap p`."
    );
    return KeywordPendingUnary<OptHelper,KwDeref>{};
}

struct NotOptHelper {};  // makes `! opt x` work [3]

constexpr NotOptHelper operator!(OptHelper) { return NotOptHelper{}; }

template<typename T>
bool operator*(
    NotOptHelper,
    const OptionWrapper<T>& option
){
    return not option.o;
}


//=/// POSTFIX OPTION EXTRACTOR ///////////////////////////////////////////=//
//
// This is used by the lightweight Fallible(T) wrapper over Option(T) that
// accomplishes some of what Result(T) can do if all you're interested in
// is null results.
//

struct OptionExtractor {};

template<typename T>
inline T operator->*(  // ->* high postfix precedence desired [1]
    const OptionWrapper<T>& option,
    OptionExtractor
){
    return option.o;
}

template<typename T>
inline T* operator->*(  // ->* high postfix precedence desired [1]
    T* pointer,
    OptionExtractor
){
    return pointer;
}
static constexpr OptionExtractor g_option_extractor{};

#undef needful_postfix_extract_option
#define needful_postfix_extract_option \
    /* ; <-- ERROR? DON'T PUT SEMICOLON! [2] */ ->* needful::g_option_extractor


//=/// BLOCK `needed` ON OptionWrapper ////////////////////////////////////=//
//
// The `needed` operator is only valid on Need(T), not on Option(T).  If you
// try to use `needed` on an Option(T), it's a compile-time error.  This makes
// `needed` a useful building block for macros that reject optional types.
//
// Guarded for the same reason as the IsContravariant specialization above:
// this overload exists only to produce a good error, and `needed` is Need's
// own vocabulary.  With NEEDFUL_NEED_USES_WRAPPER off, `needful_needed` is a
// no-op macro and there is no NeededHelper type to overload on, so naming it
// here made Option(T) fail to compile over a keyword that was not in play.
//
// The mirror-image block (`unwrap` on a Need(T)) is also here, as this file
// is included after %needful-need.hpp and it's the one defining UnwrapHelper.
// Without it you'd still get an error, but a confusing "no operator*" one.
//
#if NEEDFUL_NEED_USES_WRAPPER
    template<typename T>
    T operator*(
        NeededHelper,
        const OptionWrapper<T>&
    ){
        static_assert(
            sizeof(T) != sizeof(T),  // dependent false
            "cannot use `needed` on an Option(T) -- use `opt` or `unwrap`"
        );
        return *static_cast<T*>(nullptr);  // unreachable
    }

    template<typename T>
    T operator*(
        UnwrapHelper,
        const NeedWrapper<T>&
    ){
        static_assert(
            sizeof(T) != sizeof(T),  // dependent false
            "cannot use `unwrap` on a Need(T) -- use `needed` (or just use it)"
        );
        return *static_cast<T*>(nullptr);  // unreachable
    }
#endif


//=/// BLOCK OptionWrapper() CONTRAVARIANCE ///////////////////////////////=//
//
// While OptionWrapper() is a "wrapped type", you don't want to be able to
// pass an Option(T*) to a Sink(T).  This is because Option(T*) might be
// disengaged (nullptr)... there may be no storage to write through to.  So
// it can't behave like its "wrapped type" in that situation.
//
// We specialize IsContravariant directly to always return false.
//
// Guarded on the contra wrapper being enabled: this trait exists only to
// refuse a conversion that cannot be attempted at all when there are no
// Sink/Init/Contra wrappers to convert to.  Unguarded, it made Option(T)
// require NEEDFUL_CONTRAS_USE_WRAPPER -- not because Option needs anything
// from contras, but because a specialization cannot name a template that
// was never declared.
//
#if NEEDFUL_CONTRAS_USE_WRAPPER
    template<typename T, typename Target>
    struct IsContravariant<OptionWrapper<T>, Target, true>
        : std::false_type {};
#endif
