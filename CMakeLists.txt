cmake_minimum_required(VERSION 2.8.8)
project(HotBackup)
set(BACKUP_SOURCES
	backup.cc
	backup_copier.cc
	backup_manager.cc
	real_syscalls.cc
	backup_directory.cc)
message(${BACKUP_SOURCES})
###add_library(Backup STATIC backup.cc backup_manager.cc backup_file.cc real_sy###scalls.cc backup_directory.cc)
add_library(Backup STATIC ${BACKUP_SOURCES})
add_subdirectory(tests)
