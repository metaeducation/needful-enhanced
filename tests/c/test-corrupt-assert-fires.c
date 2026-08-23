/*
**  file: %test-corrupt-assert-fires.c
**  summary: "Assert_Corrupted_If_Needful() must fire on a clean variable"
**
**=/////////////////////////////////////////////////////////////////////////=//
**
** This is a "must abort" test, and it reports that by catching its own SIGABRT
** and exiting 0.  So it is an ordinary test: exit 0 passes, anything else
** fails.
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
** A. CTest's WILL_FAIL is NOT the right tool here, though it looks like it.
**    It inverts a normal non-zero *exit code*; it does not cover abnormal
**    termination.  CMake says so directly: "system-level test failures such as
**    segmentation faults or heap errors will still fail the test even if
**    WILL_FAIL is true."  A POSIX abort() raises SIGABRT, so CTest records
**    "Subprocess aborted" and reports a failure no matter what WILL_FAIL says.
**
**    (This is a good trap.  A first version using WILL_FAIL passed on Windows
**    and failed on Linux, because _set_abort_behavior() below turns Windows
**    abort() into a plain exit-code-3 return, which WILL_FAIL *can* invert.)
**
**    So the test reports success itself: a SIGABRT handler exits 0, and
**    reaching the end of main() -- meaning the assertion did not fire --
**    returns non-zero.  No WILL_FAIL, and identical behavior everywhere.
**
** B. A test that is *supposed* to abort also needs the platform talked out of
**    being helpful about it.  On Windows, abort() opens a modal box offering
**    to report the crash, and waits forever for a click CI will never give.
**    (Not hypothetical: an early version hung a local run for seven minutes at
**    0% CPU.)  The SIGABRT handler now runs first and exits, but clearing the
**    behavior flags stays as insurance, and the CTest registration carries a
**    TIMEOUT so a dialog that does get through fails in thirty seconds rather
**    than wedging the run.
**
** C. NDEBUG is undefined below because this file *is* the assertion under
**    test.  Compiled with NDEBUG, assert() vanishes, the check silently could
**    not fire, and the test would report the very regression it guards against.
*/

#undef NDEBUG  /* must precede needful.h, which includes <assert.h> [C] */

#define NEEDFUL_DOES_CORRUPTIONS  1
#include "needful.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static void abort_means_success(int sig) {
    (void)sig;
    _Exit(0);  /* async-signal-safe, and skips atexit/stream flushing [A] */
}

int main(void) {
    int x = 5;  /* deliberately NOT run through Corrupt_If_Needful() */

    signal(SIGABRT, abort_means_success);  /* [A] */

  #if defined(_MSC_VER)  /* insurance against a modal dialog [B] */
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  #endif

    printf("asserting that a clean variable is corrupt...\n");
    fflush(stdout);  /* flush before the abort, so the log shows we got here */

    Assert_Corrupted_If_Needful(x);

    printf("REGRESSION: assertion did not fire on a non-corrupt variable\n");
    return 1;  /* reaching here at all is the failure this test exists for */
}
