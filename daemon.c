/*
 * (C) Copyright Dan Shearer 2003-2008
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 3 of the License, or (at your option) any later version. You
 * should have received a copy of the license with this program; if
 * not, go to http://www.fsf.org.
*/

/*

Daevel Generic simple TCP daemon template.

Note! This is the *server* not the *client*.

Written because I couldn't find any such thing elsewhere.

Tries to be careful, and run on many Unix and Win32 platforms. Goal is to
encapsulate best practice and to be the starting point for other daemons with a
smallish scope

*/

#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>  /* pid_t and friends */
#include <stdlib.h>     /* exit codes */
#include <string.h>
#include <netdb.h>  /* dns stuff */
#include <fcntl.h>
#include <stdarg.h>  /* va_list */
#include <time.h>  /* strftime */
#include <arpa/inet.h>   /* ntoa etc */
#include <unistd.h>    /* getpid */

#include "global.h"
#include "util.h"
#include "log.h"
#include "lockfile.h"
#include "socket.h"
#include "confdata.h"

unsigned child_count = 0; /* 0 means no clients, not the first client */

unsigned master_process = TRUE;

int tortu_sock = -1;    /* socket descriptor */

unsigned lock_acquired = FALSE;

extern void daemon_child_function(FILE *incoming, FILE *outgoing, char *incoming_name);

/* both the initial process and the master daemon process have
   master_process set, since it is more convenient */

int dead_child()
{
  /* use __WAIT_STATUS_DEFN coz __WAIT_STATUS is a union on some architectures
     so can't assign it to NULL */
  __WAIT_STATUS_DEFN res;
  pid_t pid;

  LOG(9, ("About to wait3() in dead_child\n"));
  /* UNFEATURE don't use wait3,use wait. Check the differences */
  /* UNFEATURE don't use wait4,use waitpid */
  /* actually, strace shows that wait3 is a wrapper for wait4 anyway :-) */
  res = NULL;
  pid = wait3(res, WNOHANG, (struct rusage *)0);
  switch (pid)
  {

  case - 1:
    PANIC(("wait3 reported error %d, %s\n", errno, strerror(errno)));

  case 0:
    PANIC(("wait3 reported no child exited, error %d,%s\n",
           errno, strerror(errno)));
    /* how serious is this really? */

  default: /* note PANIC above never returns */
    if (child_count == 0)
    {
      PANIC(("dead_child: child count == 0 immediately before decrement\n"));
    }
    child_count--;
    if (child_count == 0)
    {
      LOG(9, ("dead_child: child count now 0\n"));
    }
  }
  LOG(1, ("child pid %d died\n", pid));
  return 0;
} /* dead_child */


void shutdown_signal(int signum)
{

  struct sigaction sa;
  pid_t pid = getpid();
  int res = 0;

  if (signum < 0)
  {
    PANIC(("invalid signum passed to shutdown_signal\n"));
  }

  if (master_process == TRUE)
  {

    /* kill all other processes in group nicely, then die */
    if (sigemptyset(&sa.sa_mask) == -1)
    {
      PANIC(("sigemptyset() returned error!\n"));
    }
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    res = sigaction(SIGTERM, &sa, (struct sigaction *) 0);
    LOG(1, ("Master process received shutdown signal\n"));

    if (res < 0)
    {
      PANIC(("Cannot set SIG_IGN for SIGTERM in master process: suiciding...\n"));
    }
    res = kill(-pid, SIGTERM);

    if (res < 0)
    {
      LOG(1, ("couldn't kill process group from master process\n"));
    }

    res = close(tortu_sock);
    if (res != 0)
    {
      PANIC(("Closing daemon socket gave error %d, %s\n",
             errno, strerror(errno)));
    }

    lockfile_remove();

    LOG(9, ("Finishing logging\n"));
    log_finish();

    if (close_all(0) < 0)
    {
      PANIC(("Error in close_all when terminating master process\n"));
      /* normally this won't have anywhere to write to due to log_finish */
    }

    exit(EXIT_SUCCESS);

  }
  else
  {

    LOG(1, ("Child process received shutdown signal\n"));
    log_finish();
    if (close_all(0) < 0)
    {
      PANIC(("Error in close_all when terminating child process\n"));
    }
    _exit(EXIT_FAILURE);
    /* see tcp/ip programming faq for explanation of exit() vs _exit().
       In short, it's more brutal. */
  }

} /* shutdown_signal */


