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
// NeedWrapper allows implicit conversion to its underlying type, making it
// largely transparent in usage. However, it blocks boolean coercions and
// explicit null/zero assignments to catch bugs at compile-time.
//
// 1. Need(T) works with pointers or non-pointer types like integers or enums
//    (unlike Sink(T) or Init(T) which intrinsically imply pointers).
//
// 2. The primary purpose of the existence of Need() is to stop implicit
//    conversions to bool.  A public `operator bool() const = delete` is the
//    key: when the compiler resolves a bool conversion, the deleted operator
//    is the *best* match (no extra standard conversion needed vs. the
//    operator T() → T* → bool two-step), so it wins and is ill-formed.
//    Keeping it public (not private) gives "use of deleted function" errors
//    rather than access errors, and lets std::is_convertible return false
//    cleanly in C++17 rather than a hard error.  The enable_if on the
//    explicit operator U() template is a secondary defense to block
//    static_cast<bool>() routes that bypass the implicit path.
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


//=/// UNWRAP HELPER OVERLOAD FOR Need(T) /////////////////////////////////=//
//
// `unwrap` is spelled as a prefix "keyword" by making it the left operand of
// operator+, so `unwrap foo` parses as `g_unwrap_helper + foo`:
//
//    Some_Function(unwrap foo)  =>  Some_Function(g_unwrap_helper + foo)
//
// The tag type and the macro are in %needful-wrapping.hpp, which is included
// unconditionally, because `unwrap` is shared vocabulary.  Only this overload
// is Need's business.  Why `+` here and `%` for Result(T) extraction, and
// which expressions need parentheses as a result:
//
//    https://needful.metaeducation.com/precedence

template<typename T>
T operator+(  // lower precedence than %
    UnwrapHelper,
    const NeedWrapper<T>& need
){
    return need.n;  // never allowed to be zero or null
}


//=/// "NEEDED" HELPER CLASS //////////////////////////////////////////////=//
//
// The `needed` operator extracts the raw value from Need(T), analogous to how
// `opt` extracts from Option(T).  The key difference is that `needed` gives a
// compile-time error on Option(T), making it useful as a static filter:
//
//    | Operator | On Option(T)  | On Need(T) |
//    |----------|---------------|------------|
//    | opt      | Extract raw   | Error      |
//    | needed   | Error         | Extract raw|
//    | unwrap   | Assert+Extract| Extract    |
//
// This is useful for building macros that predictably reject optional types.
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
