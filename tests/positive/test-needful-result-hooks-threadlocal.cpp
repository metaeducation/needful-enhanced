// test-needful-result-hooks-threadlocal.cpp
// Checks that the needful-result-hooks-threadlocal.h/.c pair (linked in by
// CMake) gives each thread its own pending failure.
//
// Build: needs NEEDFUL_CPP_ENHANCED=1, the directory containing needful.h in
// the include path, and a threads library.
//

#include <assert.h>
#include <string.h>
#include <thread>

#define NEEDFUL_CPP_ENHANCED  1
#include "needful.h"
#include "needful-result-hooks-threadlocal.h"

static NeedfulResult(int) fail_if_negative(int x) {
    if (x < 0)
        needful_return_fail ("negative input");
    return x;
}

int main() {
    Needful_Set_Failure("main thread's failure");  // left pending on main

    bool other_saw_nothing = false;
    bool other_got_its_own = false;

    std::thread other([&] {
        other_saw_nothing = (Needful_Get_Failure() == nullptr);

        // extract_failure asserts nothing is pending first, so this would
        // also trip if main's failure leaked into this thread
        //
        const char* e = needful_extract_failure(fail_if_negative(-1));
        other_got_its_own = (e != nullptr and 0 == strcmp(e, "negative input"));

        assert(Needful_Get_Failure() == nullptr);  // cleared by extraction
    });
    other.join();

    assert(other_saw_nothing);
    assert(other_got_its_own);

    const char* mine = Needful_Test_And_Clear_Failure();  // still intact
    assert(mine != nullptr and 0 == strcmp(mine, "main thread's failure"));
    assert(Needful_Get_Failure() == nullptr);

    return 0;
}
