# Don't run helgrind and drd when we are running valgrind
foreach(test 
 backup_directory_tests
 backup_no_fractal_tree
 backup_no_fractal_tree_threaded
 backup_no_ft2
 copy_files
 open_write_close
 multiple_backups
 ftruncate
 read_and_seek
 test6128
 test6317
 test6317b
 test6361
)
    list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
      helgrind/${test}
      drd/${test}
      )
  endforeach(test)

set(MEMORYCHECK_COMMAND /usr/bin/valgrind)
set(MEMORYCHECK_SUPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/valgrind.suppressions")
set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --soname-synonyms=somalloc=*tokuportability* --gen-suppressions=no --quiet --num-callers=20 --leak-check=full --show-reachable=yes" CACHE INTERNAL "options for valgrind")
