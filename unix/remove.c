/* remove.c
   Remove a file (Unix specific implementation).  */

#include "uucp.h"

#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif

int
remove (z)
     const char *z;
{
  return unlink (z);
}
