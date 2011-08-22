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
#include <signal.h>
#include "../utility_experimentation.h"
#include "../utility_log.h"

static volatile int terminated = 0;
static void sighand(int signo)
{
  if (signo == SIGTERM) {
    terminated = 1;
  }
}

MAIN_BEGIN("cpu_hog", "stderr", NULL)
{
  struct sigaction sighand_sigterm = {
    .sa_handler = sighand,
  };
  if (sigaction(SIGTERM, &sighand_sigterm, NULL) != 0) {
    fatal_syserror("Cannot install SIGTERM handler");
  }

  /* Hog the CPU */
  while (!terminated)
    ;
  /* END: Hog the CPU */

  return EXIT_SUCCESS;

} MAIN_END
