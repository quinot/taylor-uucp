/* sleep.c
   Sleep for a number of seconds.  */

#include "uucp.h"

#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "system.h"

void
usysdep_sleep (c)
     int c;
{
  (void) sleep (c);
}
