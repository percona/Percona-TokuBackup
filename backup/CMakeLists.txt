cmake_minimum_required(VERSION 2.8.8)

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

project(HotBackup)


# pick language dialect
check_cxx_compiler_flag(-std=c++11 HAVE_STDCXX11)
if (HAVE_STDCXX11)
  set(CMAKE_CXX_FLAGS "-std=c++11 -Wno-deprecated-declarations ${CMAKE_CXX_FLAGS}")
else ()
  message(FATAL_ERROR "${CMAKE_CXX_COMPILER} doesn't support -std=c++11, you need one that does.")
endif ()

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS 
  _FILE_OFFSET_BITS=64 
  _LARGEFILE64_SOURCE)

set(CMAKE_CXX_FLAGS "-Werror -W -Wall -Wshadow ${CMAKE_CXX_FLAGS}")

set(USE_VALGRIND OFF CACHE BOOL "whether to use valgrind headers")
if (USE_VALGRIND)
  set_property(DIRECTORY APPEND PROPERTY
    COMPILE_DEFINITIONS BACKUP_USE_VALGRIND=1)
endif ()

set(BACKUP_SOURCES
	backup_debug.cc
	backup_directory.cc
        check.cc
	copier.cc
	description.cc
	destination_file.cc
	dirsum.cc
	directory_set.cc
	file_hash_table.cc
	fmap.cc
	manager.cc
	manager_state.cc
	mutex.cc
	real_syscalls.cc
	rwlock.cc
	source_file.cc
	backup.cc
	backup_callbacks.cc
        MurmurHash3.cc
)

set(HOT_BACKUP_LIBNAME HotBackup
  CACHE STRING "installed name of the hot backup library")

add_library(${HOT_BACKUP_LIBNAME}          SHARED ${BACKUP_SOURCES})

find_program(COVERAGE_COMMAND NAMES gcov47 gcov)
option(USE_GCOV "Use gcov for test coverage." OFF)
if (USE_GCOV)
  # Put the exe linker flags *before* including the test subdir.
  set(CMAKE_EXE_LINKER_FLAGS "--coverage ${CMAKE_EXE_LINKER_FLAGS}")
endif (USE_GCOV)

IF(NOT APPLE)
  target_link_libraries(${HOT_BACKUP_LIBNAME}          LINK_PUBLIC ${CMAKE_DL_LIBS} rt)
ENDIF()

# If for some reason we go back to static libraries, we'll need these two:
# set_target_properties(${HOT_BACKUP_LIBNAME}          PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(HotBackupGlassbox  PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(add_space_separated_property type obj propname val)
  get_property(oldval ${type} ${obj} PROPERTY ${propname})
  if (oldval MATCHES NOTFOUND)
    set_property(${type} ${obj} PROPERTY ${propname} "${val}")
  else ()
    set_property(${type} ${obj} PROPERTY ${propname} "${val} ${oldval}")
  endif ()
endfunction(add_space_separated_property)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -UNDEBUG")
if (NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif ()

## add a version script and set -fvisibility=hidden for the shared library.
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  # Don't need this for the community version, since there's nothing in there except the two functions.
  # And don't need this for glassbox version, since we want everything visible, so we use visibility=default instead of the usual visibility=hidden.
  add_space_separated_property(TARGET ${HOT_BACKUP_LIBNAME} COMPILE_FLAGS "-fvisibility=default -fvisibility-inlines-hidden")
  add_space_separated_property(TARGET ${HOT_BACKUP_LIBNAME} LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map")

  #add_space_separated_property(TARGET ${HOT_BACKUP_LIBNAME} COMPILE_FLAGS "-fvisibility=default -fvisibility-inlines-hidden -flto")
  #add_space_separated_property(TARGET ${HOT_BACKUP_LIBNAME} LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map -flto -s")
endif ()
add_space_separated_property(TARGET ${HOT_BACKUP_LIBNAME} LINK_FLAGS "${OPTFLAGS}")

configure_file(CTestCustom.cmake . COPYONLY)

# INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/../tokudb

##add_subdirectory(db-benchmark-test)

IF (NOT DEFINED INSTALL_LIBDIR)
  SET(INSTALL_LIBDIR lib)

  include(CTest)
  add_subdirectory(tests)

  # And a Glassbox version of the library in which the visibility is defaul, not hidden
  add_library(HotBackupGlassbox      SHARED ${BACKUP_SOURCES})
  IF(NOT APPLE)
    target_link_libraries(HotBackupGlassbox LINK_PUBLIC ${CMAKE_DL_LIBS} rt)
  ENDIF()

  set_property(TARGET HotBackupGlassbox APPEND PROPERTY COMPILE_DEFINITIONS
    GLASSBOX)
ENDIF()
install(TARGETS ${HOT_BACKUP_LIBNAME}          DESTINATION ${INSTALL_LIBDIR} 
    COMPONENT tokubackup_libs_shared)
install(FILES backup.h DESTINATION include
    COMPONENT tokubackup_headers)

if (TOKUMX_ENTERPRISE_CREATE_EXPORTS)
  file(RELATIVE_PATH _relative_source_dir "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  install(TARGETS ${HOT_BACKUP_LIBNAME} DESTINATION "${_relative_source_dir}" COMPONENT tokumx_enterprise_exports EXPORT tokumx_enterprise_exports)
  install(FILES backup.h DESTINATION "${_relative_source_dir}" COMPONENT tokumx_enterprise_exports)
  exports_install(${HOT_BACKUP_LIBNAME} "\${INSTALL_LIBDIR}" tokubackup_libs_shared)
  unset(_relative_source_dir)
endif ()

option(BACKUP_HAS_PARENT "Don't set this by hand.  It's used to tell the subcmake not to set things" ON)
if (BACKUP_HAS_PARENT)
  set(HOT_BACKUP_LIBS ${HOT_BACKUP_LIBNAME} PARENT_SCOPE)
endif (BACKUP_HAS_PARENT)

option(USE_CTAGS "Build the ctags database." ON)
file(GLOB_RECURSE all_srcs *.cc tests/*.cc)
file(GLOB_RECURSE all_hdrs *.h  tests/*.h)
if (USE_CTAGS AND
    # Macs by default are not case-sensitive, so tags and TAGS clobber each other.  Do etags and not ctags in that case, because Emacs is superior. :P
    (NOT APPLE OR NOT USE_ETAGS))
  find_program(CTAGS "ctags")
  if (NOT CTAGS MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/tags"
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      COMMAND ${CTAGS} -o tags ${all_srcs} ${all_hdrs}
      COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/ctags-stamp"
      DEPENDS ${all_srcs} ${all_hdrs}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_backup_ctags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tags" ctags-stamp)
  endif ()
endif ()

option(USE_ETAGS "Build the etags database." ON)
if (USE_ETAGS)
  find_program(ETAGS "etags")
  if (NOT ETAGS MATCHES NOTFOUND)
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/TAGS"
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
      COMMAND ${ETAGS} -o TAGS ${all_srcs} ${all_hdrs}
      COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/etags-stamp"
      DEPENDS ${all_srcs} ${all_hdrs}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_target(build_backup_etags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TAGS" etags-stamp)
  endif ()
endif ()

# Put the compiler gcov flags *after* including the test subdir, so that we don't measure the coverage of the test code.
if (USE_GCOV)
  set(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS   "--coverage ${CMAKE_C_FLAGS}")
endif (USE_GCOV)
