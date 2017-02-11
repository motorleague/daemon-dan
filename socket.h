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

/* socket.h

*/

#include <netinet/in.h>

int init_socket(struct sockaddr_in *sin);
int filtered_accept(int s, struct sockaddr *addr, socklen_t *addrlen);





