/* addbas.c
   If we have a directory, add in a base name.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* If we have a directory, add a base name.  */

char *
zsysdep_add_base (zfile, zname)
     const char *zfile;
     const char *zname;
{
  size_t clen;
  const char *zlook;

#if DEBUG > 0
  if (*zfile != '/')
    ulog (LOG_FATAL, "zsysdep_add_base: %s: Can't happen", zfile);
#endif

  clen = strlen (zfile);

  if (zfile[clen - 1] != '/')
    {
      if (! fsysdep_directory (zfile))
	return zbufcpy (zfile);
    }
  else
    {
      char *zcopy;

      /* Trim out the trailing '/'.  */
      zcopy = (char *) alloca (clen);
      memcpy (zcopy, zfile, clen - 1);
      zcopy[clen - 1] = '\0';
      zfile = zcopy;
    }

  zlook = strrchr (zname, '/');
  if (zlook != NULL)
    zname = zlook + 1;

  return zsysdep_in_dir (zfile, zname);
}
