/* isfork.c 
   Retry fork several times before giving up.  */

#include "uucp.h"

#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

#include "sysdep.h"

pid_t
isfork ()
{
  int i;
  pid_t iret;

  for (i = 0; i < 10; i++)
    {
      iret = fork ();
      if (iret >= 0 || errno != EAGAIN)
	return iret;
      sleep (5);
    }

  return iret;
}