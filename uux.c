/* uux.c
   Prepare to execute a command on a remote system.

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
 * Revision 1.1  1991/09/10  19:40:31  ian
 * Initial revision
 *
   */

#include "uucp.h"

#if USE_RCS_ID
char uux_rcsid[] = "$Id$";
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include "getopt.h"

#include "system.h"
#include "sysdep.h"

/* Long getopt options.  */

static const struct option asXlongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asXlongopts;

/* The execute file we are creating.  */

static FILE *eXxqt_file;

/* A list of commands to be spooled.  */

static struct scmd *pasXcmds;
static int cXcmds;

/* Local functions.  */

static void uxusage P((void));
static sigret_t uxcatch P((int isig));
static void uxadd_xqt_line P((int bchar, const char *z1, const char *z2));
static void uxadd_send_file P((const char *zfrom, const char *zto,
			       const char *zoptions, const char *ztemp));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* -a: requestor address for status reports.  */
  const char *zrequestor = NULL;
  /* -b: if true, return standard input on error.  */
  boolean fretstdin = FALSE;
  /* -c,-l,-C: if true, copy to spool directory.  */
  boolean fcopy = FALSE;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  /* -j: output job id.  */
  boolean fjobid = FALSE;
  /* -g: job grade.  */
  char bgrade = BDEFAULT_UUX_GRADE;
  /* -n: do not notify upon command completion.  */
  boolean fno_ack = FALSE;
  /* -p: read standard input for command standard input.  */
  boolean fread_stdin = FALSE;
  /* -r: do not start uucico when finished.  */
  boolean fuucico = TRUE;
  /* -s: report status to named file.  */
  const char *zstatus_file = NULL;
  /* -x: set debugging level.  */
  int idebug = -1;
  /* -z: report status only on error.  */
  boolean ferror_ack = FALSE;
  const char *zuser;
  int i;
  int clen;
  char *zargs;
  char *zcmd;
  char *zexclam;
  struct ssysteminfo sxqtsys;
  const struct ssysteminfo *qxqtsys;
  boolean fxqtlocal;
  char **pzargs;
  int calloc_args;
  int cargs;
  const char *zxqtname;
  char abxqt_tname[CFILE_NAME_LEN];
  char abxqt_xname[CFILE_NAME_LEN];
  char *zprint;

  /* We need to be able to read a single - as an option, which getopt
     won't do.  So that we can still use getopt, we run through the
     options looking for an option "-"; if we find one we change it to
     "-p", which is an equivalent option.  */

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] != '-')
	break;
      if (argv[i][1] == '\0')
	argv[i] = xstrdup ("-p");
      else
	{
	  const char *z;

	  for (z = argv[i] + 1; *z != '\0'; z++)
	    if (*z == 'a' || *z == 'I' || *z == 's' || *z == 'x')
	      i++;
	}
    }

  /* The leading + in the getopt string means to stop processing
     options as soon as a non-option argument is seen.  */

  while ((iopt = getopt (argc, argv, "+a:bcCI:jg:lnprs:x:z")) != EOF)
    {
      switch (iopt)
	{
	case 'a':
	  /* Set requestor name: mail address to which status reports
	     should be sent.  */
	  zrequestor = optarg;
	  break;

	case 'b':
	  /* Return standard input on error.  */
	  fretstdin = TRUE;
	  break;

	case 'c':
	case 'l':
	  /* Do not copy local files to spool directory (default).  */
	  fcopy = FALSE;
	  break;

	case 'C':
	  /* Copy local files to spool directory.  */
	  fcopy = TRUE;
	  break;

	case 'I':
	  /* Configuration file name.  */ 
	  zconfig = optarg;
	  break;

	case 'j':
	  /* Output jobid.  */
	  fjobid = TRUE;
	  break;

	case 'g':
	  /* Set job grade.  */
	  bgrade = optarg[0];
	  break;

	case 'n':
	  /* Do not notify upon command completion.  */
	  fno_ack = TRUE;
	  break;

	case 'p':
	  /* Read standard input for command standard input.  */
	  fread_stdin = TRUE;
	  break;

	case 'r':
	  /* Do not start uucico when finished.  */
	  fuucico = FALSE;
	  break;

	case 's':
	  /* Report status to named file.  */
	  zstatus_file = optarg;
	  break;

	case 'x':
	  /* Set debugging level.  */
	  idebug = atoi (optarg);
	  break;

	case 'z':
	  /* Report status only on error.  */
	  ferror_ack = TRUE;
	  break;

	case 0:
	  /* Long option found and flag set.  */
	  break;

	default:
	  uxusage ();
	  break;
	}
    }

  if (! FGRADE_LEGAL (bgrade))
    {
      fprintf (stderr, "uux: Ignoring illegal grade\n");
      bgrade = BDEFAULT_UUX_GRADE;
    }

  if (optind == argc)
    uxusage ();

  uread_config (zconfig);

  /* Let command line override configuration file.  */
  if (idebug != -1)
    iDebug = idebug;

