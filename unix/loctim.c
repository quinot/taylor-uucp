/* loctim.c
   Turn a time epoch into a struct tm.  This is trivial on Unix.  */

#include "uucp.h"

#if TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "system.h"

void
usysdep_localtime (long int itime, struct tm *q)
{
  time_t i;

  i = (time_t) itime;
  *q = *localtime (&i);
}
