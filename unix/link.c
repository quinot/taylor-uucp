/* link.c
   Link two files.  */

#include "uucp.h"

#include <errno.h>

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

boolean
fsysdep_link (zfrom, zto, pfworked)
     const char *zfrom;
     const char *zto;
     boolean *pfworked;
{
  if (link (zfrom, zto) == 0)
    {
      *pfworked = TRUE;
      return TRUE;
    }
  *pfworked = FALSE;
  if (errno == EXDEV)
    return TRUE;
  ulog (LOG_ERROR, "link (%s, %s): %s", zfrom, zto, strerror (errno));
  return FALSE;
}
