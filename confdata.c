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

/* confdata.c

   This module is the one place where configuration options are set.

   The processing order is:

      0.  set the default options (done here in
          fill_default_options(), but some programs might set them
          elsewhere such as in global.h or even main.c)

      1.  commandline options

      2.  configuration file (if one is specified in the commandline, or
                              maybe in the default options)

   Things related to options are also handled here, including:

       - commandline usage help message
       - logging the status of all config options, if the log level
         is high enough
       - the -o parameter, which checks all configuration settings and
         exits

   If you are adding or modifying a configuration option of any sort
   all the meta-handling should be only in this file. Obviously, the
   implementation of what the option means should be in your own code
   except for options such as portnum that are part of the template.

   To add an option:

      a. edit global.h and add an entry to the struct options. Don't use
         int if you really mean unsigned. Everything else is in this file.

      b. add to the typedef struct confoptions and static struct keywords[]
         (in this file) whatever makes sense for the new option

      c. add to
                initialise_options()
                fill_default_options()
                log_option_status()

         You must do this no matter what the option is for and whether
         or not it can be set on the commandline or a configfile.

      d. if the new option is to be settable by the commandline, edit
         get_cmdline() and cmdline_message(). Options do not have to be
         accessible via the commandline.

      e. if the new option is to be settable by the config file, edit
         process_configfile to add a case statement for the new opcode.

   In designing this module, reviewed option handling in Exim, Apache,
   Samba, OpenSSH, fetchmail and a few others. Most of this code is
   completely non-reuseable. Some of this file is a little bit similar
   to what is in servconf.c in OpenSSH 2.9p2.

*/

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "global.h"
#include "confdata.h"
#include "log.h"
#include "util.h"

/* tokens */
typedef enum
{
  oBadoption,
  oLoglevel,
  oPortnum,
  oDnslookups,
  oForegroundonly,
  oLogfilename,
  oConfigfilename,
  oMaxchild,
  oDumpcore,
  oTerminate,
  oCheckcfg,
} confoptions;

/* Text representation of the tokens. */
static struct
{
  const char *name;
  confoptions opcode;
}
keywords[] =
{
  { "badoption", oBadoption },
  { "loglevel", oLoglevel },
  { "portnum", oPortnum },
  { "dnslookups", oDnslookups },
  { "foregroundonly", oForegroundonly },
  { "logfilename", oLogfilename },
  { "configfilename", oConfigfilename },
  { "maxchild", oMaxchild },
  { "dumpcore", oDumpcore },
  { "terminate", oTerminate },
  { "checkcfg", oCheckcfg },
  { NULL, 0 }
};

static FILE* conffile; /* file descriptor */

/* sets the global options structure to values which indicate that they have
   not been set yet. Every option must have this value cleared, either by
   being explicitly set or by a built-in default. Anything else is an error,
   although it is not specifically tested in the template.
*/
void initialise_options(options *my_options)
{

  my_options->loglevel = 999999;
  my_options->portnum = 0;   /* invalid port - 1 is lowest IPv4 port */
  my_options->dnslookups = UNSET;
  my_options->foregroundonly = UNSET;
  memset(my_options->logfilename, 0, sizeof(my_options->logfilename));
  memset(my_options->configfilename, 0, sizeof(my_options->configfilename));
  my_options->maxchild = 0;
  my_options->dumpcore = UNSET;
  my_options->terminate = UNSET;
  my_options->checkcfg = UNSET;
} /* initialise_options */

void fill_default_options(options *my_options)
{
  my_options->loglevel = 1;
  my_options->portnum = 3000;    /* port to listen on */
  /* can be expensive, depends on what daemon does */
  my_options->dnslookups = FALSE;
  /* stay in foreground (but children still fork()ed) */
  my_options->foregroundonly = FALSE;
  strcpy(my_options->logfilename, "./logfile");
  /* this means three, not four clients maximum */
  my_options->maxchild = 3;
  /* PANIC routine dumps core rather than exit() */
  my_options->dumpcore = FALSE;
  /* kill running copy of myself, using lockfile pid */
  my_options->terminate = FALSE;
  /* check all config options then terminate */
  my_options->checkcfg = FALSE;
} /* fill_default_options */


