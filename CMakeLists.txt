cmake_minimum_required(VERSION 2.8.8)
project(HotBackup)

include(CTest)
##set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_FLAGS "-Wall -Werror -std=c99 ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -Werror -std=c++11 ${CMAKE_CXX_FLAGS}")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS 
  _FILE_OFFSET_BITS=64 
  _LARGEFILE64_SOURCE)

set(BACKUP_SOURCES
	backup.cc
	backup_copier.cc
	backup_debug.cc
	backup_directory.cc
	backup_manager.cc
	file_description.cc
	file_descriptor_map.cc
	real_syscalls.cc)
message(${BACKUP_SOURCES})

set(CMAKE_C_FLAGS "-Wall -Werror ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wmissing-declarations ${CMAKE_CXX_FLAGS}")

add_library(HotBackup STATIC ${BACKUP_SOURCES})
set_target_properties(HotBackup PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_subdirectory(tests)

## Do we need this?
install(TARGETS HotBackup DESTINATION lib)