/* Map all shutdown-type signals to shutdown_signal */
static int setup_signals()
{

  struct sigaction sig;
  int res = 0;
  unsigned i = 0;
  unsigned num = 0;
  int ignore;

  static int siglist[] =
  {
    SIGINT, SIGQUIT, SIGSEGV, SIGILL, SIGTERM, SIGFPE
  };
  /* Don't include SIGABRT in the above list, or PANIC loops if dump_core
     set, because core is dumped in panic() by calling abort() */

  ignore = sigemptyset(&sig.sa_mask);
  sig.sa_handler = &shutdown_signal;
  sig.sa_flags = 0; /* essential; otherwise doesn't register. check this UNFEATURE */

  num = sizeof(siglist) / sizeof(siglist[0]);
  for (i = 0 ; i < num ; i++)
  {
    /* sigaction() is more portable across Unix-like OSs than signal() */
    res = sigaction(siglist[i], &sig, (struct sigaction *)0);
    if (res < 0)
    {
      LOG(1, ("sigaction failed with error %d\n", res));
      return -errno;
    }
  }

  sig.sa_handler = (void*) & dead_child;
  res = sigaction(SIGCHLD, &sig, (struct sigaction *)0);
  if (res < 0)
  {
    LOG(1, ("sigaction failed with error %d\n", res));
    return -errno;
  }

  /*  signal(SIGCHLD,(void *)dead_child); oldfashioned :) */

  return 0;
} /* setup_signals */

/* kills the process holding the lockfile. Not many facilities available
   because we skip most of the program initialisation since we're about to
   die anyway. */
void fratricide()
{
  /* Use PANIC not LOG here, because sometimes no logfile set up yet */
  char msg[255];
  pid_t mypid = getpid();
  pid_t pid = -1;

  if (lock_acquired == TRUE)
  {
    PANIC(("Programming error: can't kill others coz I'm"
           " holding lock myself\n"));
  }

  pid = lockfile_check();
  if (pid == 0)
  {
    PANIC(("No-one to kill, because nobody holds lockfile\n"));
    exit(EXIT_SUCCESS);
  }

  if (pid == mypid)
  {
    PANIC(("Odd! pid in %s was same as mine (%i)\n",
           GLOBAL_LOCKFILE_NAME, mypid));
  }

  if (kill(pid, SIGINT) < 0)
  {
    PANIC(("SIGINT to process %i gave error %d, %s\n", pid, errno, strerror(errno)));
  }

  /* can't call PANIC this time, because it exit()s (or abort()s) with an
     errorlevel, and by now we know that we are successful */
  if (isatty(2) > 0)
  {
    snprintf(msg, sizeof(msg), "Killed pid %d\n", pid);
    fprintf(stderr, msg);
  }
  exit(EXIT_SUCCESS);

} /* fratricide */

int main(int argc, char **argv)

