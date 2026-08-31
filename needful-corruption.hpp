//
//  file: %needful-corruption.hpp
//  summary: "Helpers for deliberately corrupting memory in debug builds"
//  homepage: https://needful.metaeducation.com/contra
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

#if defined(__clang_analyzer__)
    STATIC_FAIL("Don't use NEEDFUL_DOES_CORRUPTIONS with clang-analyzer");
#endif

#if defined(_MSC_VER) && defined(_PREFAST_)  // _PREFAST_ if MSVC /analyze
    STATIC_FAIL("Don't use NEEDFUL_DOES_CORRUPTIONS with MSVC /analyze");
#endif


#if !defined(NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS)
    #define NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS  1  // more expensive
#endif

#if defined(__clang__)
    #define NEEDFUL_CORRUPTION_SEED 5  // e.g. 5th corrupt pointer is zero [2]
    #define NEEDFUL_CORRUPTION_DOSE 11
#else
    #define NEEDFUL_CORRUPTION_SEED 0  // e.g. 1st corrupt pointer is zero [2]
    #define NEEDFUL_CORRUPTION_DOSE 7
#endif


//=//// THREADING MODEL FOR CORRUPTION COUNTDOWNS /////////////////////////=//
//
// Pseudo-random corruption uses countdown state.  In enhanced builds,
// default to atomics so this state is race-free without extra setup.
//
// Integrators can override in two ways:
//
// 1. Define all three custom synchronization hooks:
//
//      NEEDFUL_DECLARE_ATOMIC_VAR(name)
//      NEEDFUL_LOCK_VARIABLE(var)
//      NEEDFUL_UNLOCK_VARIABLE(var)
//
//    This path avoids the <atomic> dependency while still serializing updates
//    through project-provided synchronization.
//
// 2. Define NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED 1 to force plain
//    non-atomic statics (fastest, but only valid when truly single-threaded).
//

#if defined(NEEDFUL_DECLARE_ATOMIC_VAR) || defined(NEEDFUL_LOCK_VARIABLE) || defined(NEEDFUL_UNLOCK_VARIABLE)
  #if !defined(NEEDFUL_DECLARE_ATOMIC_VAR)
    STATIC_FAIL("Define NEEDFUL_DECLARE_ATOMIC_VAR(name) with NEEDFUL_LOCK_VARIABLE/NEEDFUL_UNLOCK_VARIABLE");
  #endif

  #if !defined(NEEDFUL_LOCK_VARIABLE)
    STATIC_FAIL("Define NEEDFUL_LOCK_VARIABLE(var) with NEEDFUL_DECLARE_ATOMIC_VAR/NEEDFUL_UNLOCK_VARIABLE");
  #endif

  #if !defined(NEEDFUL_UNLOCK_VARIABLE)
    STATIC_FAIL("Define NEEDFUL_UNLOCK_VARIABLE(var) with NEEDFUL_DECLARE_ATOMIC_VAR/NEEDFUL_LOCK_VARIABLE");
  #endif

  #define NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS  1
#else
  #define NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS  0
#endif

#if !defined(NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED)
  #define NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED  0
#endif

#if !defined(NEEDFUL_CORRUPTION_USE_ATOMICS)
  #if NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS
    #define NEEDFUL_CORRUPTION_USE_ATOMICS  0
  #else
    #define NEEDFUL_CORRUPTION_USE_ATOMICS  1
  #endif
#endif

#if NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED && NEEDFUL_CORRUPTION_USE_ATOMICS
  STATIC_FAIL("Choose one: NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED or NEEDFUL_CORRUPTION_USE_ATOMICS");
#endif

#if NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED && NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS
  STATIC_FAIL("NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED conflicts with NEEDFUL_DECLARE_ATOMIC_VAR/LOCK/UNLOCK hooks");
#endif

#if NEEDFUL_CORRUPTION_USE_ATOMICS && NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS
  STATIC_FAIL("NEEDFUL_CORRUPTION_USE_ATOMICS conflicts with NEEDFUL_DECLARE_ATOMIC_VAR/LOCK/UNLOCK hooks");
#endif

#if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS \
  && (! NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS) \
  && (! NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED) \
  && (! NEEDFUL_CORRUPTION_USE_ATOMICS)
  STATIC_FAIL("Pseudo-random corruption requires atomics, custom sync hooks, or NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED 1");
#endif

