#ident "$Id: check.h 56070 2013-05-09 15:42:27Z bkuszmaul $"

#include <stdlib.h>
#include <stdio.h>
#include "check.h"

void check_fun(long predicate, const char *expr, const char *file, int line) throw() {
    if (!predicate) {
        fprintf(stderr, "check(%s) failed at %s:%d\n", expr, file, line);
        abort();
    }
}
