/* time.c
   Routines to deal with UUCP time strings.

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.

   $Log$
   Revision 1.7  1992/01/11  17:30:10  ian
   John Antypas: use memcpy instead of relying on structure assignment

   Revision 1.6  1991/12/29  04:04:18  ian
   Added a bunch of extern definitions

   Revision 1.5  1991/12/22  20:57:57  ian
   Added externs for strcasecmp or strncasecmp

   Revision 1.4  1991/09/19  02:22:44  ian
   Chip Salzenberg's patch to allow ";retrytime" at the end of a time string

   Revision 1.3  1991/09/12  05:04:44  ian
   Wrong sense of comparison in btime_low_grade

   Revision 1.2  1991/09/11  16:59:00  ian
   fcheck_time and btime_low_grade looped endlessly on unusual grades
  
   Revision 1.1  1991/09/10  19:40:31  ian
   Initial revision
  
   */

#include "uucp.h"

#if USE_RCS_ID
char time_rcsid[] = "$Id$";
#endif

#include <ctype.h>

#if HAVE_TIME_H
#include <time.h>
#endif

#if ! HAVE_TIME_T
#if HAVE_SYS_TIME_T
#include <sys/types.h>
#endif /* HAVE_SYS_TIME_T */
#endif /* ! HAVE_TIME_T */

/* External functions.  */
extern int strncasecmp ();
extern time_t time ();
extern struct tm *localtime ();

/* Timetables are kept in a array of pairs of strings.  */

struct stimetable
{
  const char *zname;
  const char *ztime;
};

static int cTtable;
static struct stimetable *pasTtable;

/* Initialize the table of timetables as advertised in the
   documentation.  */

static void utinit_timetable P((void));

static void
utinit_timetable ()
{
  pasTtable = (struct stimetable *) xmalloc (3 * sizeof (struct stimetable));
  pasTtable[0].zname = "Evening";
  pasTtable[0].ztime = "Wk1705-0755,Sa,Su";
  pasTtable[1].zname = "Night";
  pasTtable[1].ztime = "Wk2305-0755,Sa,Su2305-1655";
  pasTtable[2].zname = "NonPeak";
  pasTtable[2].ztime = "Wk1805-0655,Sa,Su";
  cTtable = 3;
}

/* Add a new timetable entry.  This assumes it can take control of the
   strings it is passed, so they must not be on the stack and if they
   have been allocated they must not be freed.  */

void
uaddtimetable (zname, ztime)
     const char *zname;
     const char *ztime;
{
  if (pasTtable == NULL)
    utinit_timetable ();

  pasTtable = ((struct stimetable *)
	       xrealloc ((pointer) pasTtable,
			 (cTtable + 1) * sizeof (struct stimetable)));
  pasTtable[cTtable].zname = zname;
  pasTtable[cTtable].ztime = ztime;
  ++cTtable;
}

/* An array of weekday abbreviations.  */

static struct
{
  const char *zname;
  int imin;
  int imax;
} asTdays[] =
{
  { "any", 0, 6 },
  { "wk", 1, 5 },
  { "su", 0, 0 },
  { "mo", 1, 1 },
  { "tu", 2, 2 },
  { "we", 3, 3 },
  { "th", 4, 4 },
  { "fr", 5, 5 },
  { "sa", 6, 6 },
  { "never", -1, -1 },
  { NULL, 0, 0 }
};

/* Check whether a broken-down time matches a time string.
   The time string continues to a null byte, a space or a semicolon.
   On success, return the retry time.  On failure, return -1. */

static int cttime_ok P((const struct tm *, const char *ztime));

