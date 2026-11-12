//
//  file: %needful-status.hpp
//  summary: "Status code wrapper: zero is okay, and it has no truth value"
//  homepage: https://needful.metaeducation.com/status
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
// Status(T) is for the C convention where a function returns 0 on success
// and some other code otherwise:
//
//     Status(int) Flush_Buffer(Buffer* b) {
//         if (b->broken)
//             return ERR_BROKEN;
//         ...
//         return okay;
//     }
//
// In a plain C build it is just a T (with a must-use annotation where the
// compiler has one).  This file is the checked version.  It enforces:
//
// * It can't be ignored.  The wrapper is [[nodiscard]].
//
// * It has no truth value.  `if (Flush_Buffer(b))` reads as "if it worked"
//   and means "if it didn't".  So `if (status)`, `! status`, `status && x`
//   are all compile errors, and you say `is_okay(status)`.
//
// * It doesn't mix with Option(T) and Fallible(T).  Those also treat zero
//   as their special state, but with the opposite meaning: zero is `none`,
//   the *bad* one.  `return none;` here is an error, as is `return okay;`
//   in a function returning an Option(T).
//
// What counts as a status is deliberately loose: zero is okay, and anything
// else is some signal the caller must look at.  It isn't called "failed",
// because it may not be (think of a RETRY code).
//


//=//// OKAY //////////////////////////////////////////////////////////////=//
//
// In C, `okay` is just 0.  Here it's its own type, so that it only converts
// to a Status(T) and can't be returned where an Option(T) is expected.
//

struct OkayStruct {};

#undef needful_okay
#define needful_okay  needful::OkayStruct{}


//=//// STATUS WRAPPER ////////////////////////////////////////////////////=//
//
// 1. A status is made from a raw code: an integer or an enum.  Not from a
//    bool (`return true;` from a zero-is-okay function is nearly always a
//    bug, and exactly the confusion this type is here to stop), and not from
//    a pointer or an Option(T).
//
// 2. static_cast, so that e.g. building a Status(int) from a `long` code
//    doesn't raise a narrowing warning from inside this header.
//
// 3. Conversion to bool is declared so that the error can explain itself.
//    It's a template so that the static_assert only fires if it's used.
//
// 4. Comparing against a raw code works through [1]: `rc == ERR_BROKEN`
//    converts the code and compares two statuses.  (Friends defined in the
//    class are the kind of function that allows that conversion.)
//
// 5. There's no implicit conversion back to T.  If there were, `if (status)`
//    would compile by way of it.  `status_code()` gets the raw code out.
//

template<typename T>
struct NEEDFUL_NODISCARD StatusWrapper {
    NEEDFUL_DECLARE_WRAPPED_FIELD (T, s);

    StatusWrapper() = default;  // uninitialized, like a T would be

    StatusWrapper(OkayStruct) : s (static_cast<T>(0)) {}

    StatusWrapper(NoneStruct) = delete;  // `none` is Option(T)'s word

    template<
        typename U,
        typename = enable_if_t<  // raw codes only [1]
            (std::is_integral<U>::value or std::is_enum<U>::value)
            and not std::is_same<U, bool>::value
            and std::is_convertible<U, T>::value
        >
    >
    StatusWrapper(const U& code) : s (static_cast<T>(code)) {}  // [2]

    template<typename Dummy = int>
    explicit operator bool() const {  // [3]
        static_assert(
            sizeof(Dummy) != sizeof(Dummy),  // dependent false
            "A Status(T) has no truth value: zero means okay, so `if (status)`"
            " would mean \"if not okay\".  Use is_okay(status)."
        );
        return false;
    }

    friend bool operator==(const StatusWrapper& a, const StatusWrapper& b)
      { return a.s == b.s; }  // also compares against raw codes [4]

    friend bool operator!=(const StatusWrapper& a, const StatusWrapper& b)
      { return a.s != b.s; }
};

#undef NeedfulStatus
#define NeedfulStatus(T)  needful::StatusWrapper<T>

#undef NeedfulStatusVar  // same type: [[nodiscard]] is on the class here
#define NeedfulStatusVar(T)  needful::StatusWrapper<T>

template<typename X>  // else cast() would strip the wrapper, and the nodiscard
struct IsWrapperSemantic<StatusWrapper<X>> : std::true_type {};

#if NEEDFUL_USES_CORRUPT_HELPER
    template<typename T>
    struct CorruptHelper<StatusWrapper<T>> {
      static void corrupt(StatusWrapper<T>& status) {
        Corrupt_If_Needful(status.s);
      }
    };
#endif


//=//// is_okay() AND status_code() ///////////////////////////////////////=//
//
// These are the two ways to look at a status: ask if it's okay, or take the
// raw code (to `switch` on, log, or compare with `<`).
//
// Both also accept a raw integer or enum code.  That's for calling APIs you
// haven't annotated, e.g. is_okay(SQLFetch(stmt)).  They don't accept a bool,
// a pointer or an Option(T), so they can't be used to ask the backwards
// question of the wrong kind of value.
//

template<typename T>
constexpr bool Is_Okay(const StatusWrapper<T>& status)
  { return status.s == static_cast<T>(0); }

template<
    typename T,
    typename = enable_if_t<
        (std::is_integral<T>::value or std::is_enum<T>::value)
        and not std::is_same<T, bool>::value
    >
>
constexpr bool Is_Okay(const T& code)
  { return code == static_cast<T>(0); }

template<typename T>
constexpr T Status_Code(const StatusWrapper<T>& status)
  { return status.s; }

template<
    typename T,
    typename = enable_if_t<
        (std::is_integral<T>::value or std::is_enum<T>::value)
        and not std::is_same<T, bool>::value
    >
>
constexpr T Status_Code(const T& code)
  { return code; }

#undef needful_is_okay
#define needful_is_okay(_expr_)  needful::Is_Okay(_expr_)

#undef needful_status_code
#define needful_status_code(_expr_)  needful::Status_Code(_expr_)
