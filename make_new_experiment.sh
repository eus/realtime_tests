#!/bin/bash

#############################################################################
# Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  #
#                                                                           #
# This program is free software: you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation, either version 3 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.     #
#############################################################################

if [ -z "$1" ]; then
    echo "Usage: make new_experiment EXP_NAME=desired_experiment_name"
    exit 1
fi

set -e
mkdir "$1"
touch "$1/README"
cat <<'EOF' | sed -e 's%\$(1)%'"$1"'%' > "$1/main.c"
/*****************************************************************************
 * Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../utility_experimentation.h"

MAIN_BEGIN("$(1)", "stderr", NULL)
{

  return EXIT_SUCCESS;

} MAIN_END
EOF

cat <<'EOF' > "$1/Makefile"
include ../Makefile

# Part that each experimentation component should customize
test_cases = 
test_cases_sudo =
executables = main

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
EOF

exit 0
