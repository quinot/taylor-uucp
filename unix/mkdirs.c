/* mkdirs.c
   Create any directories needed for a file name.  */

#include "uucp.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

boolean
fsysdep_make_dirs (zfile, fpublic)
     const char *zfile;
     boolean fpublic;
{
  char *zcopy, *z;
  int imode;

  zcopy = (char *) alloca (strlen (zfile) + 1);
  strcpy (zcopy, zfile);

  if (fpublic)
    imode = IPUBLIC_DIRECTORY_MODE;
  else
    imode = IDIRECTORY_MODE;

  for (z = zcopy; *z != '\0'; z++)
    {
      if (*z == '/' && z != zcopy)
	{
	  *z = '\0';
	  if (! fsysdep_directory (zcopy))
	    {
	      if (mkdir (zcopy, imode) != 0)
		{
		  ulog (LOG_ERROR, "mkdir (%s): %s", zcopy,
			strerror (errno));
		  return FALSE;
		}
	    }
	  *z = '/';
	}
    }

  return TRUE;
}
