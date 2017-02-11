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


/* util.c

   General functions

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "global.h"
#include "log.h"
#include "util.h"

/* close all possible file descriptors in this process > specified value,
   except nominated descriptor (usually the connected socket) */
int close_all(int startfd)
{

  long max = 0;
  int i = 0;

  max = sysconf(_SC_OPEN_MAX);
  if ((startfd < 0) || (startfd > max))
  {
    PANIC(("startfd parameter out of range in close_all()\n"));
  }

  if (max == 0)
  {
    LOG(1, ("sysconf() failed strangely: value not available\n"));
    return -1;
  }
  else if (max == -1)
  {
    LOG(1, ("sysconf() failed with an error\n"));
    return -1;
  }

  for (i = startfd;i < max;i++)
  {
    /* UNFEATURE should check for EIO (abort) & EINTR (retry) but
       ignore EBADF */
    close(i);
  }

  return 0;

} /* close_all */

/* called by INT_ISSET macro in header file */
/* Returns first offset for int target == iset[offset], -1 for no match.*/

int int_isset(int *iset, int target, int num)
{
  unsigned i = 0;

  if ( (iset == NULL) || (num <= 0) )
  {
    PANIC(("bad parameter to int_isset\n"));
  }

  for (i = 0; i < num; i++)
  {
    if (iset[i] == target)
    {
      return i;
    }
  }
  return -1;
}

/* Become a daemon by disposing of the controlling tty */
/* Don't use daemon() because it isn't very portable even on Unixes*/

int become_daemon()
{
  pid_t pid = -1;
  int ismaster = 0;

  switch (pid = fork())
  {

  case - 1:
    LOG(1, ("Daemonising fork() failed with error %d, %s\n", errno, strerror(errno)));
    return -1;

  case 0:   /* child, so we know we are not a process group leader */
    if (setsid() < 0)
    {
      PANIC(("fork() succeeded but setsid() in child failed strangely with"
             " error %d, %s\n", errno, strerror(errno)));
    };
    ismaster = 1; /* should have been already, anyway */
    (void)chdir("/"); /* so we don't prevent filesystems unmounting */
    /* close file descriptors other than log to avoid resource depletion */
    if (close_all(3) < 0)
    {
      PANIC(("close_all failed in newly created daemon\n"));
    }
    LOG(9, ("Master daemon launched happily\n"));
    break;

  default:  /* original master process finished, so terminate */
    LOG(1, ("Initial process terminating. daemon started, pid=%i\n", pid));
    log_finish();
    if (close_all(0) < 0)
    {
      PANIC(("couldn't close all files trying to terminate initial process\n"));
    }
    exit(EXIT_SUCCESS);

  };

  return(ismaster);

} /* become_daemon */








