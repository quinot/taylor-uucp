/* rename.c
   Rename a file to a new name (Unix specific implementation).  */

#include "uucp.h"

#if USE_STDIO && HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* This implementation will not work on directories, but fortunately
   we never want to rename directories.  */

int
rename (zfrom, zto)
     const char *zfrom;
     const char *zto;
{
  /* Try to make the link without removing the old file first.  */
  if (link (zfrom, zto) < 0)
    {
      struct stat sfrom, sto;

      if (errno != EEXIST)
	return -1;

      if (strcmp (zfrom, zto) == 0)
	return 0;
      if (stat (zfrom, &sfrom) < 0
	  || stat (zto, &sto) < 0)
	return -1;
      if (sfrom.st_ino == sto.st_ino
	  && sfrom.st_dev == sto.st_dev)
	return 0;

      if (unlink (zto) < 0
	  || link (zfrom, zto) < 0)
	return -1;
    }

  return unlink (zfrom);
}
