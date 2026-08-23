/*
**  file: %test-corrupt-assert-fires.c
**  summary: "Assert_Corrupted_If_Needful() must fire on a clean variable"
**
**=/////////////////////////////////////////////////////////////////////////=//
**
** This is a "must abort" test: CTest runs it with WILL_FAIL, so a zero exit
** status is a FAILURE.
**
** It exists because the failure direction of an assertion cannot be checked
** from inside a process that is supposed to finish normally.  The passing
** direction (corrupt the variable, then assert it is corrupt) is covered by
** %test-needful-c-sweep.c.
**
** The bug it guards against was real: the assertion was written as
**
**     NEEDFUL_ASSERT("Expected variable to be corrupt and it was not");
**
** ...which passes a string literal--always a non-null pointer, so always
** true.  The check detected the condition correctly and then reported
** success.  It needed the `!` that the sibling needful_unreachable macros
** have.  With NEEDFUL_DOES_CORRUPTIONS off (the default) the macro is a
** no-op, which is why no other test could have noticed.
**
**=//// NOTES /////////////////////////////////////////////////////////////=//
**
** A. A test that is *supposed* to abort needs the platform talked out of
**    being helpful about it.  On Windows, a failing assert() opens a modal
**    "Debug Assertion Failed" box and abort() opens a second one offering to
**    report the crash--both of which wait forever for a click that CI will
**    never give.  (This was not hypothetical: the first version of this file
**    hung a local test run for seven minutes at 0% CPU.)
**
**    assert() itself already writes to stderr here; it is abort() that stops
**    to ask.  Clearing its behavior flags with _set_abort_behavior() makes the
**    process die immediately with a non-zero status, which is what WILL_FAIL
**    wants.  The call is MSVC-specific; POSIX abort() is already silent.
**
**    The CTest registration also carries a TIMEOUT, so that if a dialog ever
**    does get through on some other configuration, the test fails in thirty
**    seconds instead of wedging the run.
*/

#define NEEDFUL_DOES_CORRUPTIONS  1
#include "needful.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int x = 5;  /* deliberately NOT run through Corrupt_If_Needful() */

  #if defined(_MSC_VER)  /* see [A] */
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  #endif

    printf("asserting that a clean variable is corrupt...\n");
    fflush(stdout);  /* flush before the abort, so the log shows we got here */

    Assert_Corrupted_If_Needful(x);

    printf("REGRESSION: assertion did not fire on a non-corrupt variable\n");
    return 0;  /* WILL_FAIL means CTest treats reaching here as a failure */
}