#ifdef SIGINT
  if (signal (SIGINT, uxcatch) == SIG_IGN)
    (void) signal (SIGINT, SIG_IGN);
#endif
#ifdef SIGHUP
  if (signal (SIGHUP, uxcatch) == SIG_IGN)
    (void) signal (SIGHUP, SIG_IGN);
#endif
#ifdef SIGQUIT
  if (signal (SIGQUIT, uxcatch) == SIG_IGN)
    (void) signal (SIGQUIT, SIG_IGN);
#endif
#ifdef SIGTERM
  if (signal (SIGTERM, uxcatch) == SIG_IGN)
    (void) signal (SIGTERM, SIG_IGN);
#endif
#ifdef SIGPIPE
  if (signal (SIGPIPE, uxcatch) == SIG_IGN)
    (void) signal (SIGPIPE, SIG_IGN);
#endif
#ifdef SIGABRT
  (void) signal (SIGABRT, uxcatch);
#endif

  usysdep_initialize (FALSE);

  zuser = zsysdep_login_name ();
  if (zuser == NULL)
    zuser = "unknown";

  /* The command and files arguments could be quoted in any number of
     ways, so we split them apart ourselves.  We strip any spaces
     following < or > to make them easier to handle below.  */
  clen = 0;
  for (i = optind; i < argc; i++)
    clen += strlen (argv[i]) + 1;

  zargs = (char *) alloca (clen);
  *zargs = '\0';
  for (i = optind; i < argc - 1; i++)
    {
      strcat (zargs, argv[i]);
      if (strcmp (argv[i], "<") != 0 && strcmp (argv[i], ">") != 0)
	strcat (zargs, " ");
    }
  strcat (zargs, argv[i]);

  /* The first argument is the command to execute.  Figure out which
     system the command is to be executed on.  */

  zcmd = strtok (zargs, " \t");
  zexclam = strchr (zcmd, '!');
  if (zexclam == NULL)
    {
      qxqtsys = &sLocalsys;
      fxqtlocal = TRUE;
    }
  else
    {
      *zexclam = '\0';

      if (*zcmd == '\0' || strcmp (zcmd, zLocalname) == 0)
	{
	  qxqtsys = &sLocalsys;
	  fxqtlocal = TRUE;
	}
      else
	{
	  if (fread_system_info (zcmd, &sxqtsys))
	    qxqtsys = &sxqtsys;
	  else
	    {
	      if (! fUnknown_ok)
		ulog (LOG_FATAL, "System %s unknown", zcmd);
	      qxqtsys = &sUnknown;
	      sUnknown.zname = zcmd;
	    }

	  fxqtlocal = FALSE;
	}

      zcmd = zexclam + 1;
    }

  /* Make sure we have a spool directory.  */

  if (! fsysdep_make_spool_dir (qxqtsys))
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }

  calloc_args = 10;
  pzargs = (char **) xmalloc (calloc_args * sizeof (char *));
  cargs = 0;

  while (TRUE)
    {
      char *zarg;

      zarg = strtok ((char *) NULL, " \t");
      if (zarg == NULL)
	break;
      if (cargs >= calloc_args)
	{
	  calloc_args += 10;
	  pzargs = (char **) xrealloc (pzargs,
				       calloc_args * sizeof (char *));
	}
      pzargs[cargs] = zarg;
      ++cargs;
    }

  /* Name and open the execute file.  If the execution is to occur on
     a remote system, we must create a data file and copy it over.  */
  if (fxqtlocal)
    zxqtname = zsysdep_xqt_file_name ();
  else
    zxqtname = zsysdep_data_file_name (qxqtsys, 'X', abxqt_tname,
				       (char *) NULL, abxqt_xname);
  if (zxqtname == NULL)
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }

  eXxqt_file = esysdep_fopen (zxqtname, FALSE);
  if (eXxqt_file == NULL)
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }

  /* Specify the user.  */
  uxadd_xqt_line ('U', zuser, zLocalname);

  /* Look through the arguments.  Any argument containing an
     exclamation point character is interpreted as a file name, and is
     sent to the appropriate system.  */

  for (i = 0; i < cargs; i++)
    {
      const char *zsystem;
      char *zfile;
      boolean flocal;
      boolean finput;

      /* Check for a parenthesized argument; remove the parentheses
	 and otherwise ignore it (this is how an exclamation point is
	 quoted).  */

      if (pzargs[i][0] == '(')
	{
	  clen = strlen (pzargs[i]);
	  if (pzargs[i][clen - 1] != ')')
	    fprintf (stderr, "uux: Mismatched parentheses");
	  else
	    pzargs[i][clen - 1] = '\0';
	  ++pzargs[i];
	  continue;
	}

      zexclam = strchr (pzargs[i], '!');

      /* If there is no exclamation point and no redirection, this
	 argument is left untouched.  */

      if (zexclam == NULL
	  && pzargs[i][0] != '<'
	  && pzargs[i][0] != '>')
	continue;

      /* Get the system name and file name for this file.  */

      if (zexclam == NULL)
	{
	  zsystem = zLocalname;
	  zfile = pzargs[i] + 1;
	  flocal = TRUE;
	}
      else
	{
	  *zexclam = '\0';
	  zsystem = pzargs[i];
	  if (zsystem[0] == '>'
	      || zsystem[0] == '<')
	    ++zsystem;
	  if (zsystem[0] != '\0')
	    flocal = strcmp (zsystem, zLocalname) == 0;
	  else
	    {
	      zsystem = zLocalname;
	      flocal = TRUE;
	    }
	  zfile = zexclam + 1;
	}

      /* Check for output redirection.  We strip this argument out,
	 and create an O command which tells uuxqt where to send the
	 output.  */

      if (pzargs[i][0] == '>')
	{
	  if (strcmp (zsystem, qxqtsys->zname) == 0)
	    uxadd_xqt_line ('O', zfile, (const char *) NULL);
	  else
	    uxadd_xqt_line ('O', zfile, zsystem);
	  pzargs[i] = NULL;
	  continue;
	}

      finput = pzargs[i][0] == '>';
      if (finput)
	{
	  if (fread_stdin)
	    ulog (LOG_FATAL, "Standard input specified twice");
	  pzargs[i] = NULL;
	}

      if (flocal)
	{
	  const char *zconst;
	  char *zuse;
	  const char *zdata;
	  char abtname[CFILE_NAME_LEN];
	  char abdname[CFILE_NAME_LEN];

	  /* It's a local file.  If requested by -C, copy the file to
	     the spool directory; otherwise if the command is begin
	     executed locally, prepend the current directory.  If the
	     file is being shipped to another system, we must set up a
	     file transfer request.  */

	  zconst = zsysdep_add_cwd (zfile);
	  if (zconst == NULL)
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }
	  zuse = xstrdup (zconst);

	  if (fcopy)
	    {
	      char *zdup;

	      zdata = zsysdep_data_file_name (qxqtsys, bgrade, abtname,
					      abdname, (char *) NULL);
	      if (zdata == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      zdup = xstrdup (zdata);

	      if (! fcopy_file (zuse, zdup, FALSE))
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      xfree ((pointer) zuse);
	      xfree ((pointer) zdup);

	      zuse = abtname;
	    }
	  else if (! fxqtlocal)
	    {
	      zdata = zsysdep_data_file_name (qxqtsys, bgrade,
					      (char *) NULL, abdname,
					      (char *) NULL);
	      if (zdata == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}
	      strcpy (abtname, "D.0");
	    }

	  if (fxqtlocal)
	    {
	      if (finput)
		uxadd_xqt_line ('I', zuse, (char *) NULL);
	      else
		pzargs[i] = zuse;
	    }
	  else
	    {
	      uxadd_send_file (zuse, abdname,
			       fcopy ? "C" : "c",
			       abtname);

	      if (finput)
		{
		  uxadd_xqt_line ('F', abdname, (char *) NULL);
		  uxadd_xqt_line ('I', abdname, (char *) NULL);
		}
	      else
		{
		  const char *zbase;

		  zbase = zsysdep_base_name (zfile);
		  if (zbase == NULL)
		    {
		      ulog_close ();
		      usysdep_exit (FALSE);
		    }
		  uxadd_xqt_line ('F', abdname, zbase);
		  pzargs[i] = xstrdup (zbase);
		}
	    }
	}
      else if (strcmp (qxqtsys->zname, zsystem) == 0)
	{
	  /* The file is already on the system where the command is to
	     be executed.  Standard uux would prepend the current
	     directory to the file name, even though the current
	     directory may not have any meaning on the remote system.
	     Since we want this to work across systems with different
	     naming conventions, we don't do this, and simply require
	     any uux call to fully specify remote path names.  If I
	     think of a better way to handle this, I'll change it.  */
	  if (finput)
	    uxadd_xqt_line ('I', zfile, (const char *) NULL);
	  else
	    pzargs[i] = zfile;
	}
      else
	{
	  struct ssysteminfo sfromsys;
	  const struct ssysteminfo *qfromsys;
	  const char *zconst;
	  char *zdata;
	  char abtname[CFILE_NAME_LEN];
	  char abdname[CFILE_NAME_LEN];
	  char *ztemp;
	  struct scmd s;

	  /* We need to request a remote file.  Make sure we have a
	     spool directory for the remote system.  */

	  if (! fread_system_info (zsystem, &sfromsys))
	    {
	      if (! fUnknown_ok)
		ulog (LOG_FATAL, "System %s unknown", zsystem);
	      sfromsys = sUnknown;
	      sfromsys.zname = zsystem;
	    }
	  qfromsys = &sfromsys;

	  if (! fsysdep_make_spool_dir (qfromsys))
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }

	  /* We want the file to wind up in the spool directory of the
	     local system (whether the execution is occurring
	     locally or not); we have to use an absolute file name
	     here, because otherwise the file would wind up in the
	     spool directory of the system it is coming from.  */

	  if (! fxqtlocal)
	    {
	      if (! fsysdep_make_spool_dir (&sLocalsys))
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}
	    }

	  zconst = zsysdep_data_file_name (&sLocalsys, bgrade,
					   abtname, (char *) NULL,
					   (char *) NULL);
	  if (zconst == NULL)
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }

	  zdata = xstrdup (zconst);

	  /* Request the file.  As noted above, standard uux would
	     prepend the current directory to zfile here, but we do
	     not.  */

	  s.bcmd = 'R';
	  s.pseq = NULL;
	  s.zfrom = zfile;
	  s.zto = zdata;
	  s.zuser = zuser;
	  s.zoptions = "";
	  s.ztemp = "";
	  s.imode = 0600;
	  s.znotify = "";
	  s.cbytes = -1;

	  if (! fsysdep_spool_commands (qfromsys, bgrade, 1, &s))
	    {
	      ulog_close ();
	      usysdep_exit (FALSE);
	    }

	  /* Now if the execution is to occur on another system, we
	     must create an execute file to send the file there.  The
	     name of the file on the execution system is put into
	     abdname.  */

	  if (fxqtlocal)
	    ztemp = abtname;
	  else
	    {
	      const char *zxqt_file;
	      FILE *e;

	      /* Get a file name to use on the execution system.  */

	      if (zsysdep_data_file_name (qxqtsys, bgrade,
					  (char *) NULL, abdname,
					  (char *) NULL) == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}
	      ztemp = abdname;

	      /* The local spool directory was created above, if it
		 didn't already exist.  */

	      zxqt_file = zsysdep_xqt_file_name ();
	      if (zxqt_file == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      /* This doesn't work correctly, since it never removes
		 the file from the local spool directory.  */

	      e = esysdep_fopen (zxqt_file, FALSE);
	      if (e == NULL)
		{
		  ulog_close ();
		  usysdep_exit (FALSE);
		}

	      fprintf (e, "U %s %s\n", zuser, zLocalname);
	      fprintf (e, "F %s\n", abtname);
	      fprintf (e, "C uucp -C %s %s!%s\n", zdata, qxqtsys->zname,
		       abdname);

	      if (fclose (e) != 0)
		ulog (LOG_FATAL, "fclose: %s", strerror (errno));
	    }

	  /* Tell the command execution to wait until the file has
	     been received, and tell it the real file name to use.
	     This isn't right if the file is not being used as
	     standard input, because it should be zsysdep_base_name
	     (zfile), but we can't call that because we're the wrong
	     system.  I don't know what to do about this.  */

	  if (finput)
	    {
	      uxadd_xqt_line ('F', ztemp, (char *) NULL);
	      uxadd_xqt_line ('I', ztemp, (char *) NULL);
	    }
	  else
	    {
	      uxadd_xqt_line ('F', ztemp, zfile);
	      pzargs[i] = zfile;
	    }
	}
    }

  /* If standard input is to be read from the stdin of uux, we read it
     here into a temporary file and send it to the execute system.  */

  if (fread_stdin)
    {
      const char *zdata;
      char abtname[CFILE_NAME_LEN];
      char abdname[CFILE_NAME_LEN];
      FILE *e;
      char ab[1024];

      zdata = zsysdep_data_file_name (qxqtsys, bgrade, abtname, abdname,
				      (char *) NULL);
      if (zdata == NULL)
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}

      e = esysdep_fopen (zdata, FALSE);
      if (e == NULL)
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}

      while (fgets (ab, sizeof ab, stdin) != NULL)
	if (fputs (ab, e) == EOF)
	  ulog (LOG_FATAL, "fputs: %s", strerror (errno));

      if (fclose (e) != 0)
	ulog (LOG_FATAL, "fclose: %s", strerror (errno));

      if (fxqtlocal)
	uxadd_xqt_line ('I', abtname, (const char *) NULL);
      else
	{
	  uxadd_xqt_line ('F', abdname, (const char *) NULL);
	  uxadd_xqt_line ('I', abdname, (const char *) NULL);
	  uxadd_send_file (abtname, abdname, "C", abtname);
	}
    }

  /* Here all the arguments have been determined, so the command
     can be written out.  */

  fprintf (eXxqt_file, "C %s", zcmd);

  for (i = 0; i < cargs; i++)
    if (pzargs[i] != NULL)
      fprintf (eXxqt_file, " %s", pzargs[i]);

  fprintf (eXxqt_file, "\n");

  /* Write out all the other miscellaneous junk.  */

  if (fno_ack)
    uxadd_xqt_line ('N', (const char *) NULL, (const char *) NULL);

  if (ferror_ack)
    uxadd_xqt_line ('Z', (const char *) NULL, (const char *) NULL);

  if (zrequestor != NULL)
    uxadd_xqt_line ('R', zrequestor, (const char *) NULL);

  if (fretstdin)
    uxadd_xqt_line ('B', (const char *) NULL, (const char *) NULL);

  if (zstatus_file != NULL)
    uxadd_xqt_line ('M', zstatus_file, (const char *) NULL);

  if (fclose (eXxqt_file) != 0)
    ulog (LOG_FATAL, "fclose: %s", strerror (errno));

  /* If the execution is to occur on another system, we must now
     arrange to copy the execute file to this system.  */

  if (! fxqtlocal)
    uxadd_send_file (abxqt_tname, abxqt_xname, "C", abxqt_tname);

  if (cXcmds > 0)
    {
      if (! fsysdep_spool_commands (qxqtsys, bgrade, cXcmds, pasXcmds))
	{
	  ulog_close ();
	  usysdep_exit (FALSE);
	}
    }

  /* If all that worked, make a log file entry.  All log file reports
     up to this point went to stderr, because ulog_program was never
     called.  */

  ulog_program ("uux");
  ulog_system (qxqtsys->zname);
  ulog_user (zuser);

  clen = strlen (zcmd) + 1;
  for (i = 0; i < cargs; i++)
    if (pzargs[i] != NULL)
      clen += strlen (pzargs[i]) + 1;

  zprint = (char *) alloca (clen);
  strcpy (zprint, zcmd);
  strcat (zprint, " ");
  for (i = 0; i < cargs; i++)
    {
      if (pzargs[i] != NULL)
	{
	  strcat (zprint, pzargs[i]);
	  strcat (zprint, " ");
	}
    }
  zprint[strlen (zprint) - 1] = '\0';

  ulog (LOG_NORMAL, "Queuing %s", zprint);

  ulog_close ();

  if (fuucico)
    usysdep_exit (fsysdep_run ("uucico -r1", TRUE));
  else
    usysdep_exit (TRUE);

  /* Avoid error about not returning a value.  */
  return 0;
}

