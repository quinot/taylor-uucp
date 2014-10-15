/* sync.c
   Sync a file to disk, if FSYNC_ON_CLOSE is set.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

boolean
fsysdep_sync (openfile_t e, const char *zmsg)
{
#if FSYNC_ON_CLOSE
  int o;
#endif

#if USE_STDIO
  if (fflush (e) == EOF)
    {
      ulog (LOG_ERROR, "%s: fflush: %s", zmsg, strerror (errno));
      return FALSE;
    }
#endif

#if FSYNC_ON_CLOSE
#if USE_STDIO
  o = fileno (e);
#else
  o = e;
#endif

  if (fsync (o) < 0)
    {
      ulog (LOG_ERROR, "%s: fsync: %s", zmsg, strerror (errno));
      return FALSE;
    }
#endif

  return TRUE;
}
