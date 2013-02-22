cmake_minimum_required(VERSION 2.8.8)
project(BackupUnitTests)
include_directories(..)
find_package( Threads )



add_executable(file_descriptor_map file_descriptor_map.cc)
add_executable(file_description file_description.cc)
add_executable(backup_directory backup_directory.cc)

target_link_libraries(file_descriptor_map ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} Backup)
target_link_libraries(file_description ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} Backup)
target_link_libraries(backup_directory ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} Backup)
