/* log.c
   Routines to add entries to the log files.

   Copyright (C) 1991 Ian Lance Taylor

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.

   $Log$
   */

#include "uucp.h"

#if USE_RCS_ID
char log_rcsid[] = "$Id$";
#endif

#include <stdio.h>
#include <string.h>
#include <signal.h>

#ifdef __STDC__
#include <stdarg.h>
#endif

#if HAVE_TIME_H
#include <time.h>
#endif

#if ! HAVE_TIME_T
#if HAVE_SYS_TIME_T
#include <sys/types.h>
#endif /* HAVE_SYS_TIME_T */
#endif /* ! HAVE_TIME_T */

#include "system.h"

/* The program name.  */
static const char *zLprogram;

/* ID number.  */
static int iLid;

/* The current user name.  */
static char *zLuser;

/* The current system name.  */
static char *zLsystem;

/* The open log file.  */
static FILE *eLlog;

#if DEBUG > 0
/* The open debugging file.  */
static FILE *eLdebug;
#endif

/* Whether we are aborting because of LOG_FATAL.  */
boolean fAborting;

/* Set the program we are making log entries for.  The argument should
   be a string constant.  Any error messages reported before
   ulog_program is called are sent to stderr.  */

void
ulog_program (z)
     const char *z;
{
  zLprogram = z;
}

/* Set the ID number.  This will be called by the usysdep_initialize
   if there is something sensible to set it to.  */

void
ulog_id (i)
     int i;
{
  iLid = i;
}

/* Set the user we are making log entries for.  The arguments will be
   copied into memory.  */

void
ulog_user (zuser)
     const char *zuser;
{
  if (zuser == NULL
      || zLuser == NULL
      || strcmp (zuser, zLuser) != 0)
    {
      xfree ((pointer) zLuser);
      if (zuser == NULL)
	zLuser = NULL;
      else
	zLuser = xstrdup (zuser);
    }
}

/* Set the system name we are making log entries for.  The name is copied
   into memory.  */

void
ulog_system (zsystem)
  const char *zsystem;
{
  if (zsystem == NULL
      || zLsystem == NULL
      || strcmp (zsystem, zLsystem) != 0)
    {
      xfree ((pointer) zLsystem);
      if (zsystem == NULL)
	zLsystem = NULL;
      else
	zLsystem = xstrdup (zsystem);
    }
}

/* Make a log entry.  We make a token concession to non __STDC__ systems,
   but it clearly won't always work.  */

#ifndef __STDC__
#undef HAVE_VPRINTF
#endif

/*VARARGS2*/
#if HAVE_VPRINTF
void
ulog (enum tlog ttype, const char *zmsg, ...)
#else
void
ulog (ttype, zmsg, a, b, c, d, f, g, h, i, j)
     enum tlog ttype;
     const char *zmsg;
#endif
{
#if HAVE_VPRINTF
  va_list parg;
#endif
  FILE *e, *edebug;
  const char *zhdr, *zstr;

  if (zLprogram == NULL)
    e = stderr;
#if DEBUG > 0
  else if (ttype == LOG_DEBUG)
    {
      static boolean ftried;

      if (eLdebug == NULL && ! ftried)
	{
	  ftried = TRUE;
	  eLdebug = esysdep_fopen (zDebugfile, FALSE);
	}
      e = eLdebug;
    }
#endif /* DEBUG > 0 */
  else
    {
      static boolean ftried;

      if (eLlog == NULL && ! ftried)
	{
	  ftried = TRUE;
	  eLlog = esysdep_fopen (zLogfile, TRUE);
	  if (eLlog == NULL)
	    usysdep_exit (FALSE);
	}
      e = eLlog;
    }

  if (e == NULL)
    e = stderr;

  edebug = NULL;
#if DEBUG > 0
  if (ttype != LOG_DEBUG)
    edebug = eLdebug;
#endif

  switch (ttype)
    {
    case LOG_NORMAL:
      zhdr = "";
      break;
    case LOG_ERROR:
      zhdr = "ERROR: ";
      break;
    case LOG_FATAL:
      zhdr = "FATAL: ";
      break;
#if DEBUG > 0
    case LOG_DEBUG:
      zhdr = "DEBUG: ";
      break;
#endif
    default:
      zhdr = "???: ";
      break;
    }

  fprintf (e, "%s", zhdr);
  if (edebug != NULL)
    fprintf (edebug, "%s", zhdr);
  if (zLprogram != NULL)
    {
      fprintf (e, "%s ", zLprogram);
      if (edebug != NULL)
	fprintf (edebug, "%s ", zLprogram);
    }
  if (zLsystem != NULL)
    {
      fprintf (e, "%s ", zLsystem);
      if (edebug != NULL)
	fprintf (edebug, "%s ", zLsystem);
    }
  if (zLuser != NULL)
    {
      fprintf (e, "%s ", zLuser);
      if (edebug != NULL)
	fprintf (edebug, "%s ", zLuser);
    }

  zstr = zdate_and_time ();
  fprintf (e, "(%s", zstr);
  if (edebug != NULL)
    fprintf (edebug, "(%s", zstr); 

  if (iLid != 0)
    {
      fprintf (e, " %d", iLid);
      if (edebug != NULL)
	fprintf (edebug, " %d", iLid);
    }

  fprintf (e, ") ");
  if (edebug != NULL)
    fprintf (edebug, ") ");
  
#if HAVE_VPRINTF
  va_start (parg, zmsg);
  vfprintf (e, zmsg, parg);
  va_end (parg);
  if (edebug != NULL)
    {
      va_start (parg, zmsg);
      vfprintf (edebug, zmsg, parg);
      va_end (parg);
    }
#else /* ! HAVE_VPRINTF */
  fprintf (e, zmsg, a, b, c, d, f, g, h, i, j);
  if (edebug != NULL)
    fprintf (edebug, zmsg, a, b, c, d, f, g, h, i, j);
#endif /* ! HAVE_VPRINTF */

  fprintf (e, "\n");
  if (edebug != NULL)
    fprintf (edebug, "\n");

  (void) fflush (e);
  if (edebug != NULL)
    (void) fflush (edebug);

  /* We should be able to just call abort here, but on Ultrix abort
     raises the wrong signal, and raise (SIGABRT) is just as good.  */

  if (ttype == LOG_FATAL)
    {
      fAborting = TRUE;
      raise (SIGABRT);
    }
}

/* Close the log file.  There's nothing useful we can do with errors,
   so we don't check for them.  */

void
ulog_close ()
{
  if (eLlog != NULL)
    (void) fclose (eLlog);
  if (eLdebug != NULL)
    (void) fclose (eLdebug);
}

/* Return the date and time in a form used for a log entry.  */

const char *
zdate_and_time ()
{
  time_t itime;
  struct tm *q;
  static char ab[sizeof "Jan 31 12:00"];

  (void) time (&itime);
  q = localtime (&itime);

  sprintf (ab, "%.3s %d %02d:%02d",
	   "JanFebMarAprMayJunJulAugSepOctNovDec" + q->tm_mon * 3,
	   q->tm_mday, q->tm_hour, q->tm_min);

  return ab;
}