/* Report command usage.  */

static void
uxusage ()
{
  fprintf (stderr,
	   "Usage: uux [options] [-] command\n");
  fprintf (stderr,
	   " -,-p: Read standard input for standard input of command\n");
  fprintf (stderr,
	   " -c,-l: Do not copy local files to spool directory (default)\n");
  fprintf (stderr,
	   " -C: Copy local files to spool directory\n");
  fprintf (stderr,
	   " -g grade: Set job grade (must be alphabetic)\n");
  fprintf (stderr,
	   " -n: Do not report completion status\n");
  fprintf (stderr,
	   " -z: Report completion status only on error\n");
  fprintf (stderr,
	   " -r: Do not start uucico daemon\n");
  fprintf (stderr,
	   " -a address: Address to mail status report to\n");
  fprintf (stderr,
	   " -b: Return standard input with status report\n");
  fprintf (stderr,
	   " -s file: Report completion status to file\n");
  fprintf (stderr,
	   " -j: Report job id\n");
  fprintf (stderr,
	   " -x debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use (default %s)\n",
	   CONFIGFILE);
#endif /* HAVE_TAYLOR_CONFIG */
  exit (EXIT_FAILURE);
}

/* Catch a signal.  We should clean up here, but so far we don't.  */

static sigret_t
uxcatch (isig)
     int isig;
{
  if (fAborting)
    {
      ulog_close ();
      usysdep_exit (FALSE);
    }
  else
    {
      ulog (LOG_ERROR, "Got signal %d", isig);
      ulog_close ();
      (void) signal (isig, SIG_DFL);
      raise (isig);
    }
}

