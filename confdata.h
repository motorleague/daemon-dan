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

/* confdata.h

   sets the global options structure to values which indicate that they have
   not been set yet. Every option must have this value cleared, either by
   being explicitly set or by a built-in default. Anything else is an error.
*/
void initialise_options(options *my_options);

/* returns a string formatted for screen output listing all commandline
   options. This is the place to edit the help message. */
char * cmdline_message();

void process_cmdline(int argc, char **argv);

int process_configfile(options *my_options);

void fill_default_options(options *my_options);

void log_option_status();















