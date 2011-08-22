include ../Makefile

# Part that each experimentation component should customize
test_cases = 
test_cases_sudo =
executables = main cpu_hog_cbs

cond_for_pthread +=
cond_for_rt +=

autodep_list +=
# End of customizable part

.DEFAULT_GOAL = all
.PHONY += all

all: $(executables)

# Include autodep files of the infrastructure components
include $(filter-out %_test.d,$(patsubst ../%.c,%.d,$(wildcard ../*.c)))

# Set search path for the infrastructure components
VPATH = ..
