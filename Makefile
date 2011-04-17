.PHONY := check clean mrproper

CFLAGS := -O3 -Wall

TEST_CASES := utility_time_test

check: $(TEST_CASES)
	for test_case_exec in $^; do \
		valgrind --leak-check=full -q ./$$test_case_exec; \
	done

utility_time_test: utility_time.h

clean:
	-rm -- $(OBJECTS) $(TEST_CASES) > /dev/null 2>&1

mrproper: clean
	-rm -- $(EXECUTABLES) > /dev/null 2>&1