// test-needful-comments.cpp
// Tests for commentary macros: possibly/impossible/definitely (bool-constrained)
// and unnecessary/dont/cant/inapplicable (expression-valid constrained).
//
// These macros are no-ops at runtime but in C++ builds the enhanced versions
// call STATIC_ASSERT_DECLTYPE_BOOL or STATIC_ASSERT_DECLTYPE_VALID, so they
// fail to compile if the expressions they annotate become invalid.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_COMMENT_SHORTHANDS  1
#include "needful.h"


//=//// possibly/impossible/definitely: BOOL-CONSTRAINED /////////////////=//
//
// These macros require that their argument be explicitly convertible to bool.
// This catches stale variable names, typos, etc.

void test_possibly() {
    int x = 10;
    possibly(x > 0);        // documents that x might be > 0
    possibly(x < 100);      // documents that x might be < 100
}

void test_impossible() {
    int x = 10;
    impossible(x > 1000);   // documents this can't happen given constraints
    impossible(x < 0);
}

void test_definitely() {
    int x = 10;
    definitely(x > 0);      // documents this is an invariant
    definitely(x == 10);
}

void test_bool_constrained_with_pointer() {
    int val = 5;
    int* p = &val;
    possibly(p != nullptr);
    definitely(p);
    impossible(p == nullptr);
}


//=//// unnecessary/dont/cant/inapplicable: EXPRESSION-VALID //////////////=//
//
// These only require the expression to be syntactically valid (any type).

void test_unnecessary() {
    int x = 5;
    int y = x + 1;      // y is set but unnecessary is documenting it
    unnecessary(y);     // documents: y isn't needed but is intentionally here
    NEEDFUL_UNUSED(y);
}

void test_dont() {
    int x = 10;
    // Documents: don't do this (and checks the expression is still valid)
    dont(x = 0);  // we don't assign 0, but the expression is still valid
    NEEDFUL_UNUSED(x);
}

void test_cant() {
    // Documents: can't do this due to a current limitation
    const int* p = nullptr;
    cant(const_cast<int*>(p));  // we could const_cast but we're noting we cant
    NEEDFUL_UNUSED(p);
}

void test_inapplicable() {
    int x = 7;
    inapplicable(x * 2);  // documents: not applicable in this context
    NEEDFUL_UNUSED(x);
}


//=//// heeded: MARKS SIDE-EFFECTFUL EXPRESSIONS /////////////////////////=//
//
// heeded() is for things that look stray but have intentional side effects.

void test_heeded() {
    int x = 0;
    heeded(x += 1);   // side effect is intentional
    assert(x == 1);
}


//=//// STATIC_ASSERT, STATIC_IGNORE, STATIC_FAIL ////////////////////////=//

void test_static_assert() {
    NEEDFUL_STATIC_ASSERT(1 + 1 == 2);
    NEEDFUL_STATIC_ASSERT(not (1 + 1 == 3));  // STATIC_ASSERT_NOT has no shorthand
    NEEDFUL_STATIC_IGNORE(0 == 1);  // ignored: no error even for false
}

void test_static_assert_lvalue() {
    int x = 10;
    NEEDFUL_STATIC_ASSERT_LVALUE(x);  // x is an lvalue: passes
}


//=//// UPPERCASE COMMENTARY MACROS (GLOBAL SCOPE VARIANTS) //////////////=//

DEFINITELY(1 == 1);
IMPOSSIBLE(1 == 2);
POSSIBLY(1 == 1);

UNNECESSARY(sizeof(int));
DONT(sizeof(int));
CANT(sizeof(int));


int main() {
    test_possibly();
    test_impossible();
    test_definitely();
    test_bool_constrained_with_pointer();
    test_unnecessary();
    test_dont();
    test_cant();
    test_inapplicable();
    test_heeded();
    test_static_assert();
    test_static_assert_lvalue();
    return 0;
}
