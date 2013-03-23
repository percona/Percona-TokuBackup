set(MEMORYCHECK_COMMAND /usr/bin/valgrind)
set(MEMORYCHECK_COMMAND_OPTIONS "--gen-suppressions=no --quiet --num-callers=20 --leak-check=full --show-reachable=yes" CACHE INTERNAL "options for valgrind")
