/* memcpy.c
   Copy one memory buffer to another.  */

#include "uucp.h"

pointer
memcpy (ptoarg, pfromarg, c)
     pointer ptoarg;
     constpointer pfromarg;
     int c;
{
  char *pto = (char *) ptoarg;
  const char *pfrom = (const char *) pfromarg;

  while (c-- != 0)
    *pto++ = *pfrom++;
  return ptoarg;
}
