cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
find_package( Threads )

function(add_valgrind_tool_test tool test)
  add_test(
      NAME
        ${tool}/${test}
      COMMAND
        valgrind --tool=${tool} --error-exitcode=1 $<TARGET_FILE:${test}>
      )
endfunction(add_valgrind_tool_test)

set(tests
  backup_directory_tests
  copy_files
  open_write_close
  truncate
  read_and_seek
  test6317
  test6317b
  )

foreach(test ${tests})
  add_executable(${test} ${test} backup_test_helpers)
  target_link_libraries(${test} HotBackup ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${test} ${test})
  add_valgrind_tool_test(helgrind ${test})
  add_valgrind_tool_test(drd ${test})
endforeach(test)