/* Uses only ANSI getopt, no GNU extensions. Can't use PANIC since
   file descriptors haven't been set up yet */
void process_cmdline(int argc, char **argv)
{

  int fatal = FALSE;
  char ch;

  opterr = 0; /* ie all getopt error handling is done by this program */
  /* getopt now returns -1 not EOF, IEEE Std 1003.2-1992 (POSIX.2)*/
  while ((ch = getopt(argc, argv, "hFd:l:c:m:p:wkto")) != -1)
    switch (ch)
    {

    case '?':

      fprintf(stderr, "%s: unknown parameter %c\n", argv[0], (char)optopt);
      fatal = TRUE;
      break;

    case ':':

      fprintf(stderr, "%s: missing argument for %c\n", argv[0], (char)optopt);
      fatal = TRUE;
      break;

    case 'h':

      fatal = TRUE;
      break;

    case 'F':

      global_options->foregroundonly = TRUE;
      break;

    case 'd':

      if (optarg != NULL)
      {
        global_options->loglevel = atoi(optarg);
        if (global_options->loglevel < MIN_LOGLEVEL ||
            global_options->loglevel > MAX_LOGLEVEL)
        {
          printf("%d invalid, loglevel set to %d\n",
                 global_options->loglevel, MIN_LOGLEVEL);
          global_options->loglevel = MIN_LOGLEVEL;
        }
      }
      break;

    case 'l':

      if (optarg != NULL)
      {
        if (global_options->foregroundonly == TRUE)
        {
          printf("logfile not compatible with -F. Ignored\n");
        }
        else
        {
          strncpy(global_options->logfilename, optarg,
                  sizeof(global_options->logfilename));
        }
      }
      break;

    case 'c':

      if (optarg != NULL)
      {
        strncpy(global_options->configfilename, optarg,
                sizeof(global_options->configfilename));
      }
      break;

    case 'm':

      if (optarg != NULL)
      {
        global_options->maxchild = (unsigned)atoi(optarg);
      }
      break;

    case 'p':

      if (optarg != NULL)
      {
        global_options->portnum = (unsigned)atoi(optarg);
      }
      break;

    case 'w':

      global_options->dnslookups = TRUE;
      break;

    case 'k':

      global_options->dumpcore = TRUE;
      break;

    case 't':

      global_options->terminate = TRUE;
      break;

    case 'o':

      global_options->checkcfg = TRUE;
      global_options->foregroundonly = TRUE;
      if (global_options->loglevel != MAX_LOGLEVEL)
      {
        printf("-o given. loglevel set to %d and -l ignored\n",
               MAX_LOGLEVEL);
      }
      global_options->loglevel = MAX_LOGLEVEL;
      break;

    default:
      PANIC(("Fell through getopt() switch statement!\n"));

    }

  if (fatal == TRUE)
  {

    /* UNFEATURE should derive defaults from some global macros, because
       there is common data here with other parts of confdata.c */
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "       -h        help message\n");
    fprintf(stderr, "       -F        foreground-only, & force log to stdout"
            " (defaults to background)\n");
    fprintf(stderr, "       -d n      set debugging log level n (maximum 9,"
            " default 1)\n");
    fprintf(stderr, "       -p n      listen on TCP port n (1-65535, default"
            " 3000)\n");
    fprintf(stderr, "       -l filename  set logfile to fname (default"
            " \"logfile\")\n");
    fprintf(stderr, "       -c filename  set configuration file to fname"
            " (no default, ie \"\")\n");
    fprintf(stderr, "       -m n      set maximum number of child processes,"
            " default %d\n", ABSOLUTE_MAX_CHILDREN);
    fprintf(stderr, "       -w        write reverse dnslookups for incoming"
            " clients to logfile\n");
    fprintf(stderr, "       -k        dump core on panic rather than exit"
            " with error\n");
    fprintf(stderr, "       -t        terminate running copy of daemon\n");
    fprintf(stderr, "       -o        check config options & exit. Also sets"
            " -F and -d %d\n", MAX_LOGLEVEL);
    exit(EXIT_FAILURE);

  }

} /* process_cmdline */

/* Returns non-whitespace chars to left of '=' sign in a line, NULL for
   blank line or comment. Logs specific errors. */
