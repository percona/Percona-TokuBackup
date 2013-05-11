#include "check.h"
#include <signal.h>

// To see if check(0) aborts, install a signal handler that does exit(0).

void handler(int i __attribute__((unused))) {
    exit(0);
}

int test_main (int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    signal(SIGABRT, handler);
    check(0);
    return 1;
}

