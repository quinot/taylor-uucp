/* callin.c
   Check a login name and password against the UUCP password file.

   Copyright (C) 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_callin_rcsid[] = "$Id$";
#endif

#include <errno.h>

static int ipcheck P((pointer pglobal, int argc, char **argv,
		      pointer pvar, pointer pinfo));

struct sinfo
{
  size_t (*pfn) P((char *));
  const char *zlogin;
  char *zfilepass;
};

/* Check a login name and password against the UUCP password file.
   This looks at the Taylor UUCP password file, but will work even if
   uuconf_taylor_init was not called.  */

int
uuconf_callin (pglobal, zlogin, zpassword, pfn)
     pointer pglobal;
     const char *zlogin;
     const char *zpassword;
     size_t (*pfn) P((char *));
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  char **pz;
  struct uuconf_cmdtab as[1];
  struct sinfo s;

  /* If we have no password file names, fill in the default name.  */
  if (qglobal->qprocess->pzpwdfiles == NULL)
    {
      char ab[sizeof NEWCONFIGLIB + sizeof PASSWDFILE - 1];

      memcpy ((pointer) ab, (pointer) NEWCONFIGLIB,
	      sizeof NEWCONFIGLIB - 1);
      memcpy ((pointer) (ab + sizeof NEWCONFIGLIB - 1), (pointer) PASSWDFILE,
	      sizeof PASSWDFILE);
      iret = _uuconf_iadd_string (qglobal, ab, TRUE, FALSE,
				  &qglobal->qprocess->pzpwdfiles,
				  qglobal->pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  as[0].uuconf_zcmd = NULL;

  s.pfn = pfn;
  s.zlogin = zlogin;
  s.zfilepass = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzpwdfiles; *pz != NULL; pz++)
    {
      FILE *e;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      iret = uuconf_cmd_file (pglobal, e, as, (pointer) &s,
			      ipcheck, 0, (pointer) NULL);
      (void) fclose (e);

      if (iret != UUCONF_SUCCESS || s.zfilepass != NULL)
	break;
    }

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      iret |= UUCONF_ERROR_FILENAME;
    }
  else if (s.zfilepass == NULL)
    iret = UUCONF_NOT_FOUND;
  else
    {
      size_t clen;

      if (pfn == NULL)
	clen = strlen (s.zfilepass);
      else
	clen = (*pfn) (s.zfilepass);
      if (strncmp (s.zfilepass, zpassword, clen) != 0
	  || zpassword[clen] != '\0')
	iret = UUCONF_NOT_FOUND;
    }

  if (s.zfilepass != NULL)
    free ((pointer) s.zfilepass);

  return iret;
}

/* This is called on each line of the file.  It transforms the login
   name from the file to see if it is the one we are looking for.  If
   it is, it sets pinfo->zfilepass to the password.  */

static int
ipcheck (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  struct sinfo *q = (struct sinfo *) pinfo;
  char *z;
  size_t clen;

  if (argc != 2)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  z = strdup (argv[0]);
  if (z == NULL)
    {
      qglobal->ierrno = errno;
      return (UUCONF_MALLOC_FAILED
	      | UUCONF_ERROR_ERRNO
	      | UUCONF_CMDTABRET_EXIT);
    }
  if (q->pfn == NULL)
    clen = strlen (z);
  else
    clen = (*q->pfn) (z);
  if (strncmp (z, q->zlogin, clen) == 0
      && q->zlogin[clen] == '\0')
    {
      free ((pointer) z);
      q->zfilepass = strdup (argv[1]);
      if (q->zfilepass == NULL)
	{
	  qglobal->ierrno = errno;
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}
      return UUCONF_CMDTABRET_EXIT;
    }

  free ((pointer) z);

  return UUCONF_CMDTABRET_CONTINUE;
}
