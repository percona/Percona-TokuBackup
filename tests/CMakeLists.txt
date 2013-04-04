cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
find_package( Threads )

function(add_valgrind_tool_test tool test)
  add_test(
      NAME
        ${tool}/${test}
      COMMAND
        valgrind --tool=${tool} --error-exitcode=1 $<TARGET_FILE:${test}> --testname ${test}.${tool}
      )
endfunction(add_valgrind_tool_test)

set(blackboxtests
  multiple_backups
  open_write_close
  read_and_seek
  test6128
  test6317
  test6317b
  test6361
  )

set(glassboxtests
  backup_directory_tests
  backup_no_fractal_tree          ## Needs the keep_capturing API
  backup_no_fractal_tree_threaded ## Needs the keep_capturing API
  backup_no_ft2                   ## Needs the keep_capturing API
  ftruncate                       ## Needs the keep_capturing API
  copy_files
  test6415_enospc_injection
  test6469_many_enospc_injection
  test6431_postcopy
  write_race
  )

foreach(test ${blackboxtests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackup ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
  add_valgrind_tool_test(helgrind ${test})
  add_valgrind_tool_test(drd ${test})
endforeach(test)

foreach(test ${glassboxtests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackupGlassbox ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test} --testname ${test})
  add_valgrind_tool_test(helgrind ${test})
  add_valgrind_tool_test(drd ${test})
endforeach(test)
