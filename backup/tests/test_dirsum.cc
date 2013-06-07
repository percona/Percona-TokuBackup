
#include "backup_test_helpers.h"
#include "raii-malloc.h"

static int my_poll(float progress, const char *progress_string, void *poll_extra) {
    check(poll_extra==NULL);
    fprintf(stderr, "progress %2.f%%: %s\n", progress*100, progress_string);
    return 0;
}

static void simple_error(int errnum, const char *error_string, void *error_extra) {
    check(error_extra==NULL);
    fprintf(stderr, "backup error: errnum=%d (%s)\n", errnum, error_string);
    abort();
}

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    setup_source();
    setup_destination();
    with_object_to_free<char *> src(get_src());
    {
        long long v = dirsum(src.value);
        //printf("dirsum empty dir is %lld\n", v);
        check(v==0);
    }

    systemf("echo hello > %s/data", src.value);
    {
        long long v = dirsum(src.value);
        //printf("dirsum 1dir is %lld\n", v);
        check(v==6);
    }
    systemf("mkdir %s/dir;echo foo > %s/dir/data", src.value, src.value);
    {
        long long v = dirsum(src.value);
        //printf("dirsum 2dir is %lld\n", v);
        check(v==10);
    }
    pthread_t thread;
    start_backup_thread_with_funs(&thread, get_src(), get_dst(), my_poll, NULL, simple_error, NULL, 0);
    finish_backup_thread(thread);

    return 0;
}
