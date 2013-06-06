
#include "backup_test_helpers.h"
#include "raii-malloc.h"

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    setup_source();
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
    

    return 0;
}
