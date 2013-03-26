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
	backup_callbacks.cc
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
# And a Glassbox version of the library in which the visibility is defaul, not hidden
add_library(HotBackupGlassbox      SHARED ${BACKUP_SOURCES})


target_link_libraries(HotBackup          LINK_PUBLIC rt)
target_link_libraries(HotBackupCommunity LINK_PUBLIC rt)
target_link_libraries(HotBackupGlassbox LINK_PUBLIC rt)

# If for some reason we go back to static libraries, we'll need these two:
# set_target_properties(HotBackup          PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(HotBackupCommunity PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(HotBackupGlassbox  PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(add_space_separated_property type obj propname val)
  get_property(oldval ${type} ${obj} PROPERTY ${propname})
  if (oldval MATCHES NOTFOUND)
    set_property(${type} ${obj} PROPERTY ${propname} "${val}")
  else ()
    set_property(${type} ${obj} PROPERTY ${propname} "${val} ${oldval}")
  endif ()
endfunction(add_space_separated_property)

## add a version script and set -fvisibility=hidden for the shared library.
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  # Don't need this for the community version, since there's nothing in there except the two functions.
  # And don't need this for glassbox version, since we want everything visible, so we use visibility=default instead of the usual visibility=hidden.
  add_space_separated_property(TARGET HotBackup COMPILE_FLAGS "-fvisibility=default -fvisibility-inlines-hidden")
  add_space_separated_property(TARGET HotBackup LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map")
endif ()

add_subdirectory(tests)

install(TARGETS HotBackup          DESTINATION lib)
install(TARGETS HotBackupCommunity DESTINATION lib)
install(TARGETS HotBackupGlassbox  DESTINATION lib)