static char* extract_option(const char *lp, const char *filename, int linenum)
{
  char* ret = NULL;
  size_t lw = 0; /* whitespace len */
  size_t lo = 0; /* option len */

  if (lp == NULL)
    return NULL;

  lw = strspn(lp, WHITESPACE);
  /* LOG(9,("lp=%s\n",lp)); */
  /* LOG(9,("lw=%d\n",lw)); */

  if (*(lp + lw) == '#')
  {
    LOG(9, ("%s: line %d: comment.\n", filename, linenum));
    return NULL;
  }

  if (strlen(lp) == 0)
  {
    LOG(9, ("%s: line %d: blank line.\n", filename, linenum));
    return NULL;
  }

  if (strchr(lp, '=') == NULL)
  {
    LOG(1, ("%s: line %d: no '=' present in option.\n",
            filename, linenum));
    return NULL;
  }

  lo = strcspn(lp + lw, WHITESPACE "=");
  /* LOG(9,("lo=%d\n",lo)); */
  ret = (char *)malloc(strlen(lp) + 1);
  memset(ret, 0, strlen(lp) + 1);
  if (ret == NULL)
  {
    PANIC(("Out of memory in extract_option() !\n"));
  }
  strncat(ret, (lp + lw), lo);

  return ret;

} /* extract_option */

/* Returns the opcode of the token pointed at by cp, or oBadoption. */
static confoptions
parse_option(const char *cp)
{
  unsigned i;
  confoptions ret = oBadoption;

  if ((cp == NULL) || (strlen(cp) == 0))
  {
    return ret;
  }

  for (i = 0; keywords[i].name; i++)
  {
    if (strcasecmp(cp, keywords[i].name) == 0)
    {
      ret = keywords[i].opcode;
    }
  }

  return ret;
} /* parse_option */

/* returns the expression to the right of the '=' sign in a line from the
   config file, stripping whitespace from beginning and end. This is where
   clever interpretive stuff can go one day. Warning: doesn't yet handle
   quoted strings. The expression is regarded as delimited by whitespace */
static char* extract_expr(const char *lp, const char *filename, int linenum)
{
  char* ret = NULL;
  char* es = 0; /* expression start */
  size_t el = 0; /* expression len */
  size_t sk = 0; /* skip len */

  if (lp == NULL)
    return NULL;

  es = strchr(lp, '=') + 1;

  if (es == NULL)
  {
    LOG(1, ("%s: line %d: no '=' present in expression.\n",
            filename, linenum));
    return NULL;
  }

  /* expr may contain '=', that's quite ok */
  sk = strspn(es, WHITESPACE);
  /* LOG(9,("sk=%d\n",sk)); */
  el = strcspn(es + sk, WHITESPACE);
  /* LOG(9,("el=%d\n",el)); */
  ret = (char *)malloc(strlen(lp) + 1);
  if (ret == NULL)
  {
    PANIC(("Out of memory in extract_expr() !\n"));
  }
  memset(ret, 0, strlen(lp) + 1);
  strncat(ret, es + sk, el);
  /* LOG(9,("ret=%s\n",ret)); */

  if (strlen(ret) == 0)
  {
    free(ret);
    ret = NULL;
  }

  return ret;

} /* extract_expr */

/* parses a string into an integer, putting the result in target and
   returning TRUE on success, and logging the results as we go */
int parseint(confoptions opcode, const char *expr, const int min,
             const int max, const char *fn, const int linenum,
             int* target)
{
  int s = FALSE;
  int i = 0;

  if (strspn(expr, INT_BASE10) > 0)
  {
    /* NB atoi doesn't do any error checking */
    i = atoi(expr);
    if ((i >= min) && (i <= max))
    {
      LOG(9, ("%s: line %d %s=%d\n", fn, linenum, keywords[opcode].name,
              i));
      s = TRUE;
      *target = i;
    }
    else
    {
      LOG(9, ("%s: line %d %s='%s': not integer or not >%d & <%d\n", fn, linenum,
              keywords[opcode].name, expr, min, max));
    }
  }
  return s;
} /* parseint */

/* parses a string into a boolean integer, putting the result in target
   and returning TRUE on success, and logging the results as we go */
