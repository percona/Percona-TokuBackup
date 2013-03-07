cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
find_package( Threads )
set(INTEGRATION_TEST_SOURCES
        backup_integration_tests_main.cc
        backup_test_helpers.cc
        backup_directory_tests.cc
        file_descriptor_map_tests.cc
        copy_files.cc
        open_write_close.cc
        read_and_seek.cc
        truncate.cc)
add_executable(IntegrationTests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(IntegrationTests ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} HotBackup)
