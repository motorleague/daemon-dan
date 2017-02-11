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

/* util.h

*/

/* prototypes */
int close_all(int startfd);
int int_isset(int *iset, int target, int num);
int become_daemon();

/* Returns first offset for int target == iset[offset], -1 for no match.*/
/* This is styled after FD_ISSET */

#define INT_ISSET(iset, target) int_isset((int *)&iset, target, \
sizeof(iset)/sizeof(int))

