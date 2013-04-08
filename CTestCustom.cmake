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
