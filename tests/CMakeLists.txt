cmake_minimum_required(VERSION 2.8.8)
project(BackupTests)
include_directories(..)
set(BACKUP_TEST_SOURCES
	backup_test_helpers.cc
	backup_test_main.cc
	copy_files.cc
	open_write_close.cc
	read_and_seek.cc)
find_package( Threads )
add_executable(IntegrationTests ${BACKUP_TEST_SOURCES})
target_link_libraries(IntegrationTests ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} HotBackup)
