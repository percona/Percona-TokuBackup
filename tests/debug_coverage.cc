#include "backup_debug.h"
// Call all the trace and warn code (for coverage)
int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    HotBackup::CopyTrace("hello", "there");
    return 0;
}
