/* sync.c
   Sync a file to disk, if FSYNC_ON_CLOSE is set.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

boolean
fsysdep_sync (e)
     openfile_t e;
{
  int o;

#if USE_STDIO
  if (fflush (e) == EOF)
    {
      ulog (LOG_ERROR, "fflush: %s", strerror (errno));
      return FALSE;
    }
#endif

#if USE_STDIO
  o = fileno (e);
#else
  o = e;
#endif

#if FSYNC_ON_CLOSE
  if (fsync (o) < 0)
    {
      ulog (LOG_ERROR, "fsync: %s", strerror (errno));
      return FALSE;
    }
#endif

  return TRUE;
}
