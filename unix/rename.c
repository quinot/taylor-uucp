/* rename.c
   Rename a file to a new name (Unix specific implementation).  */

#include "uucp.h"

#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

/* This implementation will not work on directories, but fortunately
   we never want to rename directories.  */

int
rename (zfrom, zto)
     const char *zfrom;
     const char *zto;
{
  if (link (zfrom, zto) < 0)
    {
      if (errno != EEXIST)
	return -1;
      if (unlink (zto) < 0
	  || link (zfrom, zto) < 0)
	return -1;
    }
  return unlink (zfrom);
}
