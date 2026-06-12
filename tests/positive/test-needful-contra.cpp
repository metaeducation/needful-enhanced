// test-needful-contra.cpp
// Tests for Sink(T), Init(T), Contra(T): contravariant output parameters.
//
// Ren-C uses stylized inheritance where derived types are MORE constrained
// (more specific) than their bases.  So Sink(Derived) accepting Base*
// is SAFE: the function writes more-constrained bits into a less-constrained
// slot.  Sink(Base) accepting Derived* is UNSAFE (less-constrained bits
// written to a more-constrained slot) and is blocked at compile time.
//
// All types that participate must be standard-layout and same-sizeof.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>
#include <cstring>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_CONTRA_SHORTHANDS  1
#include "needful.h"

#define STATIC_ASSERT  NEEDFUL_STATIC_ASSERT

#include <type_traits>


//=//// TEST TYPE HIERARCHY ///////////////////////////////////////////////=//
//
// Base is less constrained; Derived is more constrained (no extra members).
// This models Ren-C's Cell <- Value <- Element style hierarchy.

struct Base {
    int bits;
};

struct Derived : Base {
    // No new members: same layout, same sizeof, standard layout
};

static_assert(std::is_standard_layout<Base>::value,    "Base must be std layout");
static_assert(std::is_standard_layout<Derived>::value, "Derived must be std layout");
static_assert(sizeof(Base) == sizeof(Derived),          "Must be same size");
static_assert(std::is_base_of<Base, Derived>::value,    "Derived must derive from Base");


//=//// Sink(T): CONTRAVARIANT OUTPUT PARAMETER ///////////////////////////=//
//
// A function that writes T (more constrained = Derived) may safely accept
// a pointer to a Base slot (less constrained), because the written bits
// are also valid for Base.

static void write_derived(Sink(Derived) out) {
    // When accessed as T*, Sink corrupts the target first (in debug builds)
    Derived* p = out;  // implicit conversion to T*
    p->bits = 42;
}

void test_sink_accepts_base_pointer() {
    Base base { 0 };

    // Sink(Derived) accepts Base* — Base is a base of Derived (SAFE direction)
    write_derived(&base);
    assert(base.bits == 42);
}

void test_sink_accepts_identity_pointer() {
    Derived d {};
    d.bits = 0;

    write_derived(&d);
    assert(d.bits == 42);
}


//=//// Sink(T): STATIC ASSERTIONS ///////////////////////////////////////=//

void test_sink_type_properties() {
    // Sink(Derived) should accept Base* (Base is a base of Derived)
    STATIC_ASSERT((needful::IsContravariant<Base*, Derived>::value));

    // Sink(Base) should NOT accept Derived* (Derived is not a base of Base)
    STATIC_ASSERT(not (needful::IsContravariant<Derived*, Base>::value));

    // Sink(T) is distinct from T*
    STATIC_ASSERT(not std::is_same<Sink(Base), Base*>::value);
}


//=//// Init(T): CONTRAVARIANT OUTPUT PARAMETER (INITIALIZATION) //////////=//
//
// Init(T) has the same contravariance semantics as Sink(T) but is
// semantically for first initialization rather than overwrite.

static void init_derived(Init(Derived) out) {
    Derived* p = out;  // implicit conversion to T*
    p->bits = 99;
}

void test_init_accepts_base_pointer() {
    Base base { 0 };
    init_derived(&base);
    assert(base.bits == 99);
}

void test_init_accepts_identity_pointer() {
    Derived d {};
    init_derived(&d);
    assert(d.bits == 99);
}


//=//// Contra(T): LIGHTWEIGHT READ-THROUGH CONTRAVARIANCE ////////////////=//
//
// Contra(T) = ContraWrapper<T*>: like Sink but no corruption pending.

static void read_contra(Contra(Derived) in) {
    const Derived* p = in;  // implicit conversion to T*
    assert(p->bits == 7);
}

void test_contra_accepts_base_pointer() {
    Base base { 7 };
    read_contra(&base);
}

void test_contra_accepts_identity() {
    Derived d {};
    d.bits = 7;
    read_contra(&d);
}

void test_contra_null() {
    // Contra accepts nullptr (unlike Need)
    Contra(Derived) c = nullptr;
    STATIC_ASSERT(not std::is_same<decltype(c), Derived*>::value);  // distinct type
    Derived* p = c;
    assert(p == nullptr);
}


//=//// Sink(T): EXPLICIT BOOL, DISTINCT FROM T* /////////////////////////=//

void test_sink_bool_and_distinct_type() {
    // Sink(T) is a distinct type from T*
    STATIC_ASSERT(not std::is_same<Sink(Base), Base*>::value);

    // Explicit bool cast works (operator T*() -> bool)
    Base b { 5 };
    Sink(Derived) s = &b;
    bool non_null = (bool)s;
    assert(non_null);

    Sink(Derived) snull = nullptr;
    assert(not (bool)snull);
}


int main() {
    test_sink_accepts_base_pointer();
    test_sink_accepts_identity_pointer();
    test_sink_type_properties();
    test_init_accepts_base_pointer();
    test_init_accepts_identity_pointer();
    test_contra_accepts_base_pointer();
    test_contra_accepts_identity();
    test_contra_null();
    test_sink_bool_and_distinct_type();
    return 0;
}
