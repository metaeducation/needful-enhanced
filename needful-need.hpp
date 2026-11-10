//
//  file: %needful-need.hpp
//  summary: "Need Wrapper Trick to get Non-Boolean-Coercible Types"
//  homepage: https://needful.metaeducation.com/need
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
// See %needful.h for an overview of Need(T).
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
//


//=//// NeedWrapper: STRICT NON-NULL / NON-BOOLEAN TYPE /////////////////////=//
//
// NeedWrapper is mostly transparent as T, but blocks bool coercion and
// null-like construction/assignment to catch misuse at compile time.
//
// 1. Need(T) works with pointers or non-pointer types like integers or enums
//    (unlike Sink(T) or Init(T) which intrinsically imply pointers).
//
// 2. Public deleted operator bool() is the main guard: it is the best match
//    for bool conversion, so misuse fails with a direct diagnostic.  The
//    constrained explicit operator U() is a second guard against cast paths.
//
// 3. Non-dependent enable_if conditions work in MSVC, but GCC has trouble
//    with them.  Introducing a dependent type seems to help it along.


#undef NeedfulNeed
#define NeedfulNeed(T) \
    needful::NeedWrapper<T>  // * not implicit [1]

template<typename T>
struct NeedWrapper {
  public:
    NEEDFUL_DECLARE_WRAPPED_FIELD (T, n);

    NeedWrapper() = default;  // need compatibility with C build behavior

    NeedWrapper(std::nullptr_t) = delete;
    NeedWrapper(NoneStruct) = delete;

    template <
        typename U,
        typename = enable_if_t<not needful_is_convertible_v(U, T)>
    >
    explicit NeedWrapper(const U& something)
        : n {needful_c_cast(T, something)}
            { NEEDFUL_ASSERT(n); }

    template <
        typename U,
        typename = enable_if_t<needful_is_convertible_v(U, T)>
    >
    NeedWrapper(U&& u) : n {static_cast<T>(u)} { NEEDFUL_ASSERT(n); }

    template <
        typename U,
        typename = enable_if_t<needful_is_convertible_v(U, T)>
    >
    NeedWrapper(const NeedWrapper<U>& other) : n {other.n}
        {}  // shouldn't need to assert(n)

    NeedWrapper(const NeedWrapper& other) : n {other.n}
        {}  // shouldn't need to assert(n)

    operator bool() const = delete;  // see note [2]

    NeedWrapper& operator=(std::nullptr_t) = delete;
    NeedWrapper& operator=(NoneStruct) = delete;

    NeedWrapper& operator=(const NeedWrapper& other) {
        if (this != &other) {
            this->n = other.n;
            NEEDFUL_ASSERT(this->n);
        }
        return *this;
    }

    template<
        typename U,
        typename = enable_if_t<needful_is_convertible_v(U, T)>
    >
    NeedWrapper& operator=(U&& u) {
        this->n = static_cast<T>(u);
        NEEDFUL_ASSERT(this->n);
        return *this;
    }

    operator T() const { return n; }

    operator ExactWrapper<needful_constify_t(T)>() const { return n; }

    template<
        typename U,
        typename = enable_if_t<not std::is_same<U, bool>::value>  // [2]
    >
    explicit operator U() const
        { return static_cast<U>(n); }

    T operator->() const { return n; }
};

  //=//// LABORIOUS REPEATED OPERATORS ////////////////////////////////////=//

  // While the combinatorics may seem excessive with repeating the equality
  // and inequality operators, this is the way std::optional does it too.

template<typename L, typename R>
bool operator==(const NeedWrapper<L>& left, const NeedWrapper<R>& right)
  { return left.n == right.n; }

template<typename L, typename R>
bool operator==(const NeedWrapper<L>& left, R right)
  { return left.n == right; }

template<typename L, typename R>
bool operator==(L left, const NeedWrapper<R>& right)
  { return left == right.n; }

template<typename L, typename R>
bool operator!=(const NeedWrapper<L>& left, const NeedWrapper<R>& right)
  { return left.n != right.n; }

template<typename L, typename R>
bool operator!=(const NeedWrapper<L>& left, R right)
  { return left.n != right; }

template<typename L, typename R>
bool operator!=(L left, const NeedWrapper<R>& right)
  { return left != right.n; }


// Vouch NeedWrapper<T> safe for needful_void_waypoint_cast (see
// NeedfulVoidWaypointSafe in needful-utilities.hpp): its implicit
// `operator T()`, implicit `operator ExactWrapper<...>()`, and templated
// explicit `operator U()` all just hand back the same underlying field `n`
// (via static_cast, with no side effects), so it doesn't matter which one a
// (void*) cast actually picks -- they're all value-preserving views of `n`.
//
template<typename T>
struct NeedfulVoidWaypointSafe<NeedWrapper<T>> : std::true_type {};


//=/// "NEEDED" HELPER CLASS //////////////////////////////////////////////=//
//
// The `needed` operator extracts the raw value from Need(T), analogous to how
// `opt` and `unwrap` extract from Option(T).  Each operator belongs to exactly
// one of the two types, and is a compile-time error on the other:
//
//    | Operator | On Option(T)  | On Need(T) |
//    |----------|---------------|------------|
//    | opt      | Extract raw   | Error      |
//    | unwrap   | Assert+Extract| Error      |
//    | needed   | Error         | Extract raw|
//
// This makes `needed` useful for macros that predictably reject optional
// types.  (Most code won't need it at all, as Need(T) converts implicitly.)
//
// 1. `unwrap` is not accepted on Need(T).  If it were, then a build with only
//    one of the two wrappers enabled would leave the other's type as a plain
//    T--and `unwrap` would have to accept plain T's, losing the check that
//    you only `unwrap` things that are optional.  With `unwrap` belonging
//    only to Option(T), that check holds whenever Option(T) is wrapped.
//

struct NeededHelper {};
constexpr NeededHelper g_needed_helper = {};

#undef needful_needed
#define needful_needed \
    needful::g_needed_helper +  // lower precedence than %


template<typename T>
T operator+(  // lower precedence than %
    NeededHelper,
    const NeedWrapper<T>& need
){
    return need.n;  // never allowed to be zero or null
}