/* Add a line to the execute file.  */

static void
uxadd_xqt_line (bchar, z1, z2)
     int bchar;
     const char *z1;
     const char *z2;
{
  if (z1 == NULL)
    fprintf (eXxqt_file, "%c\n", bchar);
  else if (z2 == NULL)
    fprintf (eXxqt_file, "%c %s\n", bchar, z1);
  else
    fprintf (eXxqt_file, "%c %s %s\n", bchar, z1, z2);
}

/* Add a file to be sent to the execute system.  */

static void
uxadd_send_file (zfrom, zto, zoptions, ztemp)
     const char *zfrom;
     const char *zto;
     const char *zoptions;
     const char *ztemp;
{
  struct scmd s;

  s.bcmd = 'S';
  s.pseq = NULL;
  s.zfrom = xstrdup (zfrom);
  s.zto = xstrdup (zto);
  s.zuser = zsysdep_login_name ();
  s.zoptions = xstrdup (zoptions);
  s.ztemp = xstrdup (ztemp);
  s.imode = 0666;
  s.znotify = "";
  s.cbytes = -1;

  ++cXcmds;
  pasXcmds = (struct scmd *) xrealloc ((pointer) pasXcmds,
				       cXcmds * sizeof (struct scmd));
  pasXcmds[cXcmds - 1] = s;
}