{

  int clisockdes = -1;
  int clisockdes_dup = -1;
  unsigned len = 0;
  pid_t pid = 0;

  struct sockaddr_in sin, child_sin;
  char incoming_addr[16];    /* UNFEATURE AF_INET6 */
  struct hostent *incoming_dns; /* only in children; can be high-cost */
  char incoming_name[256];  /* plain text hostname from DNS */

  FILE *incoming = NULL;
  FILE *outgoing = NULL;

  unsigned children [ABSOLUTE_MAX_CHILDREN];
  /* active childrens' socket descriptors */

  memset(incoming_addr, 0, sizeof(incoming_addr));
  /*memset(&incoming_dns,0,sizeof(incoming_dns));*/
  /*LOG(1,("Done memset\n"));*/

  global_options = malloc(sizeof(options));
  if (global_options == NULL)
  {
    PANIC(("Out of memory allocating global options, dying\n"))
  };
  initialise_options(global_options);
  fill_default_options(global_options);

  if (argc > 0)
  {
    if (argv != NULL)
    {
      process_cmdline(argc, argv);
    }
    else
    {
      /* how paranoid can you get :) */
      PANIC(("strange! argv null when argc > 0\n"));
    }
  }

  /* sets up logging on fd's 0,1 and 2, unless foregroundonly
   * this will close tortu_sock if it is open, so call this first */
  log_start();

  if (global_options->loglevel == MAX_LOGLEVEL)
  {
    LOG(9, ("read cmdline arguments, settings follow\n"));
    log_option_status();
  }

  /* loglevel doesn't get changed underneath you in the special case
     of checkconfig==TRUE even if it is set in the config file, because
     confdata.c notices and readdjusts it to MAX_LOGLEVEL */
  if (strlen(global_options->configfilename) > 0)
  {
    process_configfile(global_options);
    if (global_options->loglevel == MAX_LOGLEVEL)
    {
      LOG(9, ("config file read, settings follow\n"));
      log_option_status();
    }
  }
  else
  {
    if (global_options->loglevel == MAX_LOGLEVEL)
    {
      LOG(1, ("No config file to read\n"));
    }
  }

  if (global_options->checkcfg == TRUE)
  {
    LOG(1, ("checkcfg - exiting without runing daemon\n"));
    exit(EXIT_SUCCESS);
  }

  if (global_options->terminate == TRUE)
  {
    fratricide();
  }

  /* lockfile checking is a special case, where we always want the error
     message to go to stdout regardless of logging options */
  pid = lockfile_check();
  if (pid > 0)
  {
    LOG(1, ("Can't start new daemon: pid %d has a valid lock\n", pid));
    PANIC(("Can't start new daemon: pid %d has a valid lock\n", pid));
  }

  LOG(1, ("%s: checked config. Initial process starting\n", argv[0]));
  log_option_status(); /* does nothing if loglevel too low */

  memset(children, 0, (ABSOLUTE_MAX_CHILDREN)*sizeof(unsigned));

  if (setup_signals() < 0)
  {
    PANIC(("can't setup signals, exiting\n"));
  }

  /* get_lock_or_die(); */

  if (global_options->foregroundonly == FALSE)
  {
    int ismaster;
    ismaster = become_daemon();
    if (ismaster < 0)
    {
      PANIC(("Can't become daemon, exiting\n"));
    }
    if (ismaster)
      master_process = TRUE;
    else
      master_process = FALSE;
    LOG(9, ("Now I'm a daemon\n"));
  }
  else
  {
    LOG(2, ("Not daemonising - foregroundonly flag set\n"));
  }

  /* init_socket should be after become_daemon, since become_daemon
   * closes all open file descriptors */
  tortu_sock = init_socket((struct sockaddr_in *) & sin);
  if ( tortu_sock == -1 )
  {
    PANIC(("Can't setup socket in %s\n", argv[0]));
  }

  get_lock_or_die();  /* can't do until we're a daemon */

  /* This loop accepts a connection, applies some basic checks, starts a
  child process if it passes the tests, the child does more acceptance
  tests, and finally the child can do the processing. A good example
  of splitting the testing like this is found in the Exim 3.x source. The
  child gets to do the potentially time-consuming tests, but the parent
  does tests which stop forking denials of service */

  len = sizeof(child_sin);

  for (;;)
  {

    LOG(9, ("About to block on filtered_accept()\n"));
    clisockdes = filtered_accept(tortu_sock, (struct sockaddr *) & child_sin, &len);

    if (clisockdes < 0)
    {
      if (errno == EAGAIN)
        continue; /* many errors are squashed into EAGAIN */
      PANIC(("filtered_accept() failed with error %i, %s\n", errno,
             strerror(errno)));
      /* UNFEATURE - are there any errors we want to recover from? */
    }

    /* UNFEATURE cater for AF_INET6 */
    /* could equally have used getpeername */
    strncpy(incoming_addr, inet_ntoa(child_sin.sin_addr), sizeof(incoming_addr));

    LOG(1, ("Connection attempt from %s\n", incoming_addr));

    /* before we do initial checks, need to be able to talk to the client. This
    will almost always be the case unless the protocol being implemented
    doesn't care about error states very much */
    outgoing = fdopen(clisockdes, "w");
    clisockdes_dup = dup(clisockdes);
    if (clisockdes_dup < 0)
    {
      LOG(1, ("dup() failed initialising connection with %d, %s\n", errno, strerror(errno)));
      fprintf(outgoing, "Can't initialise connection\n");
      goto error_shortcut;
    }
    incoming = fdopen(clisockdes_dup, "r");

    if (child_count == global_options->maxchild)
    {
      LOG(1, ("Maximum children reached, refusing to fork() again\n"));
      fprintf(outgoing, "Maximum processes reached, try again later\n");
      goto error_shortcut;
    }
    else
    {
      LOG(9, ("About to fork() off a child\n"));
    }

    /* increment child counter before the fork. SIGCHLD could arrive
       between the fork and the increment, and the signal handler
       decrements counter. Any other behaviour is a bug */

    child_count++;

    switch (fork())
    {

    case - 1:
      child_count--;
      LOG(1, ("fork() parent main loop gave error %d, %s\n", errno, strerror(errno)));
      break;

    case 0:

      /* Don't set child_count=0. It's global. If you want a variable to answer
         "how many children does this process have" then create a new one. */
      master_process = FALSE;
      close(tortu_sock);

      LOG(9, ("Created new child process\n"));

      if (global_options->dnslookups == TRUE)
      {
        incoming_dns = gethostbyaddr((const char *) & child_sin.sin_addr,
                                     sizeof(&child_sin.sin_addr), AF_INET);
        if (incoming_dns != NULL)
        {
          strncpy(incoming_name, incoming_dns->h_name, sizeof(incoming_name));
          LOG(1, ("Resolved %s\n", incoming_name));
        }
        else
        {
          LOG(1, ("PTR lookup failed for %s\n", incoming_addr));
          strncpy(incoming_name, incoming_addr, sizeof(incoming_name));
        }
      }
      else
      {
        /* no lookups, so put numeric address in name string */
        strncpy(incoming_name, incoming_addr, sizeof(incoming_name));
      } /* dns_lookups */

      daemon_child_function(incoming, outgoing, incoming_name); /* never returns */

      PANIC(("Unreachable code in main switch reached!\n"));
      /* PANIC never returns */

    default:
      /* we're the parent, and no error - just keep going */

      ; /* nop */

    } /* switch */

error_shortcut:

    if (incoming != NULL)
    {
      fclose(incoming);
      incoming = NULL;
    }

    fclose(outgoing);
    outgoing = NULL;
    memset(&incoming_addr, 0, sizeof(incoming_addr));

  } /* endless loop */

  return 0;

} /* main */


