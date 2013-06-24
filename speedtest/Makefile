CFLAGS=-O3 -W -Wall -Werror -g -std=c99
TESTS = write pwrite
TARGETS = $(patsubst %,speed_%_plain,$(TESTS)) $(patsubst %,speed_%_hb,$(TESTS))
default: $(TARGETS)

%_plain: %.o
	$(CXX) $(CFLAGS) -pthread $< -o $@
%_hb: %.o
	$(CXX) $(CFLAGS) -pthread $< ../backup/Debug.gcc/libHotBackupGlassbox.a -ldl -lrt -o $@

clean:
	rm -rf $(TARGETS) $(patsubst %,speed_%.o,$(TESTS))
