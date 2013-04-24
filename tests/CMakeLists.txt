cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
find_package( Threads )

option(USE_GRIND "Use helgrind and valgrind." ON)

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
  empty_dest
  multiple_backups
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
  ftruncate                       ## Needs the keep_capturing API
  ftruncate_injection_6480
  copy_files
  disable_race
  range_locks
  test6415_enospc_injection
  test6431_postcopy
  test6469_many_enospc_injection
  test6477_close_injection
  test6478_read_injection
  test6483_mkdir_injection
  open_injection_6476
  rename
  rename_injection
  unlink
  unlink_during_copy_test6515c
  unlink_injection
  write_race
  )

foreach(test ${blackboxtests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackup ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
  if (USE_GRIND)
    add_valgrind_tool_test(helgrind ${test})
    add_valgrind_tool_test(drd ${test})
  endif (USE_GRIND)
endforeach(test)

foreach(test ${glassboxtests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackupGlassbox ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
  if (USE_GRIND)
    add_valgrind_tool_test(helgrind ${test})
    add_valgrind_tool_test(drd ${test})
  endif (USE_GRIND)
endforeach(test)