#if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS
  template<typename Tag>
  NEEDFUL_ALWAYS_INLINE uint_fast8_t Next_Corruption_Countdown() {
    #if NEEDFUL_CORRUPTION_CUSTOM_SYNC_HOOKS
    static NEEDFUL_DECLARE_ATOMIC_VAR(countdown) = NEEDFUL_CORRUPTION_SEED;
    NEEDFUL_LOCK_VARIABLE(countdown);
    const uint_fast8_t out = i_cast(uint_fast8_t, countdown);
    if (out == 0)
      countdown = i_cast(uint_fast8_t, NEEDFUL_CORRUPTION_DOSE - 1);
    else
      countdown = i_cast(uint_fast8_t, out - 1);
    NEEDFUL_UNLOCK_VARIABLE(countdown);
    return out;

    #elif NEEDFUL_CORRUPTION_ASSUME_SINGLE_THREADED
    static uint_fast8_t countdown = NEEDFUL_CORRUPTION_SEED;
    const uint_fast8_t out = countdown;
    if (countdown == 0)
      countdown = NEEDFUL_CORRUPTION_DOSE;
    --countdown;
    return out;

    #else
    static std::atomic<uint_fast8_t> countdown(NEEDFUL_CORRUPTION_SEED);
    uint_fast8_t old = countdown.load(std::memory_order_relaxed);
    for (;;) {
      const uint_fast8_t next = (old == 0)
        ? i_cast(uint_fast8_t, NEEDFUL_CORRUPTION_DOSE - 1)
        : i_cast(uint_fast8_t, old - 1);

      if (countdown.compare_exchange_weak(
        old,
        next,
        std::memory_order_relaxed,
        std::memory_order_relaxed
      ))
        return old;
    }
    #endif
  }
#endif


//=//// CORRUPTION HELPER /////////////////////////////////////////////////=//
//
// Helper struct for Corrupt_If_Needful() with generalized memset() fallback.
//
// Function templates can't be partially specialized, class templates can.
// Using a struct allows us to provide custom behavior for type families
// (e.g. "all classes derived from some Base") while still being able to
// have a generic fallback.  (A function template would see a generic
// fallback as ambiguous with SFINAE trying to "carve up" the space.)
//
// 1. It's unsafe to memory fill an arbitrary C++ class by value with
//    garbage bytes, because they can have extra vtables and such--you
//    can overwrite private compiler data.  But this is a C codebase
//    which uses just a few C++ features.  If you don't have virtual
//    methods then is_trivially_copyable<> should be true, and the memset()
//    shouldn't be a problem...
//
// 2. See definition of Cell and Mem_Fill() for why casting to void* is
//    needed.  (Mem_Fill() macro is not defined in the Needful library)
//
// 3. Having tried a lot of variations of this code...including using masking
//    to avoid branching...it seems that using uint_fast8_t and decrement
//    with a test against 0 is about the fastest way to get good periodicity
//    of zeroing and non-zeroing.
//
// 4. Use macro for efficiency, avoid another function call overhead
//
//    decltype(ref) deduces the type of ref (incl. reference/cv qualifiers)
//
//    remove_reference strips the reference, so the template matches
//    the CorruptHelper<T> specializations
//

template<typename T, typename Enable = void>
struct CorruptHelper {
  static_assert(
    std::is_trivially_copyable<T>::value or std::is_union<T>::value,
    "Cannot memset() a non-trivial C++ class (union fallback is permitted)"
  );

  static_assert(  // see common specializations later in this file
    not std::is_pointer<T>::value
        and not std::is_fundamental<T>::value
        and not std::is_enum<T>::value,
    "Fallback CorruptHelper<T> should be overridden by specialization"
  );

  static void corrupt(T& ref) {  // fallback if no other specialization
  #if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS
    struct CountdownTag;
    const uint_fast8_t countdown = Next_Corruption_Countdown<CountdownTag>();
    memset(
        raw_cast(void*, &ref),  // void* cast needed [2]
        countdown,  // countdown does double-duty as the fill byte
        sizeof(T)
    );
  #else
    memset(raw_cast(void*, &ref), 0xBD, sizeof(T));
  #endif
  }
};

#undef Corrupt_If_Needful
#define Corrupt_If_Needful(ref) /* macro for efficiency [4] */ \
    needful::CorruptHelper< \
        needful::remove_reference_t<decltype(ref)> \
    >::corrupt(ref)

STATIC_ASSERT(NEEDFUL_USES_CORRUPT_HELPER == 0);

#undef NEEDFUL_USES_CORRUPT_HELPER
#define NEEDFUL_USES_CORRUPT_HELPER  1


