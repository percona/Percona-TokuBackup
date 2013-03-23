# Don't run helgrind and drd when we are running valgrind
foreach(test backup_directory_tests copy_files open_write_close truncate read_and_seek)
    list(APPEND CTEST_CUSTOM_MEMCHECK_IGNORE
      helgrind/${test}
      drd/${test}
      )
  endforeach(test)
