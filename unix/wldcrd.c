/* wldcrd.c
   Expand wildcards.

   Copyright (C) 1991, 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Local variables to hold the expanded wildcard string.  */

static char *zSwildcard_alloc;
static char *zSwildcard;

/* Start getting a wildcarded file spec.  We use the shell to expand
   the wildcard.  */

boolean
fsysdep_wildcard_start (zfile)
     const char *zfile;
{
  char *zcmd;
  size_t c;
  const char *azargs[4];
  FILE *e;
  pid_t ipid;

#if DEBUG > 0
  if (*zfile != '/')
    ulog (LOG_FATAL, "fsysdep_wildcard: %s: Can't happen", zfile);
#endif

  zSwildcard_alloc = NULL;
  zSwildcard = NULL;

  zcmd = (char *) alloca (sizeof ECHO_PROGRAM + sizeof " " + strlen (zfile));
  sprintf (zcmd, "%s %s", ECHO_PROGRAM, zfile);

  azargs[0] = "/bin/sh";
  azargs[1] = "-c";
  azargs[2] = zcmd;
  azargs[3] = NULL;

  e = espopen (azargs, TRUE, &ipid);
  if (e == NULL)
    {
      ulog (LOG_ERROR, "espopen: %s", strerror (errno));
      return FALSE;
    }

  zSwildcard_alloc = NULL;
  c = 0;
  if (getline (&zSwildcard_alloc, &c, e) <= 0)
    {
      xfree ((pointer) zSwildcard_alloc);
      zSwildcard_alloc = NULL;
    }

  if (iswait ((unsigned long) ipid, ECHO_PROGRAM) != 0)
    {
      xfree ((pointer) zSwildcard_alloc);
      return FALSE;
    }

  if (zSwildcard_alloc == NULL)
    return FALSE;

  DEBUG_MESSAGE1 (DEBUG_EXECUTE,
		  "fsysdep_wildcard_start: got \"%s\"",
		  zSwildcard_alloc);

  zSwildcard = zSwildcard_alloc;

  return TRUE;
}

/* Get the next wildcard spec.  */

/*ARGSUSED*/
char *
zsysdep_wildcard (zfile)
     const char *zfile;
{
  char *zret;

  if (zSwildcard_alloc == NULL || zSwildcard == NULL)
    return NULL;

  zret = zSwildcard;

  while (*zSwildcard != '\0' && ! isspace (BUCHAR (*zSwildcard)))
    ++zSwildcard;

  if (*zSwildcard == '\0')
    zSwildcard = NULL;
  else
    {
      *zSwildcard = '\0';
      ++zSwildcard;
      while (*zSwildcard != '\0' && isspace (BUCHAR (*zSwildcard)))
	++zSwildcard;
      if (*zSwildcard == '\0')
	zSwildcard = NULL;
    }

  return zbufcpy (zret);
}

/* Finish up getting wildcard specs.  */

boolean
fsysdep_wildcard_end ()
{
  xfree ((pointer) zSwildcard_alloc);
  zSwildcard_alloc = NULL;
  zSwildcard = NULL;
  return TRUE;
}