int parsebool(confoptions opcode, const char *expr, const char *fn,
              const int linenum, int* target)
{
  int s = FALSE;

  /* handle in separate blocks to cope with whatever weird booleans come
     along in the future */
  if ((strncasecmp(expr, "TRUE", 4) == 0) || (strncasecmp(expr, "YES", 3) == 0))
  {
    LOG(1, ("%s: line %d %s=%s\n", fn, linenum, keywords[opcode].name,
            "TRUE"));
    *target = TRUE;
    s = TRUE;
  }

  if ((strncasecmp(expr, "FALSE", 5) == 0) || (strncasecmp(expr, "NO", 2) == 0))
  {
    LOG(1, ("%s: line %d %s=%s\n", fn, linenum, keywords[opcode].name,
            "FALSE"));
    *target = FALSE;
    s = TRUE;
  }
  if (s == FALSE)
  {
    LOG(9, ("%s: line %d bad boolean expression '%s' for %s\n", fn, linenum,
            expr, keywords[opcode].name));
  }

  return s;

} /* parsebool */

/* parses a string into a validated string, putting the result in target
   and returning TRUE on success, and logging the results as we go. */
int parsestring(confoptions opcode, const char *expr, const int len,
                const char *fn, const int linenum, char* target)
{
  int s = FALSE;

  /* note: whitespace already stripped by extract_expr */
  /* note: extract_expr can't cope with quoted strings; everything delimited
           by whitespace */

  if ((strlen(expr) > len) || (strlen(expr) == 0))
  {
    LOG(1, ("%s: line %d invalid string for option %s\n", fn, linenum,
            keywords[opcode].name));
  }
  else
  {
    LOG(1, ("%s: line %d: valid string '%s' for option %s\n", fn, linenum,
            expr, keywords[opcode].name));
    strncpy(target, expr, strlen(expr) + 1);
    s = TRUE;
  }

  return s;

} /* parsestring */

/* The config file is processed strictly one line at a time, where a
   line must be less than CONFIG_FILE_LINELEN characters long including the \n.
   The \n is mandatory, except for the last line in the file.
 */
