/*
 * (C) Dan Shearer 2003-2008
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 3 of the License, or (at your option) any later version. You
 * should have received a copy of the license with this program; if
 * not, go to http://www.fsf.org.
*/


/* daemon_child_func.c

   child function for daemon template. This function never returns, because it
   is what the detached daemon spins off for every connection.

*/

#include <stdio.h>
#include <stdlib.h> /* exit codes and things */
#include <unistd.h> /* _exit */
#include "log.h"
#include "global.h"

void daemon_child_function(FILE *incoming, FILE *outgoing, char *incoming_name)
{
  int num = 0;
  int ignore;

  fprintf(outgoing, "Hello %s\n", incoming_name);
  ignore = fflush(outgoing);

  while ( (num = getc(incoming)) > 0 )
  {
    if (num == '1')
    {
      break;
    }
    ignore = putc(num, outgoing);
    ignore = fflush(outgoing);
  }
  fclose(outgoing); /* and a close_all...? */
  fclose(incoming);

  LOG(9, ("About to _exit() child\n"));
  _exit(EXIT_SUCCESS);

} /* child_function */
