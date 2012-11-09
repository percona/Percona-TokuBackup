cmake_minimum_required(VERSION 2.8.8)
project(HotBackup)
set(BACKUP_SOURCES
	backup.cc
	backup_copier.cc
	backup_manager.cc
	backup_directory.cc
	file_description.cc
	file_descriptor_map.cc
	real_syscalls.cc)
message(${BACKUP_SOURCES})
###add_library(Backup STATIC backup.cc backup_manager.cc backup_file.cc real_sy###scalls.cc backup_directory.cc)
add_library(Backup STATIC ${BACKUP_SOURCES})
add_subdirectory(tests)
