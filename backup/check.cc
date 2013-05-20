#ident "$Id$"

#include <stdlib.h>
#include <stdio.h>
#include "check.h"

void check_fun(long predicate, const char *expr, const char *file, int line) throw() {
    if (!predicate) {
        fprintf(stderr, "check(%s) failed at %s:%d\n", expr, file, line);
        abort();
    }
}
