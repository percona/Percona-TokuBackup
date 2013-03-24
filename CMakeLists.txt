cmake_minimum_required(VERSION 2.8.8)
project(HotBackup)

include(CTest)
##set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_FLAGS   "-Wall -Werror -Wextra -std=c99   ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wextra -std=c++11 ${CMAKE_CXX_FLAGS}")

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

## adds a compiler flag if the compiler supports it
macro(set_cflags_if_supported_named flag flagname)
  check_c_compiler_flag("${flag}" HAVE_C_${flagname})
  if (HAVE_C_${flagname})
    set(CMAKE_C_FLAGS "${flag} ${CMAKE_C_FLAGS}")
  endif ()
  check_cxx_compiler_flag("${flag}" HAVE_CXX_${flagname})
  if (HAVE_CXX_${flagname})
    set(CMAKE_CXX_FLAGS "${flag} ${CMAKE_CXX_FLAGS}")
  endif ()
endmacro(set_cflags_if_supported_named)

set_cflags_if_supported_named(-Wmissing-declarations MISSING_DECLARATIONS)

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
# message(${BACKUP_SOURCES})
set(BACKUP_COMMUNITY_SOURCES
        backup_community.cc)

# Two libraries.  One is the actual backup, one is for the community edition, and doesn't do anything.
add_library(HotBackup          SHARED ${BACKUP_SOURCES})
add_library(HotBackupCommunity SHARED ${BACKUP_COMMUNITY_SOURCES})

# If for some reason we go back to static libraries, we'll need this.
# set_target_properties(HotBackup          PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(HotBackupCommunity PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(tests)

install(TARGETS HotBackup DESTINATION lib)
install(TARGETS HotBackupCommunity DESTINATION lib)
