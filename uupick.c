/* uupick.c
   Get files stored in the public directory by uucp -t.

   Copyright (C) 1992 Ian Lance Taylor

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.  */

#include "uucp.h"

#if USE_RCS_ID
char uupick_rcsid[] = "$Id$";
#endif

#include <errno.h>

#include "getopt.h"

#include "system.h"
#include "sysdep.h"

/* Local functions.  */

static void upmovedir P((const char *zfull, const char *zrelative,
			 pointer pinfo));
static void upmove P((const char *zfrom, const char *zto));

/* The program name.  */
char abProgram[] = "uupick";

/* Long getopt options.  */

static const struct option asClongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asClongopts;

/* Local functions.  */

static void upusage P((void));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* -s: system name.  */
  const char *zsystem = NULL;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  const char *zfile, *zfrom, *zfull;
  const char *zallsys;
  char ab[1000];

  while ((iopt = getopt (argc, argv, "I:s:x:")) != EOF)
    {
      switch (iopt)
	{
	case 's':
	  /* System name to get files from.  */
	  zsystem = optarg;
	  break;

	case 'I':
	  /* Name configuration file.  */
	  zconfig = optarg;
	  break;

	case 'x':
#if DEBUG > 1
	  /* Set debugging level.  */
	  iDebug |= idebug_parse (optarg);
#endif
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  upusage ();
	  break;
	}
    }

  if (argc != optind)
    upusage ();

  uread_config (zconfig);

  usysdep_initialize (INIT_NOCHDIR);

  if (! fsysdep_uupick_init (zsystem))
    usysdep_exit (FALSE);

  zallsys = NULL;

  while ((zfile = zsysdep_uupick (zsystem, &zfrom, &zfull)) != NULL)
    {
      boolean fdir;
      char *zto;
      const char *zconst;
      FILE *e;
      boolean fcontinue;

      fdir = fsysdep_directory (zfull);

      do
	{
	  fcontinue = FALSE;

	  if (zallsys == NULL
	      || strcmp (zallsys, zfrom) != 0)
	    {
	      printf ("from %s: %s %s ?\n", zfrom, fdir ? "dir" : "file",
		      zfile);

	      if (fgets (ab, sizeof ab, stdin) == NULL)
		break;
	    }

	  if (ab[0] == 'q')
	    break;

	  switch (ab[0])
	    {
	    case '\n':
	      break;

	    case 'd':
	      if (fdir)
		(void) fsysdep_rmdir (zfull);
	      else
		{
		  if (remove (zfull) != 0)
		    ulog (LOG_ERROR, "remove (%s): %s", zfull,
			  strerror(errno));
		}
	      break;

	    case 'm':
	    case 'a':
	      zto = ab + 1 + strspn (ab + 1, " \t");
	      zto[strcspn (zto, " \t\n")] = '\0';
	      if (*zto == '\0')
		zconst = zsysdep_add_cwd (zfile, TRUE);
	      else
		zconst = zsysdep_in_dir (zto, zfile);
	      if (zconst == NULL)
		usysdep_exit (FALSE);

	      zto = xstrdup (zconst);
	      if (! fdir)
		upmove (zfull, zto);
	      else
		{
		  usysdep_walk_tree (zfull, upmovedir, (pointer) zto);
		  (void) fsysdep_rmdir (zfull);
		}
	      xfree ((pointer) zto);

	      if (ab[0] == 'a')
		{
		  zallsys = xstrdup (zfrom);
		  ab[0] = 'm';
		}

	      break;

	    case 'p':
	      if (fdir)
		ulog (LOG_ERROR, "Can't print directory");
	      else
		{
		  e = fopen (zfull, "r");
		  if (e == NULL)
		    ulog (LOG_ERROR, "fopen (%s): %s", zfull,
			  strerror (errno));
		  else
		    {
		      while (fgets (ab, sizeof ab, e) != NULL)
			(void) fputs (ab, stdout);
		      (void) fclose (e);
		    }
		}
	      fcontinue = TRUE;
	      break;

	    case '!':
	      (void) system (ab + 1);
	      fcontinue = TRUE;
	      break;

	    default:
	      printf ("uupick commands:\n");
	      printf ("q: quit\n");
	      printf ("<return>: skip file\n");
	      printf ("m [dir]: move file to directory\n");
	      printf ("a [dir]: move all files from this system to directory\n");
	      printf ("p: list file to stdout\n");
	      printf ("! command: shell escape\n");
	      fcontinue = TRUE;
	      break;
	    }
	}
      while (fcontinue);
    }

  (void) fsysdep_uupick_free (zsystem);

  usysdep_exit (TRUE);

  /* Avoid error about not returning.  */
  return 0;
}

/* Print usage message.  */

static void
upusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   abVersion);
  fprintf (stderr,
	   "Usage: uupick [-s system] [-I config] [-x debug]\n");
  fprintf (stderr,
	   " -s system: Only consider files from named system\n");
  fprintf (stderr,
	   " -x debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use (default %s%s)\n",
	   NEWCONFIGLIB, CONFIGFILE);
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* This routine is called by usysdep_walk_tree when moving the
   contents of an entire directory.  */

static void
upmovedir (zfull, zrelative, pinfo)
     const char *zfull;
     const char *zrelative;
     pointer pinfo;
{
  const char *ztodir = (const char *) pinfo;
  const char *zconst;
  char *zcopy;

  zconst = zsysdep_in_dir (ztodir, zrelative);
  if (zconst == NULL)
    usysdep_exit (FALSE);
  zcopy = (char *) alloca (strlen (zconst) + 1);
  strcpy (zcopy, zconst);
  upmove (zfull, zcopy);
}

/* Move a file.  */

static void
upmove (zfrom, zto)
     const char *zfrom;
     const char *zto;
{
  (void) fsysdep_move_file (zfrom, zto, TRUE, TRUE, FALSE,
			    (const char *) NULL);
}
