backup.pb.o: backup.pb.h
CXXFLAGS = -W -Wall -Werror -O1
backup.pb.cc backup.bp.h: backup.proto
	protoc -I=. --cpp_out=. backup.proto
