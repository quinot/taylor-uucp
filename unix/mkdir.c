/* mkdir.c
   Create a directory.  We must go through a subsidiary program to
   force our real uid to be the uucp owner before invoking the setuid
   /bin/mkdir program.  */

#include "uucp.h"
#include "sysdep.h"

#include <errno.h>

int
mkdir (zdir, imode)
     const char *zdir;
     int imode;
{
  const char *azargs[3];
  int aidescs[3];
  pid_t ipid;

  /* /bin/mkdir will create the directory with mode 777, so we set our
     umask to get the mode we want.  */
  (void) umask ((~ imode) & (S_IRWXU | S_IRWXG | S_IRWXO));

  azargs[0] = UUDIR_PROGRAM;
  azargs[1] = zdir;
  azargs[2] = NULL;
  aidescs[0] = SPAWN_NULL;
  aidescs[1] = SPAWN_NULL;
  aidescs[2] = SPAWN_NULL;

  ipid = isspawn (azargs, aidescs, FALSE, FALSE, (const char *) NULL,
		  TRUE, FALSE, (const char *) NULL,
		  (const char *) NULL, (const char *) NULL);

  (void) umask (0);

  if (ipid < 0)
    return -1;

  if (iswait ((unsigned long) ipid, (const char *) NULL) != 0)
    {
      /* Make up an errno value.  */
      errno = EACCES;
      return -1;
    }

  return 0;
}
