//
//  file: %needful-result.hpp
//  summary: "Simulate Rust's Result<T,E> And `?` w/o Exceptions or longjmp()"
//  homepage: https://needful.metaeducation.com/result
//
//=/////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2015-2026 metaeducation.com
//
// Licensed under the MIT License
//
// https://en.wikipedia.org/wiki/MIT_License/
//
//=//// THE Result(T) VOCABULARY //////////////////////////////////////////=//
//
// Seven macros make up the vocabulary.  Needful spells each one out in full
// -- `needful_return_if_failed`, and `return_if_failed` if you opt into the
// NEEDFUL_RESULT_SHORTHANDS group -- and never claims a bare keyword like
// `trap` or `fail` for itself.  Projects that want keyword-style names define
// them locally; %docs/result.md suggests a set.
//
// (An earlier draft of this file documented such a set -- fail, trap, require,
// assume, except, rescue -- as though it were the library's own, alongside a
// `g_divergent` flag separating recoverable failures from abrupt ones.  None
// of that shipped.  There is one error state and one kind of failure.)
//
// The whole mechanism rests on hooks the client supplies: Needful_Get_Failure,
// Needful_Set_Failure, Needful_Test_And_Clear_Failure, Needful_Panic_Abruptly
// and Needful_Assert_Not_Failing.  Define NEEDFUL_DECLARE_RESULT_HOOKS in
// exactly one translation unit for a stock single-threaded implementation, or
// write your own over thread-local storage.
//
//=//// make_failure(...) /////////////////////////////////////////////////=//
//
// Sets the global error state and evaluates to NEEDFUL_RESULT_0, which is
// constructible as the T of any Result(T).  It is not a statement: you return
// it, and Result0Struct is [[nodiscard]] so that forgetting the `return` is
// caught rather than silently setting an error and falling through.
//
//     if (bad_condition)
//         return make_failure (Error_Bad_Thing());
//
//=//// panic(...) ////////////////////////////////////////////////////////=//
//
// For conditions no caller should be asked to handle.  Reports the error and
// ends the process; it does not propagate and cannot be caught.  Unlike
// make_failure() this IS a statement, and is not used with `return`.
//
//     if (catastrophic_condition)
//         panic (Error_Catastrophe());
//
//=//// return_if_failed(stmt) ////////////////////////////////////////////=//
//
// Runs `stmt`, which should call something returning a Result(T).  If no
// failure was signaled, the result is extracted and execution continues.  If
// one was, the current function returns NEEDFUL_RESULT_0, propagating it up
// the call stack.  This is the analogue of Rust's `?` operator, and it is why
// the enclosing function must itself return a Result(T).
//
//     Result(int) foo() {
//         return_if_failed (int x = bar());
//         // ... code continues, with x, only if no failure ...
//     }
//
//=//// panic_if_failed(stmt) /////////////////////////////////////////////=//
//
// Like return_if_failed(), but turns a failure into a panic instead of
// propagating it: the error is handed to Needful_Panic_Abruptly() to be
// reported, and the process ends.  For call sites where a failure means the
// program's assumptions are already broken.
//
//     panic_if_failed (bar());
//     // ... code continues only if no failure ...
//
// (It is deliberately not called abort_if_failed(), which would suggest it
// shares an exit path with the none-reactive family's abort_if_none().  It
// does not -- a failure carries an error object worth reporting first.)
//
//=//// assert_not_failed(stmt) ///////////////////////////////////////////=//
//
// For when you have inside knowledge that this particular call cannot fail.
// Runs `stmt` and asserts no failure was signaled.  The assertion compiles
// away in release builds; the statement itself always runs.
//
//     assert_not_failed (bar());
//     // ... code always continues ...
//
//=//// ...expr... catch_if_failed (decl) {...} ///////////////////////////=//
//
// Attaches to a call that may have signaled a failure, and handles it.  This
// leans on a standard C feature: a `for` loop can scope declarations, so
// `decl` is assigned the error and the state cleared before the body runs
// exactly once.  Because it expands to a `for`, an `else` clause attaches to
// it naturally, giving a success branch.
//
//     Result(int) foo() {
//         bar() catch_if_failed (Error* err) {
//             // handle error in err
//         }
//         // ... code continues if no failure ...
//     }
//
//     Result(int) foo() {
//         Option(Error*) err;
//         bar() catch_if_failed (err) {
//             // handle error in err
//         }
//         // code common to the failing and non-failing case
//         if (err)
//             return make_failure (unwrap err);  // manual propagation
//         // ... code continues if no failure ...
//     }
//
//=//// extract_failure(expr) /////////////////////////////////////////////=//
//
// Evaluates `expr`, then reads and clears the failure state, handing it back
// as a value.  Use it when you want the error as data rather than as control
// flow -- logging it, or deciding among several recoveries.
//
//     Error* e = extract_failure (Some_Result_Bearing_Function(args));
//     if (e)
//         Log_And_Continue(e);
//
//=/////////////////////////////////////////////////////////////////////////=//
//
// C. An attempt was made to actually subtype errors with Result(T,E) vs.
//    just Result(T), and enforce that you could only auto-propagate errors
//    out of compatible functions.  But injecting the type-awareness into
//    the body of the function is weird:
//
//       #define Result(T,E) /  /* Note: can't backslash in this comment */
//           template<typename RetError = E> /
//                ResultWrapper<T, E> /* function definition */
//
//    This way when you write `Result(T,E) Some_Func(...) {...}` you have
//    awareness of the return error type inside the body, for propagation
//    macros like return_if_failed() to use.
//
//    But it doesn't solve the issue for catch_if_failed(), which has to
//    telegraph the error type of the called function out of an expression
//    that has to be parenthesized -- impossible.  And that definition of
//    Result can't work in both a prototype and a definition, because it uses a
//    default template parameter that can only be defined once.  Also, if
//    you try to add inline like `INLINE Result(T, E) Some_Func(...) {...}`
//    that can't work because you can't put INLINE before the `template<>`
//
//    FURTHERMORE... there are limits to the ability to handle errors in a
//    polymorphic way that works in both C and C++.  C++ has inheritance and
//    that's the only way to beat strict aliasing, while C can use common
//    leading substructures which violate strict aliasing in C++.
//
//    AND FINALLY... Needful arose specifically for implementing Rebol, and
//    unlike Rust, Rebol's own error handling lacks a notion of statically
//    subclassing in its `except` and `trap` features.  When all of this is
//    considered together, it explains why Result(T) is not parameterized
//    by an error type, and just assumes one common error.
//


