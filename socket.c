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

/* socket.c

   Socket handling functions.

*/

#include <sys/socket.h>
#include <arpa/inet.h>   /* ntoa etc */
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#include "global.h"
#include "socket.h"
#include "util.h"
#include "log.h"

/* Set up a listening socket. returns -1 for failure, positive socket
   descriptor for success */
int init_socket(struct sockaddr_in *sin)
{
  struct hostent *h;
  char hostname[MAXHOSTLEN];
  int sockdes = -1;
  int ret = -1;

  if (sin == NULL)
  {
    return -1;
  }

  if (gethostname(hostname, MAXHOSTLEN) == -1)
    /* UNFEATURE should be separate function that deals with systems returning
       results in strange case combinations */
  {
    LOG(1, ("gethostname failed\n"));
    return -1;
  }

  /* get host info */
  if (global_options->dnslookups == TRUE)
  {
    if ((h = gethostbyname(hostname)) == 0)
    {
      LOG(1, ("gethostbyname: Unknown host %s\n", hostname));
      return -1;
    }
  }

  memset(sin, 0, sizeof(sin));

  sin->sin_port = htons(global_options->portnum);
  sin->sin_family = AF_INET; /* pedantically, could be h->addrtype */
  sin->sin_addr.s_addr = 0;
  /* UNFEATURE should allow for binding to other or multiple addresses */

  sockdes = socket(sin->sin_family, SOCK_STREAM, 0);
  if (sockdes == -1)
  {
    LOG(1, ("failed to allocate socket\n"));
    return -1;
  }

  ret = bind( sockdes, (struct sockaddr*)sin, sizeof(struct sockaddr) );
  if (ret == -1)
  {
    LOG(1, ("Error %d binding to socket: %s\n", errno, strerror(errno)));
    return -1;
  }

  ret = listen(sockdes, 5);
  if (ret == -1)
  {
    LOG(1, ("listen failed with error %d, %s\n", errno, strerror(errno)));
    return -1;
  }

  LOG(1, ("%s listening on port %i\n", hostname, global_options->portnum));
  return sockdes;

} /*init_socket*/


/* accept() returns lots of errors. This squashes a range of errors
   which experience says we can ignore into EAGAIN. Some valid but rare
   inputs to accept() are rejected as errors, because they probably will
   be. If you don't like the cautious approach, change the code.
*/
int filtered_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{

  int ret = -1;
  int ret2 = -1;

  /* list of errors we can safely ignore. Postfix does this, and the man
     page for Linux accept(2) recommends it too, with an explanation of
     why it differs from other BSD socket implementations (such as BSD:)
  */

  static int squash_errors[] =
  {
    ECONNREFUSED,
    ECONNRESET,
    EHOSTDOWN,
    EHOSTUNREACH,
    EINTR,
    ENETDOWN,
    ENETUNREACH,
    ENOTCONN,
    EWOULDBLOCK
  };

  if ( (s == 0) || \
       (addr == NULL) || \
       (addrlen == NULL) )
  {
    PANIC(("bad parameter passed to filtered_accept\n"));
  }

  ret = accept(s, addr, addrlen);
  if (ret == -1)
  {
    ret2 = INT_ISSET(squash_errors, errno);
    if ( ret2 > -1 )
    {
      LOG(9, ("Squashed error %i into EAGAIN\n", errno));
      errno = EAGAIN;
    }
  }

  return ret;

} /* filtered_accept */