static int
cttime_ok (q, ztime)
     const struct tm *q;
     const char *ztime;
{
  int i, cretry;
  const char *zend;

  zend = ztime + strcspn (ztime, "; ");

  cretry = 0;
  if (*zend == ';')
    cretry = atoi (zend + 1);

  if (pasTtable == NULL)
    utinit_timetable ();

  /* Expand the time string using a timetable.  */
  for (i = 0; i < cTtable; i++)
    {
      if (strncasecmp (ztime, pasTtable[i].zname, zend - ztime) == 0)
	{
	  ztime = pasTtable[i].ztime;
	  zend = ztime + strlen (ztime);
	  break;
	}
    }

  /* Look through the portions of the time string separated by a
     comma or a vertical bar.  */

  for (; ztime < zend; ztime += strcspn (ztime, ",|"))
    {
      const char *z;
      boolean fmatch;

      if (*ztime == ',' || *ztime == '|')
	++ztime;

      /* Look through the days.  */

      fmatch = FALSE;
      z = ztime;
      do
	{
	  int iday;

	  for (iday = 0; asTdays[iday].zname != NULL; iday++)
	    {
	      int clen;

	      clen = strlen (asTdays[iday].zname);
	      if (strncasecmp (z, asTdays[iday].zname, clen) == 0)
		{
		  if (q->tm_wday >= asTdays[iday].imin
		      && q->tm_wday <= asTdays[iday].imax)
		    fmatch = TRUE;
		  z += clen;
		  break;
		}
	    }
	  if (asTdays[iday].zname == NULL)
	    {
	      ulog (LOG_ERROR, "%s: unparseable time string", ztime);
	      return -1;
	    }
	}
      while (isalpha (BUCHAR (*z)));

      if (isdigit (BUCHAR (*z)))
	{
	  char *zendnum;
	  int istart, iend;

	  istart = (int) strtol (z, &zendnum, 10);
	  if (*zendnum != '-' || ! isdigit (BUCHAR (zendnum[1])))
	    {
	      ulog (LOG_ERROR, "%s: unparseable time string", ztime);
	      return -1;
	    }
	  z = zendnum + 1;
	  iend = (int) strtol (z, &zendnum, 10);
	  if (*zendnum != '\0'
	      && *zendnum != ' '
	      && *zendnum != ';'
	      && *zendnum != ','
	      && *zendnum != '|')
	    {
	      ulog (LOG_ERROR, "%s: unparseable time string", ztime);
	      return -1;
	    }

	  if (fmatch)
	    {
	      int ihour;

	      ihour = q->tm_hour * 100 + q->tm_min;
	      if (istart < iend)
		{
		  if (ihour < istart || ihour > iend)
		    fmatch = FALSE;
		}
	      else
		{
		  if (ihour < istart && ihour > iend)
		    fmatch = FALSE;
		}
	    }
	}

      if (fmatch)
	return cretry;
    }

  return -1;
}

/* Check whether we can call a system now, given a grade of work to
   be done.  Return retry time, or -1 if failure.  */

int
ccheck_time (bgrade, ztimegrade)
     int bgrade;
     const char *ztimegrade;
{
  struct tm *q;
  time_t itime;
  int cretry;

  /* Get the time.  */
  time (&itime);
  q = localtime (&itime);

  /* The format of ztime is a series of single character grades
     followed by time strings.  The time string may end with a
     semicolon and a retry time in minutes.  Each grade/time/retry
     tuple is separated by a space.  */

  while (TRUE)
    {
      /* Make sure this grade/time pair applies to this grade.  It
	 doesn't if the grade from ztimegrade is executed before the
	 grade from bgrade.  */
      if (igradecmp (*ztimegrade, bgrade) >= 0)
	{
	  cretry = cttime_ok (q, ztimegrade + 1);
	  if (cretry >= 0)
	    return cretry;
	}

      ztimegrade += strcspn (ztimegrade, " ");

      if (*ztimegrade == '\0')
	break;

      ++ztimegrade;
    }
      
  return -1;
}

/* Determine the lowest grade of work we are permitted to do at the
   current time, given a time/grade string.  Return a null byte if
   no grades are legal.  */

char
btime_low_grade (ztimegrade)
     const char *ztimegrade;
{
  struct tm *q;
  time_t itime;
  char bgrade;

  /* Get the time.  */
  time (&itime);
  q = localtime (&itime);

  bgrade = '\0';

  while (TRUE)
    {
      if ((bgrade == '\0'
	   || igradecmp (bgrade, *ztimegrade) < 0)
	  && cttime_ok (q, ztimegrade + 1) >= 0)
	bgrade = *ztimegrade;

      ztimegrade += strcspn (ztimegrade, " ");

      if (*ztimegrade == '\0')
	break;

      ++ztimegrade;
    }
      
  return bgrade;
}

/* Check whether the current time matches a time string.  We only
   compute the current time when this function is first called.  We
   should probably reset the time occasionally.  */

boolean
ftime_now (ztime)
     const char *ztime;
{
  static struct tm s;
  static boolean fhave;

  if (! fhave)
    {
      time_t itime;

      (void) time (&itime);
      memcpy (&s, localtime (&itime), sizeof (struct tm));
      fhave = TRUE;
    }

  return cttime_ok (&s, ztime) >= 0;
}
