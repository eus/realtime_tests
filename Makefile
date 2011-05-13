test_cases := utility_time_test utility_log_test utility_file_test
test_cases_sudo := utility_cpu_test

executables := $(test_cases) $(test_cases_sudo)

cond_for_pthread := utility_log.h utility_cpu.h
cond_for_rt := utility_cpu.h

# The part that follows should need no modification

.PHONY := check check_sudo clean mrproper
.DEFAULT_GOAL := check

override CFLAGS := -O3 -Wall $(CFLAGS)

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
	-rm -- *.o *.d $(executables) > /dev/null 2>&1
# End of main rules

# Automatic dependency generation based on GNU Make documentation
define gen_shell_cmd_to_inline_prereq_hdrs
sed -n -e '1 {h;D}' -e 'H' -e '$$ {x;s%[\\\n]%%g;p}'
endef

extract_prerequisites := 's%^[^:]\+:% %'
remove_system_headers := 's% /[^ ]\+%%g'
define gen_shell_cmd_to_extract_prereq_hdrs
sed -e $(extract_prerequisites) -e $(remove_system_headers) $(1) \
| $(gen_shell_cmd_to_inline_prereq_hdrs)
endef

replace_space_for_grep := 's% %\\\\| %g'
put_the_prefix_for_grep := 's%^% %'
define gen_shell_cmd_to_make_condition_list
sed -e $(replace_space_for_grep) \
    -e $(put_the_prefix_for_grep)
endef

# The result of this function expects to receive the input through shell pipe.
# The content of the shell pipe is a space-separated list of words and the first
# word in the list must have at least one leading space.
# $1: a space-separated list of words serving as the pattern
# $2: the shell command to be executed for each word in the shell pipe that
#     matches the given pattern. The matched word is stored in shell variable
#     WORD.
define gen_shell_cmd_to_filter
for WORD in $$(grep -o "`echo '$(strip $(1))' \
                         | $(gen_shell_cmd_to_make_condition_list)`"); \
do \
    $(2); \
done
endef

# $1: old extension
# $2: new extension
define gen_shell_cmd_to_change_file_ext
sed -e 's%\.$(1)$$%.$(2)%'
endef

# $1: the name of the dependency
define gen_shell_cmd_for_autodep
echo "$$prereqs" \
| $(call gen_shell_cmd_to_filter,$(cond_for_$(1)),$(gen_makefile_for_$(1)));
endef

autodep_list := pthread rt needed_objects

cond_for_needed_objects := [^ ]*\.h

# These are to be solely used in the autodep rule "%.d: %.c" that follows
define gen_makefile_for_pthread
echo '$*.o: CFLAGS += -pthread' >> $@; \
echo '$*: LDFLAGS += -lpthread' >> $@; \
break
endef

define gen_makefile_for_rt
echo '$*: LDFLAGS += -lrt' >> $@; \
break
endef

define gen_makefile_for_needed_objects
if [ -f `echo $$WORD | $(call gen_shell_cmd_to_change_file_ext,h,c)` ]; \
then \
    echo $*: `echo $$WORD | $(call gen_shell_cmd_to_change_file_ext,h,o)` \
        >> $@; \
fi
endef
# End of defines solely used in the following autodep rule "%.d: %.c"

%.d: %.c
	@echo -n "Dependency generation for $<... "
	@set -e; rm -f $@; \
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
	prereqs="`$(call gen_shell_cmd_to_extract_prereq_hdrs,$@.$$$$)`"; \
	$(foreach x,$(autodep_list),$(call gen_shell_cmd_for_autodep,$(x))) \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ >> $@; \
	rm -f $@.$$$$; \
	echo DONE

include $(patsubst %.c,%.d,$(wildcard *.c))
# End of automatic dependency generation
