/* splnam.c
   Get the full name of a file in the spool directory.  */

#include "uucp.h"

#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

/* Get the real name of a spool file.  */

char *
zsysdep_spool_file_name (qsys, zfile, flocal)
     const struct uuconf_system *qsys;
     const char *zfile;
     boolean flocal;
{
  return zsfind_file (zfile, qsys->uuconf_zname, flocal);
}