//=//// POINTER CORRUPTION ////////////////////////////////////////////////=//
//
// 1. Unlike the standard memset() fallback which doesn't know what it's
//    corrupting, this pointer corrupter knows...and there's not a lot of
//    good reason to pay additional cost to try and randomize states vs.
//    "bad pointer" and "null pointer".

template<typename T>
struct CorruptHelper<T*> {  // Pointer (faster than memset() fallback)
    static void corrupt(T*& ref) {
    #if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS
    struct CountdownTag;
    const uint_fast8_t countdown = Next_Corruption_Countdown<CountdownTag>();
      if (countdown == 0) {
          ref = nullptr;  // nullptr occasionally, deterministic
      }
      else {
          ref = p_cast(T*, i_cast(intptr_t, 0xDECAFBAD));  // fixed value [1]
      }
    #else
      ref = p_cast(T*, i_cast(intptr_t, 0xDECAFBAD));  // fixed value [1]
    #endif
    }

    static bool is_corrupt(T* ptr) {
      #if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS
        if (ptr == nullptr)
            return true;  // have to consider nullptr potentially corrupt
      #endif
        return ptr == p_cast(T*, i_cast(intptr_t, 0xDECAFBAD));
    }
};

#undef Assert_Corrupted_If_Needful
#define Assert_Corrupted_If_Needful(ptr) \
  NEEDFUL_ASSERT(needful::CorruptHelper<decltype(ptr)>::is_corrupt(ptr))


//=//// BOOLEAN CORRUPTION (MUST PRODUCE BOTH TRUE + FALSE) ////////////////=//
//
// 1. Booleans are special in the sense that writing a fixed garbage value
//    into them is not attention-getting, since they're only interpreted as
//    true and false.  Always use pseudorandom values to corrupt them, even
//    if the build requests not to use NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS.
//
template<>
struct CorruptHelper<bool> {
    static void corrupt(bool& ref) {
        possibly(NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS);  // ignore it [1]
    struct CountdownTag;
    const uint_fast8_t countdown = Next_Corruption_Countdown<CountdownTag>();
        ref = (countdown & 0x1);
    }
};


//=//// NON-POINTER CORRUPTION FOR FUNDAMENTALS/ENUMS /////////////////////=//
//

template<typename T>
struct CorruptHelper<
    T,  // Integer/bool/float/enum/etc. (faster than memset() fallback)
    enable_if_t<
        not std::is_same<T, bool>::value and (
            std::is_fundamental<T>::value
            or std::is_enum<T>::value
        )
    >
>{
    static void corrupt(T& ref) {
    #if NEEDFUL_PSEUDO_RANDOM_CORRUPTIONS
    struct CountdownTag;
    const uint_fast8_t countdown = Next_Corruption_Countdown<CountdownTag>();
      if (countdown == 0) {
          ref = static_cast<T>(0);  // false/0 occasionally, deterministic
      }
      else {
          ref = static_cast<T>(12345678);  // garbage the rest of the time
      }
    #else
      ref = static_cast<T>(12345678);
    #endif
    }
};


//=//// GIVE UNUSED() "TEETH" TO CORRUPT VARIABLES ////////////////////////=//

template<typename T>
void Unused_Helper(const T& ref)  // const reference... can't corrupt
  { USED(ref); }

template<typename T>
void Unused_Helper(T& ref)  // mutable references... we can corrupt
  { Corrupt_If_Needful(ref); }

// Redefine the PREFIXED macro, not the shorthand.  Two reasons, and needful.h
// already assumes this is how it works:
//
// 1. `UNUSED` is an optional cosmetic alias.  needful.h only defines it when
//    NEEDFUL_USAGE_SHORTHANDS is on, and even then guards with `#if
//    !defined(UNUSED)` so a project that already has its own `UNUSED` keeps
//    it.  Doing an unguarded `#undef UNUSED` here threw that away: it
//    destroyed a caller's macro, and conjured up a `UNUSED` even for projects
//    that deliberately left the shorthands off.
//
// 2. Redefining only the alias meant needful.h's own internal NEEDFUL_UNUSED()
//    calls never gained the teeth -- while %needful.h's comment on
//    NEEDFUL_USED/NEEDFUL_UNUSED said they did, and needful-casts.hpp worked
//    around a call to Unused_Helper that could not actually happen.  Both of
//    those are now true statements.
//
// Redefining NEEDFUL_UNUSED gets the shorthand for free, since `UNUSED`
// expands to `NEEDFUL_UNUSED` at the point of use, not at the point of
// definition.

#undef NEEDFUL_UNUSED
#define NEEDFUL_UNUSED(...)  needful::Unused_Helper(__VA_ARGS__)