//=//// NEEDFUL_RESULT_0, More Lax Coercing Zero in C++ ////////////=//
//
// If you have code which wants to polymorphically be able to convert to an
// Option(SomeEnum) or SomePointer* or bool, etc. then this introduces a
// permissive notion of zero.  It lets you bring back some of the flexibility
// that C originally had with permissive 0 conversions, but more tightly
// controlled through a special type.
//
// 1. The main purpose of permissive zero is to be the polymorphic return
//    value of `make_failure(...)`, used as `return make_failure(...)`, which
//    is able to make the T in any Result(T) type.  Making it [[nodiscard]]
//    helps catch cases where someone omits the `return`, a mistake
//    (easy to make, as `panic (...)` looks similar and takes an error but
//    is *not* used with return.)
//

struct NEEDFUL_NODISCARD Result0Struct {  // [[nodiscard]] is good [1]
   // no members or behaviors, should only initialize ResultWrapper<T>
};

#undef NEEDFUL_RESULT_0
#define NEEDFUL_RESULT_0  needful::Result0Struct{}


//=//// RESULT WRAPPER ////////////////////////////////////////////////////=//
//
// The Result type is trickery that mimics something like Rust's Result<T, E>
// type.  It is a wrapper that characterizes a function which may return
// a failure by means of a global variable, but will construct the result
// from zero in that case.
//
// 1. The error machinery hinges on the ability to return a zerolike state
//    for anything that is a Result(T) in the case of a failure.  But rather
//    than allow Result to be constructed from any integer in the C++
//    checked build, it's narrowly constructible from Result0Struct,
//    which is what `return make_failure(...)` returns.
//

