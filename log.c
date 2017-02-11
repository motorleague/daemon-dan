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

/* logfile.c

   Module to provide both a bare-bones logging (PANIC macro) and a more
   sophisticated facility (LOG macro).

*/

#include <stdio.h>
#include <stdarg.h>  /* vararg stuff */
#include <stdlib.h>  /* exit codes */
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "global.h"
#include "util.h"
#include "log.h"

char GLOBAL_LINE[255] = "";  /* For PANIC macro. ANSI C macros do not allow */
char GLOBAL_FILE[255] = "";  /* variable arguments (...), unlike gcc. */

int logfile = -1;         /* file descriptor */

static int logfile_istty = FALSE;
/* set in log_start, avoids ioctl failure and errno reset in log_msg */

/* panic */
void panic(const char* f, ...)
{
  va_list ap;
  char msg[255];
  char msg2[255];

  memset(msg, 0, sizeof(msg));
  memset(msg2, 0, sizeof(msg2));

  va_start(ap, f);
  vsnprintf(msg, sizeof(msg), f, ap);
  va_end(ap);
  if ( (strlen(GLOBAL_FILE) > 0) && (strlen(GLOBAL_LINE) > 0) )
  {
    snprintf(msg2, sizeof(msg2),
             "PANIC in line %s of %s: %s", GLOBAL_LINE, GLOBAL_FILE, msg);
  }
  else
  {
    snprintf(msg2, sizeof(msg2),
             "Programmer error: call PANIC not panic() when trying to log %s", msg);
    abort();
  }

  /* can't call log_msg before or during log_start */
  if (logfile > -1)
  {
    log_msg(msg2);
  }
  else
  {
    /* might not work if stderr has already been taken, but worth a try */
    if (isatty(2) > 0)
    {
      fprintf(stderr, msg2);
    }
  }

  if (global_options->dumpcore == TRUE)
  {
    abort();
  }
  else
  {
    exit(EXIT_FAILURE);
  }

} /* panic */

/* log_start: close all, then open 0, 1 and 2 */
void log_start()
{

  /* global var, avoids annoying error with -F foreground (and strace -f) */
  if (isatty(logfile) == 1)
  {
    logfile_istty = TRUE;
  }

  if (global_options->foregroundonly == FALSE)
  {
    if (close_all(0) < 0)
    {
      PANIC(("Cannot close all files, error %d, %s\n",
             global_options->logfilename, errno, strerror(errno)));
    }
    if (open("/dev/null", O_WRONLY) != 0)
      /* now guaranteed to be stdin, ie fd 0 on Unix/Cygwin
         A daemon doesn't want any input. */
    {
      PANIC(("open(""/dev/null"") gave %d, %s\n", errno, strerror(errno)));
      /* in fact no error message will ever get printed, because we've just closed
         all fd's. */
    }
  }
  else
  {
    strncpy(global_options->logfilename, "/dev/stdout",
            sizeof(global_options->logfilename));
    LOG(0, ("Opened /dev/stdout for logfile\n"));
  }

  logfile = open(global_options->logfilename,
                 O_RDWR | O_APPEND | O_CREAT, 0600 );
  if (logfile < 0)
  {
    PANIC(("cannot open() logfilename=%s, error %d, %s\n",
           global_options->logfilename, errno, strerror(errno)));
  }

  if (!dup(1))
  {
    PANIC(("dup() failed with rare error %d, %s\n", errno, strerror(errno)));
  }

} /* log_start */

/* log_finish */
void log_finish()
{

  if (logfile_istty == TRUE)
  {
    if (fsync(logfile))
    {
      PANIC(("Cannot sync() logfile in log_finish\n"));
    }
  }
  if (close(logfile))
  {
    PANIC(("Cannot close() logfile in log_finish\n"));
  }
  logfile = -1;
} /* log_finish */


/* log_msg. writes to logfile (which may be a file, or /dev/console for debug)
   Errors are handled where possible, but you can't exactly write a log entry.
*/
void log_msg(const char* f, ...)
{
  va_list ap;
  char str[400];
  char str2[400];
  unsigned res = 0;
  unsigned str2_count = 0;
  time_t now = time(NULL);
  struct tm *tm;
  pid_t mypid = getpid();
  pid_t parentpid = getppid();
  int ignore;

  memset(str, 0, sizeof(str));
  memset(str2, 0, sizeof(str2));

  /* timestamp, not including "\n" from strftime */
#define TIMESTAMPL 26 /* width of time string output */

  if (now == -1)
  {
    snprintf(str2, TIMESTAMPL, "Invalid time() result!   ");
  }
  else
  {
    tm = localtime(&now);  /* no error values, and always > NULL */
    /* year according to ISO8601 */
    res = strftime(str2, TIMESTAMPL, "%G-%m-%d %H:%M:%S ", tm);
    /* The Linux strftime man page of 29 Mar [19]99 claims zero returned if
       zero bytes written to string, but zero is returned on successful
       write! Assuming for now that <0 means error. Need to verify */
    if (res < 0)
    {
      snprintf(str2, TIMESTAMPL, "Invalid strftime() res!  ");
    }
  }

  /* pid display field 5 chars for 16-bit, 11 for 32-bit pids, add one for spacing */
#define MAX_PID_LEN sizeof(pid_t)/2*3

  str2_count = strlen(str2);
  strncpy(str, "pid=%i \x0", 6 + MAX_PID_LEN);
  if (mypid >= 0)
  {
    snprintf(&str2[str2_count], strlen(str) + MAX_PID_LEN - 2, str, mypid);
  }
  else
  {
    snprintf(&str2[str2_count], strlen(str) + MAX_PID_LEN - 2, "bad getpid ");
  }

  str2_count = strlen(str2);
  strncpy(str, "(%i) \0x0", 3 + MAX_PID_LEN);
  if (parentpid >= 0)
  {
    snprintf(&str2[str2_count], strlen(str) + MAX_PID_LEN - 2, str, parentpid);
  }
  else
  {
    snprintf(&str2[str2_count], strlen(str) + MAX_PID_LEN - 2, "bad ppid");
  }

  str2_count = strlen(str2);
  va_start(ap, f);
  vsnprintf(&str2[str2_count], (sizeof(str2) - str2_count), f, ap);

  va_end(ap);

  /* ignore errors with write() and fsync() here. We have no way of
  reporting these errors, and it is better to attempt to continue
  operation in the hope that the logfile comes good than just halt the
  daemon without explanation */

  ignore = write(logfile, str2, strlen(str2));

  if (logfile_istty == FALSE)
  {
    /* no fsync if to stdout - annoying error with strace */
    ignore = fsync(logfile);
  }

} /* log_msg */
