/* uulog.c
   Display the UUCP log file.

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

#if USE_RCS_ID
const char uulog_rcsid[] = "$Id$";
#endif

#include <ctype.h>
#include <errno.h>

#include "getopt.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"

/* This is a pretty bad implementation of uulog, which I don't think
   is a very useful program anyhow.  It only takes a single -s and/or
   -u switch.  When using HAVE_HDB_LOGGING it requires a system.  */

/* The program name.  */
char abProgram[] = "uulog";

/* Local functions.  */

static void ulusage P((void));

/* Long getopt options.  */
static const struct option asLlongopts[] = { { NULL, 0, NULL, 0 } };

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* -f: keep displaying lines forever.  */
  boolean fforever = FALSE;
  /* -n lines: number of lines to display.  */
  int cshow = 0;
  /* -s: system name.  */
  const char *zsystem = NULL;
  /* -u: user name.  */
  const char *zuser = NULL;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -x: display uuxqt log file.  */
  boolean fuuxqt = FALSE;
  int i;
  int iopt;
  pointer puuconf;
  int iuuconf;
  const char *zlogfile;
  const char *zfile;
  FILE *e;
  char **pzshow = NULL;
  int ishow = 0;
  size_t csystem = 0;
  size_t cuser = 0;
  char *zline;
  size_t cline;

  /* Look for a straight number argument, and convert it to -n before
     passing the arguments to getopt.  */
  for (i = 0; i < argc; i++)
    {
      if (argv[i][0] == '-' && isdigit (argv[i][1]))
	{
	  size_t clen;
	  char *znew;

	  clen = strlen (argv[i]);
	  znew = (char *) alloca (clen + 2);
	  znew[0] = '-';
	  znew[1] = 'n';
	  memcpy (znew + 2, argv[i] + 1, clen);
	  argv[i] = znew;
	}
    }

  while ((iopt = getopt_long (argc, argv, "fI:n:s:u:xX:", asLlongopts,
			      (int *) NULL)) != EOF)
    {
      switch (iopt)
	{
	case 'f':
	  /* Keep displaying lines forever.  */
	  fforever = TRUE;
	  break;

	case 'I':
	  /* Configuration file name.  */
	  if (fsysdep_other_config (optarg))
	    zconfig = optarg;
	  break;

	case 'n':
	  /* Number of lines to display.  */
	  cshow = (int) strtol (optarg, (char **) NULL, 10);
	  break;

	case 's':
	  /* System name.  */
	  zsystem = optarg;
	  break;

	case 'u':
	  /* User name.  */
	  zuser = optarg;
	  break;

	case 'x':
	  /* Display uuxqt log file.  */
	  fuuxqt = TRUE;
	  break;

	case 'X':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  ulusage ();
	  break;
	}
    }

  if (optind != argc)
    ulusage ();

  iuuconf = uuconf_init (&puuconf, (const char *) NULL, zconfig);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

#if DEBUG > 1
  {
    const char *zdebug;

    iuuconf = uuconf_debuglevel (puuconf, &zdebug);
    if (iuuconf != UUCONF_SUCCESS)
      ulog_uuconf (LOG_FATAL, puuconf, iuuconf);
    if (zdebug != NULL)
      iDebug |= idebug_parse (zdebug);
  }
#endif

  iuuconf = uuconf_logfile (puuconf, &zlogfile);
  if (iuuconf != UUCONF_SUCCESS)
    ulog_uuconf (LOG_FATAL, puuconf, iuuconf);

  usysdep_initialize (puuconf, 0);

#if ! HAVE_HDB_LOGGING
  zfile = zlogfile;
#else
  {
    const char *zprogram;
    char *zalc;

    /* We need a system to find a HDB log file.  */
    if (zsystem == NULL)
      ulusage ();

    if (fuuxqt)
      zprogram = "uuxqt";
    else
      zprogram = "uucico";

    zalc = (char *) alloca (strlen (zlogfile)
			    + strlen (zprogram)
			    + strlen (zsystem)
			    + 1);
    sprintf (zalc, zlogfile, zprogram, zsystem);
    zfile = zalc;
  }
#endif

  e = fopen (zfile, "r");
  if (e == NULL)
    {
      ulog (LOG_ERROR, "fopen (%s): %s", zfile, strerror (errno));
      usysdep_exit (FALSE);
    }

  if (cshow > 0)
    {
      pzshow = (char **) xmalloc (cshow * sizeof (char *));
      for (ishow = 0; ishow < cshow; ishow++)
	pzshow[ishow] = NULL;
      ishow = 0;
    }

  /* Read the log file and output the appropriate lines.  */
  if (zsystem != NULL)
    csystem = strlen (zsystem);

  if (zuser != NULL)
    cuser = strlen (zuser);

  zline = NULL;
  cline = 0;

  while (TRUE)
    {
      while (getline (&zline, &cline, e) > 0)
	{
	  char *zluser, *zlsys, *znext;
	  size_t cluser, clsys;

	  /* Skip any leading whitespace (not that there should be
	     any).  */
	  znext = zline + strspn (zline, " \t");

#if ! HAVE_TAYLOR_LOGGING
	  /* The user name is the first field on the line.  */
	  zluser = znext;
	  cluser = strcspn (znext, " \t");
#endif
      
	  /* Skip the first field.  */
	  znext += strcspn (znext, " \t");
	  znext += strspn (znext, " \t");

	  /* The system is the second field on the line.  */
	  zlsys = znext;
	  clsys = strcspn (znext, " \t");

	  /* Skip the second field.  */
	  znext += clsys;
	  znext += strspn (znext, " \t");

#if HAVE_TAYLOR_LOGGING
	  /* The user is the third field on the line.  */
	  zluser = znext;
	  cluser = strcspn (znext, " \t");
#endif

	  /* See if we should print this line.  */
	  if (zsystem != NULL
	      && (csystem != clsys
		  || strncmp (zsystem, zlsys, clsys) != 0))
	    continue;

	  if (zuser != NULL
	      && (cuser != cluser
		  || strncmp (zuser, zluser, cluser) != 0))
	    continue;

	  /* Output the line, or save it if we are outputting only a
	     particular number of lines.  */
	  if (cshow <= 0)
	    printf ("%s", zline);
	  else
	    {
	      ubuffree ((pointer) pzshow[ishow]);
	      pzshow[ishow] = zbufcpy (zline);
	      ishow = (ishow + 1) % cshow;
	    }
	}

      /* Output the number of lines requested by the -n option.  */
      if (cshow > 0)
	{
	  for (i = 0; i < cshow; i++)
	    {
	      if (pzshow[ishow] != NULL)
		printf ("%s", pzshow[ishow]);
	      ishow = (ishow + 1) % cshow;
	    }
	}

      /* If -f was not specified, or an error occurred while reading
	 the file, get out.  */
      if (! fforever || ferror (e))
	break;

      clearerr (e);
      cshow = 0;

      /* Sleep 1 second before going around the loop again.  */
      usysdep_sleep (1);
    }

  (void) fclose (e);

  ulog_close ();

  usysdep_exit (TRUE);

  /* Avoid errors about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void
ulusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   VERSION);
  fprintf (stderr,
	   "Usage: uulog [-s system] [-u user] [-x] [-I file] [-X debug]\n");
  fprintf (stderr,
	   " -s: print entries for named system\n");
  fprintf (stderr,
	   " -u: print entries for name user\n");
#if HAVE_HDB_LOGGING
  fprintf (stderr,
	   " -x: print uuxqt log rather than uucico log\n");
#endif
  fprintf (stderr,
	   " -X debug: Set debugging level (0 for none, 9 is max)\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use\n");
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}
