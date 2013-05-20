find_package( Threads )

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS _GNU_SOURCE DONT_DEPRECATE_ERRNO)

set(all_bins
  db-benchmark-test
  backup1
#  scanscan
#  ptquery
#  txncommit
#  scanrace
#  multi-bench
#  db-verify
  )

include_directories("${PROJECT_SOURCE_DIR}/../tokudb/toku_include" "${PROJECT_SOURCE_DIR}/../tokudb/release/include" "${PROJECT_SOURCE_DIR}/../tokudb/portability" "${PROJECT_SOURCE_DIR}/../backup")
find_library(LIBTOKUPORTABILITY NAMES tokuportability PATHS "${PROJECT_SOURCE_DIR}/../tokudb/release/lib/")
find_library(LIBTOKUDB          NAMES tokudb          PATHS "${PROJECT_SOURCE_DIR}/../tokudb/release/lib/")
foreach(tokudb_bin ${all_bins})
  add_executable(${tokudb_bin} ${tokudb_bin})
  target_link_libraries(${tokudb_bin} ${LIBTOKUDB} ${LIBTOKUPORTABILITY} HotBackup ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  add_test(${tokudb_bin} ${tokudb_bin} 1)
endforeach(tokudb_bin)
