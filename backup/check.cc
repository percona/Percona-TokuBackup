#ident "$Id$"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "check.h"

void check_fun(long predicate, const char *expr, const backtrace bt) throw() {
    if (!predicate) {
        int e = errno;
        fprintf(stderr, "check(%s) failed\n", expr);
        fprintf(stderr, "errno=%d\n", e);
        const backtrace *btp=&bt;
        fprintf(stderr, "backtrace:\n");
        while (btp) {
            fprintf(stderr, " %s:%d (%s)\n", btp->file, btp->line, btp->fun);
            btp = btp->prev;
        }
        abort();
    }
}
