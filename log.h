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

/* log.h

*/

/* prototypes */

void panic(const char* f, ...);
void log_start();
void log_finish();
void log_msg(const char* f, ...);

#define LOG(level, body) \
if (global_options->loglevel >= level) log_msg body;

#define TO_STRING(s) XTO_STRING(s)
#define XTO_STRING(s) #s

extern char GLOBAL_LINE[255];  /* For PANIC macro. ANSI C macros do not allow */
extern char GLOBAL_FILE[255];  /* variable arguments (...), unlike gcc. */

/* PANIC is more elegant with gcc macro extensions. But we only use ANSI C */
#define PANIC(args) \
strncpy(GLOBAL_FILE, __FILE__,200); \
strncpy(GLOBAL_LINE,TO_STRING(__LINE__),200); \
panic args; \
strncpy(GLOBAL_FILE,"",1); \
strncpy(GLOBAL_LINE,"",1); /* to catch people who call panic() directly */


