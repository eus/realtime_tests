.PHONY := check check_sudo clean mrproper
.DEFAULT_GOAL := check

override CFLAGS := -O3 -Wall $(CFLAGS)

test_cases_header_only := utility_time_test utility_log_test
test_cases_with_source := utility_file_test
test_cases := $(test_cases_header_only) $(test_cases_with_source)

test_cases_sudo_header_only :=
test_cases_sudo_with_source := utility_cpu_test
test_cases_sudo := $(test_cases_sudo_header_only) $(test_cases_sudo_with_source)

# Requirements of individual test cases
$(test_cases_with_source) $(test_cases_sudo_with_source): %_test: %.o

test_cases_requiring_pthread := utility_log_test utility_cpu_test
$(test_cases_requiring_pthread): CFLAGS += -pthread
$(test_cases_requiring_pthread): LDFLAGS += -lpthread

test_cases_requiring_rt := utility_cpu_test
$(test_cases_requiring_pthread): LDFLAGS += -lrt

utility_cpu_test: utility_file.o
# End of individual test case requirements

# Main rules
check: $(test_cases) $(test_cases_sudo)
	@set -e; \
	echo "[With Valgrind] Test cases without root privilege"; \
	for test_case_exec in $(test_cases); do \
		valgrind --leak-check=full -q ./$$test_case_exec 1; \
	done; \
	echo "[With Valgrind] Test cases with root privilege"; \
	for test_case_sudo_exec in $(test_cases_sudo); do \
		sudo valgrind --leak-check=full -q ./$$test_case_sudo_exec 1; \
	done; \
	echo "[Without Valgrind] Test cases without root privilege"; \
	for test_case_exec in $(test_cases); do \
		./$$test_case_exec 0; \
	done; \
	echo "[Without Valgrind] Test cases with root privilege"; \
	for test_case_sudo_exec in $(test_cases_sudo); do \
		sudo ./$$test_case_sudo_exec 0; \
	done

clean:
	-rm -- *.o *.d $(test_cases) $(test_cases_sudo) > /dev/null 2>&1
# End of main rules

# Automatic dependency generation taken from GNU Make documentation
%.d: %.c
	@echo -n "Dependency generation for $<... "
	@set -e; rm -f $@; \
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$; \
	echo DONE

include $(patsubst %.c,%.d,$(wildcard *.c))
# End of automatic dependency generation
