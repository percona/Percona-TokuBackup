#include "backup_debug.h"
#include "backup.h"
// Call all the trace and warn code (for coverage)
int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    HotBackup::CopyTrace("hello", "there");
    HotBackup::CopyWarn("hello", "there");
    HotBackup::CopyError("hello", "there");

    HotBackup::CaptureTrace("hello", "there");
    HotBackup::CaptureTrace("hello", 3);
    HotBackup::CaptureWarn("hello", "there");
    HotBackup::CaptureError("hello", "there");
    HotBackup::CaptureError("hello", 3);

    HotBackup::InterposeTrace("hello", "there");
    HotBackup::InterposeTrace("hello", 3);
    HotBackup::InterposeWarn("hello", "there");
    HotBackup::InterposeError("hello", "there");

    HotBackup::should_pause(HotBackup::MANAGER_IN_PREPARE);
    HotBackup::should_pause(HotBackup::MANAGER_IN_DISABLE);
    HotBackup::should_pause(-1);

    return 0;
}
