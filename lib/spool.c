/* spool.c
   See whether a filename is legal for the spool directory.  */

#include "uucp.h"

#include <ctype.h>

/* See whether a file is a spool file.  Spool file names are specially
   crafted to hand around to other UUCP packages.  They always begin
   with 'C', 'D' or 'X', and the second character is always a period.
   The remaining characters are any character that could appear in a
   system name.  */

boolean
fspool_file (zfile)
     const char *zfile;
{
  const char *z;

  if (*zfile != 'C' && *zfile != 'D' && *zfile != 'X')
    return FALSE;
  if (zfile[1] != '.')
    return FALSE;
  for (z = zfile + 2; *z != '\0'; z++)
    if (! isalnum (BUCHAR (*z)) && *z != '_' && *z != '-' && *z != '.')
      return FALSE;
  return TRUE;
}
