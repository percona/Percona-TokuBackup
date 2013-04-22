cmake_minimum_required(VERSION 2.8.8)
project(HotBackup)

# No implicit templates, since that's how mysql compiles.
set(CMAKE_CXX_FLAGS "-fno-implicit-templates  ${CMAKE_CXX_FLAGS}")

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS 
  _FILE_OFFSET_BITS=64 
  _LARGEFILE64_SOURCE)

set(CMAKE_CXX_FLAGS "-Werror -W -Wall -Wshadow ${CMAKE_CXX_FLAGS}")

set(BACKUP_SOURCES
	backup_debug.cc
	backup_directory.cc
	copier.cc
	description.cc
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

add_library(HotBackup          SHARED ${BACKUP_SOURCES})

IF(NOT APPLE)
  target_link_libraries(HotBackup          LINK_PUBLIC rt)
ENDIF()

# If for some reason we go back to static libraries, we'll need these two:
# set_target_properties(HotBackup          PROPERTIES POSITION_INDEPENDENT_CODE ON)
# set_target_properties(HotBackupGlassbox  PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(add_space_separated_property type obj propname val)
  get_property(oldval ${type} ${obj} PROPERTY ${propname})
  if (oldval MATCHES NOTFOUND)
    set_property(${type} ${obj} PROPERTY ${propname} "${val}")
  else ()
    set_property(${type} ${obj} PROPERTY ${propname} "${val} ${oldval}")
  endif ()
endfunction(add_space_separated_property)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -s")

## add a version script and set -fvisibility=hidden for the shared library.
configure_file(export.map . COPYONLY)
if (NOT CMAKE_SYSTEM_NAME STREQUAL Darwin)
  # Don't need this for the community version, since there's nothing in there except the two functions.
  # And don't need this for glassbox version, since we want everything visible, so we use visibility=default instead of the usual visibility=hidden.
  add_space_separated_property(TARGET HotBackup COMPILE_FLAGS "-fvisibility=default -fvisibility-inlines-hidden")
  add_space_separated_property(TARGET HotBackup LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map")

  #add_space_separated_property(TARGET HotBackup COMPILE_FLAGS "-fvisibility=default -fvisibility-inlines-hidden -flto")
  #add_space_separated_property(TARGET HotBackup LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/export.map -flto -s")
endif ()
add_space_separated_property(TARGET HotBackup LINK_FLAGS "${OPTFLAGS}")

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
    target_link_libraries(HotBackupGlassbox LINK_PUBLIC rt)
  ENDIF()

  set_property(TARGET HotBackupGlassbox APPEND PROPERTY COMPILE_DEFINITIONS
    GLASSBOX)
ENDIF()
install(TARGETS HotBackup          DESTINATION ${INSTALL_LIBDIR} COMPONENT Server)

set(HOT_BACKUP_LIBS HotBackup PARENT_SCOPE)

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
    add_custom_target(build_ctags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tags" ctags-stamp)
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
    add_custom_target(build_etags ALL DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TAGS" etags-stamp)
  endif ()
endif ()