// IsResultWrapper is used in ResultWrapper's own SFINAE (to prevent
// ResultWrapper-from-ResultWrapper ambiguity), so the base template must
// precede the class definition.  Specialized to true_type after the class.
//
template<typename>
struct IsResultWrapper : std::false_type {};

template<typename T>
struct NEEDFUL_NODISCARD ResultWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD (T, r);

    ResultWrapper() = delete;

    ResultWrapper(Result0Struct)  // how failures are returned [1]
      : r {}
        {}

    template <
        typename U,
        typename = enable_if_t<
            not needful_is_convertible_v(decay_t<U>, ResultWrapper<T>)
            and not IsResultWrapper<decay_t<U>>::value
            and needful_is_convertible_v(U, T)  // implicit cast okay
        >
    >
    ResultWrapper(U&& something) : r {something} {}

    template <
        typename U,
        typename = enable_if_t<
            not needful_is_convertible_v(decay_t<U>, ResultWrapper<T>)
            and not IsResultWrapper<decay_t<U>>::value
            and not needful_is_convertible_v(U, T)  // must cast explicitly
        >,
        typename = void
    >
    explicit ResultWrapper(U&& something)
        : r {needful_c_cast(T, something)}
    {}

    template <
        typename X,
        typename = enable_if_t<needful_is_convertible_v(X, T)>
    >
    ResultWrapper (const ResultWrapper<X>& result)
        : r {result.r}
    {}

    template <
        typename X,
        typename = enable_if_t<not needful_is_convertible_v(X, T)>,
        typename = void
    >
    explicit ResultWrapper (const ResultWrapper<X>& result)
        : r {needful_c_cast(T, result.r)}
    {}
};

#undef NeedfulResult
#define NeedfulResult(T) /* not Result(T,E)... see [C] */ \
    needful::ResultWrapper<T>

template<typename X>
struct IsResultWrapper<ResultWrapper<X>> : std::true_type {};

template<typename X>  // Result carries error-signaling semantics [8]
struct IsWrapperSemantic<ResultWrapper<X>> : std::true_type {};


//=//// RESULT EXTRACTOR //////////////////////////////////////////////////=//
//
// 1. The choice of % for the result extractor has the goal of being able
//    to extract the result before it would get picked up by things like
//    `nocast` or `opt` or `unwrap`, which are prefix operators built on
//    `operator+` -- deliberately lower precedence than `%`:
//
//       return_if_failed (Foo* foo = opt Some_Function())
//
//    This expands to:
//
//       Foo* foo = g_opt_helper + Some_Function() % g_result_extractor;
//       /* more expansion of the return_if_failed macro */
//
//    The `%` binds first, peeling the ResultWrapper off, and only then does
//    the `+` see a plain Option(Foo*) to take the raw value out of.
//
// 2. The error is a bit opaque if you write:
//
//        return_if_failed (
//           Some_Function();
//        );
//
//    ignoring returned value of type needful::ResultWrapper<RebolValueStruct*>
//    declared with attribute 'nodiscard'
//
//    We try to give you a hint what's going on with the comment, if you
//    read on to the error about the operator not getting its left side.
//

struct ResultExtractor {};

template<typename T>
inline T operator%(  // % high postfix precedence desired [1]
    const ResultWrapper<T>& result,
    ResultExtractor
){
    return result.r;
}

static constexpr ResultExtractor g_result_extractor{};

#undef needful_postfix_extract_result
#define needful_postfix_extract_result \
    /* ; <-- ERROR? DON'T PUT SEMICOLON! [2] */ % needful::g_result_extractor
