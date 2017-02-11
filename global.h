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

/* global.h

Keep this small as poss.

*/

#define CYGWIN 0
#define UNIX 1

#define TRUE  0
#define FALSE 1
#define UNSET 2

#define FILENAME_LEN 255          /* varies quite a bit across OSs */
#define MAXHOSTLEN 254
#define ABSOLUTE_MAX_CHILDREN 200 /* for array declaration. maxchild is a */
/* variable, and usually much lower than this */

#define MIN_LOGLEVEL 1
#define MAX_LOGLEVEL 9

#define GLOBAL_LOCKFILE_NAME "/tmp/daevel.pid"
/* UNFEATURE - make it configurable, and */
/* also portable to funny OSs */

extern int logfile;         /* file descriptors */

/* This holds values for all commandline and config file options, or
   their defaults. Allocated in main. */
typedef struct
{

  unsigned loglevel;
  unsigned portnum;  /* server port to listen on */
  unsigned dnslookups; /* can be expensive; depends on what daemon does */
  unsigned foregroundonly; /* don't background daemon (but children fork off)*/
  char logfilename[FILENAME_LEN];
  char configfilename[FILENAME_LEN];
  unsigned maxchild; /* maximum number of clients, starting from 1 */
  unsigned dumpcore; /* PANIC routine dumps core rather than exit() */
  unsigned terminate; /* kill running copy of myself, using lockfile pid */
  unsigned checkcfg; /* run all configuration logic, then exit */
}
options;
options* global_options;

extern unsigned lock_acquired;

/* used in both util.c and confdata.c (and modified in confdata.c) */
#define WHITESPACE " \n\r\t"

#define INT_BASE10 "1234567890+-"

#define CONFIG_FILE_LINELEN 255

