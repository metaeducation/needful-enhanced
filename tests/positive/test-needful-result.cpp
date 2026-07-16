// test-needful-result.cpp
// Tests for Result(T): trap/except/assume/rescue error propagation flow,
// and NEEDFUL_DECLARE_RESULT_HOOKS for self-contained testing.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1 and the directory containing
// needful.h in the include path.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define NEEDFUL_CPP_ENHANCED  1
#define NEEDFUL_DECLARE_RESULT_HOOKS 1
#include "needful.h"

#define Result  NeedfulResult
#define fail     needful_make_failure
#define trap     needful_return_if_failed
#define require  needful_abort_if_failed
#define assume   needful_assert_not_failed
#define except   needful_catch_if_failed
#define rescue   needful_extract_failure

#define STATIC_ASSERT  NEEDFUL_STATIC_ASSERT

#include <type_traits>


//=//// RESULT WRAPPER TYPE PROPERTIES ////////////////////////////////////=//

void test_result_wrapper_types() {
    // Result(int) wraps int; the wrapper is distinct from int
    STATIC_ASSERT(not std::is_same<Result(int), int>::value);

    // Result(T) is not implicitly convertible to T (must use trap/assume/etc.)
    STATIC_ASSERT(not needful_is_convertible_v(Result(int), int));

    // Result(T*) not implicitly convertible to T*
    STATIC_ASSERT(not needful_is_convertible_v(Result(int*), int*));
}


//=//// trap: AUTO-PROPAGATE ON FAILURE ///////////////////////////////////=//

Result(int) risky_add(int a, int b) {
    if (a < 0 or b < 0)
        return fail ("negative input");
    return a + b;
}

Result(int) call_risky_trap(int a, int b) {
    trap (int sum = risky_add(a, b));
    return sum * 2;
}

void test_trap_propagates() {
    // Success case: trap extracts value and continues
    {
        Result(int) r = call_risky_trap(3, 4);
        assume (int v = r);
        assert(v == 14);
        assert(not Needful_Get_Failure());
    }

    // Failure case: trap propagates error upward
    {
        Result(int) r = call_risky_trap(-1, 4);
        // r should hold a zeroed state and the failure is in global state
        // but call_risky_trap itself trapped and re-propagated, so:
        const char* e = Needful_Test_And_Clear_Failure();
        assert(e != nullptr);
    }
}


//=//// except: CATCH AND HANDLE ERRORS ///////////////////////////////////=//

Result(int) maybe_fail(bool should_fail) {
    if (should_fail)
        return fail ("deliberate failure");
    return 99;
}

void test_except_success() {
    bool caught = false;
    int result = maybe_fail(false) except (const char* e) {
        NEEDFUL_UNUSED(e);
        caught = true;
    }
    assert(not caught);
    assert(result == 99);
}

void test_except_catches_error() {
    bool caught = false;
    const char* caught_msg = nullptr;

    maybe_fail(true) except (const char* e) {
        caught = true;
        caught_msg = e;
    }
    assert(caught);
    assert(caught_msg != nullptr);
    assert(not Needful_Get_Failure());  // error was cleared by except
}

void test_except_else_clause() {
    bool else_ran = false;
    bool except_ran = false;

    maybe_fail(false) except (const char* e) {
        NEEDFUL_UNUSED(e);
        except_ran = true;
    } else {
        else_ran = true;
    }
    assert(else_ran);
    assert(not except_ran);
}


//=//// assume: ASSERT NO FAILURE (debug check) ///////////////////////////=//

void test_assume_success() {
    assume (int v = risky_add(10, 20));
    assert(v == 30);
}


//=//// rescue: CLEAR AND RETURN FAILURE ///////////////////////////////////=//

void test_rescue_clears_error() {
    // rescue() evaluates the expression, clears any failure, and returns it
    const char* e = rescue (maybe_fail(true));
    assert(e != nullptr);
    assert(not Needful_Get_Failure());  // error was cleared
}

void test_rescue_no_error() {
    const char* e = rescue (maybe_fail(false));
    assert(e == nullptr);
}


//=//// Result(T) WITH POINTER TYPES //////////////////////////////////////=//

Result(const char*) maybe_string(bool fail_it) {
    if (fail_it)
        return fail ("no string");
    return "hello";
}

void test_result_pointer() {
    {
        const char* s = nullptr;
        maybe_string(false) except (const char* e) {
            NEEDFUL_UNUSED(e);
        } else {
            // result needs to be extracted via assume/trap when successful
        }
        assume (s = maybe_string(false));
        assert(s != nullptr);
    }

    {
        const char* e = rescue (maybe_string(true));
        assert(e != nullptr);
    }
}


int main() {
    test_result_wrapper_types();
    test_trap_propagates();
    test_except_success();
    test_except_catches_error();
    test_except_else_clause();
    test_assume_success();
    test_rescue_clears_error();
    test_rescue_no_error();
    test_result_pointer();
    return 0;
}
