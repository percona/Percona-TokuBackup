CFLAGS   = -W -Wall -Werror -O2 -g
CXXFLAGS = $(CFLAGS)
default: check_linkage linkit.well linkit.badly
check_linkage:

linkit.well:  LDFLAGS = -fPIC -lHotBackup -lpthread -L. -Wl,-rpath,.
linkit.badly: LDFLAGS = -fPIC -lpthread -lHotBackup -L. -Wl,-rpath,.
linkit.well: linkit.c libHotBackup.so
	$(CC) $(LDFLAGS) $(CFLAGS) $< -o $@ 
linkit.badly: linkit.c  libHotBackup.so
	$(CC) $(LDFLAGS) $(CFLAGS) $< -o $@

# A simple library
libHotBackup.so: interpose.c
	$(CC) -shared $(CXXFLAGS) $< -o $@

check: check.well check.badly
check.well: check_linkage linkit.well
	./check_linkage linkit.well
check.badly: check_linkage linkit.badly
	./check_linkage linkit.badly 2> /dev/null; test $$?==1

clean:
	rm *.so linkit.well linkit.badly check_linkage