int process_configfile(options *my_options)
{
  unsigned ret = -1;
  char line[CONFIG_FILE_LINELEN];
  char *lp = NULL;
  char *expr = NULL;
  char *myoption = NULL;
  char *fn = NULL; /*global filename. saves typing*/
  int linenum = 0;
  int l = 0;
  confoptions opcode = oBadoption;
  int s = FALSE; /* status of parse_xx() calls */

  memset(line, 0, CONFIG_FILE_LINELEN);

  conffile = fopen(global_options->configfilename, "r");
  if (conffile == NULL)
  {
    /* Sometimes this might be better to just LOG and carry on */
    PANIC(("Config file \"%s\" failed open with error %s\n",
           global_options->configfilename, strerror(errno)));
  }

  fn = global_options->configfilename;
  LOG(9, ("Listing config file lines including literal '\\n'\n"));
  while ( (lp = fgets(line, CONFIG_FILE_LINELEN, conffile)))
  {

    linenum++;
    lp = line;

    if ((strchr(line, '\n') == NULL))
    {
      if (!feof(conffile))
      {
        LOG(1, ("%s: line %d too long, skipping\n", fn, linenum));
        lp = fgets(line, CONFIG_FILE_LINELEN, conffile);
        while ((strchr(line, '\n') == NULL) && !feof(conffile))
        {
          /* chomp */
          lp = fgets(line, CONFIG_FILE_LINELEN, conffile);
        }
        continue;
      }
      else
      {
        if (strlen(line) < CONFIG_FILE_LINELEN - 2)
        {
          l = strlen(line);
          *(line + l) = '\n';
          *(line + l + 1) = '\000';
          LOG(9, ("%s: line %d: added missing '\\n' to last line\n",
                  fn, linenum));
        }
        else
        {
          LOG(1, ("%s: last line (%d) too long to insert missing '\\n'\n",
                  fn, linenum));
        }
      }
    }

    /* Don't log line contents because it leads to a very distracting logfile.
       Uncomment for debugging. */
    /* LOG(9,("line %d: %s",linenum,lp)) */

    myoption = extract_option(lp, fn, linenum);
    if (myoption == NULL)
    {
      /* Silently skip, because error already reported. Uncomment to debug */
      /* LOG(9,("%s: line %d bad option, skipping\n",fn,linenum)); */
      continue;
    }

    /* LOG(9,("read option '%s', about to parse\n",myoption)); */

    opcode = parse_option(myoption);
    if (opcode == oBadoption)
    {
      LOG(1, ("%s: line %d: skipping bad or unknown option '%s'\n",
              fn, linenum, myoption));
      continue;
    }

    expr = extract_expr(lp, fn, linenum);
    if (expr == NULL)
    {
      LOG(1, ("%s: line %d bad expression, skipping\n",
              fn, linenum));
      continue;
    }

    /* LOG(9,("just read expr '%s', not yet parsed\n",expr)); */

    switch (opcode)
    {

    case oLoglevel:
      s = parseint(opcode, expr, 1, 9, fn, linenum, (int*) & global_options->loglevel);

      if (global_options->checkcfg == TRUE)
      {
        global_options->loglevel = MAX_LOGLEVEL;
        LOG(9, ("%s: line %d: loglevel ignored due to checkconfig (-o). "
                "Set to %d\n", fn, linenum, MAX_LOGLEVEL));
        /* Overwrite so we don't make a mess of the special -o option. */
        /* The problem is that checkcfg relies on everything being */
        /* reported at MAX_LOGLEVEL, and changing that partway through */
        /* stops checkcfg from working. */
      }
      break;

    case oPortnum:

      s = parseint(opcode, expr, 1, 65535, fn, linenum, (int *) & global_options->portnum);
      break;

    case oDnslookups:

      s = parsebool(opcode, expr, fn, linenum, (int*) & global_options->dnslookups);
      break;

    case oForegroundonly:

      s = parsebool(opcode, expr, fn, linenum, (int*) & global_options->foregroundonly);
      break;

    case oLogfilename:

      s = parsestring(opcode, expr, FILENAME_LEN, fn, linenum,
                      (char *) & global_options->logfilename);
      break;

    case oConfigfilename:

      LOG(1, ("%s: line %d: May not set configfilename from config file\n",
              fn, linenum));
      break;

    case oMaxchild:

      s = parseint(opcode, expr, 1, ABSOLUTE_MAX_CHILDREN, fn, linenum,
                   (int*) & global_options->maxchild);
      break;

    case oDumpcore:

      s = parsebool(opcode, expr, fn, linenum, (int*) & global_options->dumpcore);
      break;

    case oTerminate:

      s = parsebool(opcode, expr, fn, linenum, (int*) & global_options->terminate);
      break;

    case oCheckcfg:

      LOG(1, ("%s: line %d: May not set checkcfg from config file\n",
              fn, linenum));
      break;

    default:
      PANIC(("Fell through process_config_file() switch statement!\n"));

    } /* option handling */
    free(expr);
  }

  fclose(conffile);
  return ret;

} /* process_configfile */

void log_option_status()
{

  LOG(9, ("loglevel = %i\n", global_options->loglevel));
  LOG(9, ("foregroundonly = %s\n",
          (global_options->foregroundonly == TRUE) ? "TRUE" : "FALSE"));
  LOG(9, ("logfilename = \"%s\"\n", global_options->logfilename));
  LOG(9, ("configfilename = \"%s\"\n",
          (strlen(global_options->configfilename) == 0) ? "NULL - not set" :
          global_options->configfilename));
  LOG(9, ("maxchild = %i\n", global_options->maxchild));
  LOG(9, ("portnum = %i\n", global_options->portnum));
  LOG(9, ("dnslookups = %s\n",
          (global_options->dnslookups == TRUE) ? "TRUE" : "FALSE"));
  LOG(9, ("dumpcore = %s\n", (global_options->dumpcore == TRUE) ? "TRUE" : "FALSE"));
  LOG(9, ("terminate = %s\n", (global_options->terminate == TRUE) ? "TRUE" : "FALSE"));
  LOG(9, ("checkcfg = %s\n", (global_options->checkcfg == TRUE) ? "TRUE" : "FALSE"));

} /* log_option_status */

