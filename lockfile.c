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

/* lockfile.c

   These simplistic locking functions do not use the lockfile
   generation algorithm as given in the man page for lockfile_create, which
   is complete and complex. A more complete daemon should link against
   liblockfile, available from any Debian archive and many other places.

   The method here uses neither fnctl nor flock, which improves portability.
   It is not safe for use on NFS filesystems due to NFS brokenness.

   Looked at fetchmail 5's probably-unsafe lockfile routines (and carefully use
   nothing found there)

 */

#include <sys/types.h>  /* pid_t and friends */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "global.h"
#include "lockfile.h"
#include "log.h"

void get_lock_or_die()
{
  int fd, die;
  char str[50];
  pid_t mypid = getpid();

  die = FALSE;
  fd = open(GLOBAL_LOCKFILE_NAME, O_WRONLY | O_CREAT | O_EXCL | O_SYNC, 0600 );
  LOG(9, ("open() on %s returned %d\n", GLOBAL_LOCKFILE_NAME, fd));

  if (fd != -1)
  {
    memset(str, 0, sizeof(str));
    if (mypid >= 0)
    {
      snprintf(str, sizeof(str), "%d", mypid);
      write(fd, str, strlen(str));
      close(fd);
      LOG(9, ("Wrote pid of %s to lockfile %s\n", str, GLOBAL_LOCKFILE_NAME));
    }
    else
    {
      die = TRUE;
    }
  }
  else
  {
    die = TRUE;
    LOG(9, ("In get_lock_or_die open() on %s failed with %d, %s\n",
            GLOBAL_LOCKFILE_NAME, errno, strerror(errno)));
  }

  if (die == TRUE)
  {
    PANIC(("Cannot secure lock on %s\n", GLOBAL_LOCKFILE_NAME))
  }

  lock_acquired = TRUE;
  LOG(9, ("Acquired lock\n"));
} /* get_lock_or_die */

/* Returns 0 for no other process, else pid of process that has the lock.
   Removes stale lockfiles.
*/
int lockfile_check()
{
  pid_t pid = 0;
  int ret = -1;
  int val = 0;
  FILE *lf = NULL;

  lf = fopen(GLOBAL_LOCKFILE_NAME, "r");
  LOG(9, ("fopen() on %s returned error %d, %s\n",
          GLOBAL_LOCKFILE_NAME, errno, strerror(errno)));

  if (lf != NULL)
  {
    if (fscanf(lf, "%d", &pid) == 1)
    {
      LOG(9, ("Existing lockfile %s has pid = %d\n", GLOBAL_LOCKFILE_NAME, pid));
      val = kill(pid, 0);
      /* ideally should just look for ESRCH but EINVAL is returned on
         some platforms including Linux 2.4 */
      if (val < 0)
      {
        LOG(9, ("Assume no lock; process kill returned %d, %s\n", errno, strerror(errno)));
        ret = 0;
        fclose(lf);
        if (unlink(GLOBAL_LOCKFILE_NAME) == 0)
        {
          LOG(9, ("Unlinked stale lockfile %s\n", GLOBAL_LOCKFILE_NAME));
        }
        else
        {
          LOG(9, ("Unlinking %s returned error %d, %s\n",
                  GLOBAL_LOCKFILE_NAME, errno, strerror(errno)));
        }
      }
      else /* another process exists so we can't get the lock */
      {
        ret = pid;
      }
    }
    else
    {
      PANIC(("Error reading contents of lockfile %s!\n", GLOBAL_LOCKFILE_NAME));
    }
  }
  else
  {
    LOG(9, ("lockfile_check assuming no lock\n"));
    ret = 0;
  }

  LOG(9, ("About to return %d in lockfile_check\n", ret));
  return ret;

} /* lockfile_check */

/* relinquish lock and remove lockfile */
void lockfile_remove()
{
  if (GLOBAL_LOCKFILE_NAME)
  {
    if (lock_acquired == TRUE)
    {
      if (unlink(GLOBAL_LOCKFILE_NAME))
      {
        PANIC(("Couldn't remove lockfile %s!\n", GLOBAL_LOCKFILE_NAME));
      }
      LOG(9, ("Unlinked lockfile %s\n", GLOBAL_LOCKFILE_NAME));
    }
    else
    {
      PANIC(("Tried to remove a lock on %s I didn't have!\n",
             GLOBAL_LOCKFILE_NAME));
    }
  }

  lock_acquired = FALSE;

} /* lockfile_remove */
