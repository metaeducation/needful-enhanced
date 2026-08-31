//
//  file: %needful-const.hpp
//  summary: "Mutability transference from input arguments to return results"
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


//=//// CONSTIFY/UNCONSTIFY: ADD/REMOVE CONST ON POSSIBLY-WRAPPED TYPE ////=//
//
// Needful needs predictable const add/remove across wrapped pointer-like
// types.  The helper below treats top-level const separately so function
// returns like const int are avoided in warning-heavy toolchains [1].
//

template<typename T, bool TopLevel = true, typename Enable = void>
struct ConstifyHelper {
    using consted = T;  // don't add const for top level [1]
    using unconsted = remove_const_t<T>;
};

template<typename T>
struct ConstifyHelper<T, false, enable_if_t<  // TopLevel = false
    not HasWrappedType<T>::value
    and not std::is_pointer<T>::value
    and not std::is_function<T>::value
>> {
    using consted = const T;
    using unconsted = remove_const_t<T>;
};

template<typename T, bool TopLevel>
struct ConstifyHelper<T*, TopLevel, void> {  // pointed-to things, any level
    using consted = const T*;  // even top level adds const
    using unconsted = typename ConstifyHelper<T, false>::unconsted*;
};

template<typename T, bool TopLevel>
struct ConstifyHelper<T* const, TopLevel, void> {  // pointer *itself* is const
    using consted = const T* const;
    using unconsted = typename ConstifyHelper<T, false>::unconsted* const;
};

template<typename Ret, typename... Args, bool TopLevel>
struct ConstifyHelper<Ret(*)(Args...), TopLevel, void> {  // function pointers
    using consted = Ret(*)(Args...);  // function pointers can't be const
    using unconsted = Ret(*)(Args...);
};

template<typename Ret, typename... Args, bool TopLevel>
struct ConstifyHelper<Ret(* const)(Args...), TopLevel, void> {
    using consted = Ret(* const)(Args...);  // preserve const pointer-ness
    using unconsted = Ret(*)(Args...);
};

// For recursion inside wrappers, use TopLevel = false.
//
// 1. Rewrappable wrappers recurse into wrapped_type and rebuild via
//    needful_rewrap_type(...).
// 2. Non-rewrappable wrappers only receive top-level const changes.
//
template<typename T, bool TopLevel>  // rewrappable wrappers [1]
struct ConstifyHelper<T, TopLevel, enable_if_t<
    HasWrappedType<T>::value and IsRewrappable<T>::value
>>
{
  private:
    using wrapped_consted = typename ConstifyHelper<
        typename T::wrapped_type, false
    >::consted;

    using wrapped_unconsted = typename ConstifyHelper<
        typename T::wrapped_type, false
    >::unconsted;

  public:
    using consted = needful_rewrap_type(T, wrapped_consted);
    using unconsted = needful_rewrap_type(T, wrapped_unconsted);
};

template<typename T, bool TopLevel>  // non-rewrappable wrappers [2]
struct ConstifyHelper<T, TopLevel, enable_if_t<
    HasWrappedType<T>::value and not IsRewrappable<T>::value
>>
{
    using consted = const T;
    using unconsted = remove_const_t<T>;
};

#define needful_constify_t(T) \
    typename needful::ConstifyHelper< \
        needful::remove_reference_t<T>, true>::consted

#define needful_unconstify_t(T) \
    typename needful::ConstifyHelper< \
        needful::remove_reference_t<T>, true>::unconsted


//=//// IsConstIrrelevantForType: DODGE GCC WARNINGS //////////////////////=//
//
// It would be nice if we could just mirror const onto things without
// special-casing things.  But GCC will warn if you try to make const enum
// values or things of the sort e.g. as a return type from a function
// (it shouldn't matter, but the warning is probably there just to help make
// sure you aren't misunderstanding and thinking the const does something).
//
// So we have special handling for types where constness is irrelevant.
//
// 1. If you ask about decltype("abc"), then sometimes that will ask about
//    a const char [] array, for which the constness is relevant, though
//    it is not directly a pointer.
//

template<typename T>
struct IsConstIrrelevant : std::integral_constant<
    bool,
    not std::is_pointer<T>::value  // note: must decay for arrays [1]
    and not HasWrappedType<T>::value
> {};

#define needful_is_const_irrelevant_v(T) \
    needful::IsConstIrrelevant<needful::decay_t<T>>::value  // decay [1]


//=/// IsConstlikeType: SMART-POINTER EXTENSIBLE CONSTNESS CHECK //////////=//
//
// This helper for testing if something is "constlike" is able to return
// true for things like Option(const char*), and false for Option(char*).
//
// It does so by building on top of the hook that such types already have
// to write for ConstifyHelper<T>... all it does is check to see if the
// type is the same as its constification!

template<typename T>
struct IsConstlike {
    static constexpr bool value
        = std::is_same<T, needful_constify_t(T)>::value;
};

#define needful_is_constlike_v(T) \
    needful::IsConstlike<needful::remove_reference_t<T>>::value


//=//// CONST MIRRORING: MATCH CONSTNESS OF ONE TYPE ONTO ANOTHER ////////=//
//
// This const mirroring builds on top of the ConstifyHelper, rather than
// needing to be a separate construct that things like smart pointers need
// to overload.  The concept is that it checks to see if a type is the same
// as its constification, and if so then it's const.
//

template<typename From, typename To>
struct MirrorConstHelper {
    using type = conditional_t<
        needful_is_const_irrelevant_v(From),
        To,  // leave as-is for const-irrelevant types
        conditional_t<
            needful_is_constlike_v(From),  // mirror constness otherwise
            needful_constify_t(To),
            needful_unconstify_t(To)
        >
    >;
};

#define needful_mirror_const_t(From, To) \
    typename needful::MirrorConstHelper<From, To>::type


//=//// CONST MERGING: ADD ANY CONSTNESS FROM ONE TYPE ONTO ANOTHER ///////=//
//
// This is a slight variation on the MirrorConstHelper, which will not make
// the constness match (so it doesn't unconstify), but will add constness.
//

template<typename From, typename To>
struct MergeConstHelper {
    using type = conditional_t<
        not needful_is_const_irrelevant_v(From)
            and needful_is_constlike_v(From),
        needful_constify_t(To),
        To  // don't unconstify (see needful_mirror_const_t() for that)
    >;
};

#define needful_merge_const_t(From, To) \
    typename needful::MergeConstHelper<From, To>::type
