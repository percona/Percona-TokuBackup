set(MEMORYCHECK_COMMAND /usr/bin/valgrind)
set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --soname-synonyms=somalloc=*tokuportability* --suppressions=${PROJECT_SOURCE_DIR}/valgrind.suppressions --gen-suppressions=no --quiet --num-callers=20 --leak-check=full --show-reachable=yes" CACHE INTERNAL "options for valgrind")
