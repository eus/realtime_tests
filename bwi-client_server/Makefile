include ../Makefile

# Part that each experimentation component should customize
test_cases = 
test_cases_sudo =
executables = main-1 main-2 main-3 main-4 main-5 main-6 main-7 main-8 main-9 \
main-10 main-11 main-12 main-13 main-14 main-15 main-16 main-17 main-18 \
main-19 main-20 main-21 main-22 \
server client cpu_hog server_hog cpu_hog_cbs hrt_cbs

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
