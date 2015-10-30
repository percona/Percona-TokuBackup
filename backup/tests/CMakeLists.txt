cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
find_package( Threads )

if (USE_VALGRIND)
  option(USE_GRIND "Use helgrind and valgrind." ON)
endif ()

function(add_valgrind_tool_test tool test)
  add_test(
      NAME
        ${tool}/${test}
      COMMAND
        valgrind --tool=${tool} --suppressions=${PROJECT_SOURCE_DIR}/../${tool}.suppressions --error-exitcode=1 $<TARGET_FILE:${test}> --testname ${test}.${tool}
      )
endfunction(add_valgrind_tool_test)

set(blackboxtests
  cannotopen_dest_dir
  closedirfails_dest_dir
  dest_no_permissions_10
  dest_no_permissions_with_open_10
  empty_dest
  multiple_backups
  open_close_6731
  open_write_close
  open_prepare_race_6610
  read_and_seek
  test6128
  no_dest_dir_6317b
  notinsource_6570
  notinsource_6570b
  null_dest_dir_6317
  nondir_dest_dir_6317
  readdirfails_dest_dir
  open_write_race
  source_no_permissions_10
  test6361
  throttle_6564
  unlink_during_copy_test6515
  unlink_during_copy_test6515b
  )

set(glassboxtests
  backup_directory_tests
  backup_no_fractal_tree          ## Needs the keep_capturing API
  backup_no_fractal_tree_threaded ## Needs the keep_capturing API
  backup_no_ft2                   ## Needs the keep_capturing API
  capture_only_rename
  check_check
  check_check2
  create_rename_race
  create_unlink_race
  debug_coverage
  exclude_all_files
  failed_rename_kills_backup_6703 ## Needs the keep_capturing API
  failed_unlink_kills_backup_6704 ## Needs the keep_capturing API
  ftruncate                       ## Needs the keep_capturing API
  ftruncate_injection_6480
  copy_files
  test_dirsum
  disable_race
  end_race_open_6668
  end_race_rename_6668
  end_race_rename_6668b
  many_directories
  range_locks
  realpath_error_injection
  test6415_enospc_injection
  test6431_postcopy
  test6469_many_enospc_injection
  test6477_close_injection
  test6478_read_injection
  test6483_mkdir_injection
  two_renames_race
  open_injection_6476
  rename
  rename_injection
  unlink
##  unlink_create_close_race_6727 ## This test fails because of a known and immaterial issue between rename and close.
  unlink_copy_race
  unlink_during_copy_test6515c
  unlink_injection
  write_race
  )

set(glassboxtests_no_grind
  abort_while_holding_lock        ## needs to register a rename function.  This function won't work well under helgrind or memcheck since it exits out of a signal handler.
  )

foreach(test ${blackboxtests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackup ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
endforeach(test)

foreach(test ${glassboxtests} ${glassboxtests_no_grind})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackupGlassbox ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
endforeach(test)

foreach(test ${blackboxtests} ${glassboxtests})
  if (USE_GRIND)
    add_valgrind_tool_test(helgrind ${test})
    add_valgrind_tool_test(drd ${test})
  endif (USE_GRIND)
endforeach(test)  
