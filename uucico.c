/* uucico.c
   This is the main UUCP communication program.

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
   Revision 1.39  1992/01/15  07:06:29  ian
   Set configuration directory in Makefile rather than sysdep.h

   Revision 1.38  1992/01/14  04:38:43  ian
   Chip Salzenberg: only declare sportinfo if it will be used

   Revision 1.37  1992/01/12  19:53:05  ian
   John Antypas: pass in sportinfo structure for fdo_call to use

   Revision 1.36  1992/01/05  03:09:17  ian
   Changed abProgram and abVersion to non const to avoid compiler bug

   Revision 1.35  1992/01/04  21:53:36  ian
   Start up uuxqt even if a call fails

   Revision 1.34  1991/12/31  19:43:13  ian
   Added 'e' protocol

   Revision 1.33  1991/12/28  04:33:09  ian
   Set fmasterdone correctly in slave mode

   Revision 1.32  1991/12/23  05:15:54  ian
   David Nugent: set debugging level for a specific system

   Revision 1.31  1991/12/21  23:10:43  ian
   Terry Gardner: record failed file transfers in statistics file

   Revision 1.30  1991/12/21  22:17:20  ian
   Change protocol ordering to 't', 'g', 'f'

   Revision 1.29  1991/12/21  22:07:47  ian
   John Theus: don't warn if port file does not exist

   Revision 1.28  1991/12/20  04:30:24  ian
   Terry Gardner: record conversation time in log file

   Revision 1.27  1991/12/20  00:42:24  ian
   Clear user name from error message given by getting next command

   Revision 1.26  1991/12/18  05:12:00  ian
   Added -l option to uucico to prompt for login name once and then exit

   Revision 1.25  1991/12/18  03:54:14  ian
   Made error messages to terminal appear more normal

   Revision 1.24  1991/12/17  04:55:01  ian
   David Nugent: ignore SIGHUP in uucico and uuxqt

   Revision 1.23  1991/12/15  03:42:33  ian
   Added tprocess_chat_cmd for all chat commands, and added CMDTABTYPE_PREFIX

   Revision 1.22  1991/12/11  03:59:19  ian
   Create directories when necessary; don't just assume they exist

   Revision 1.21  1991/11/21  22:17:06  ian
   Add version string, print version when printing usage

   Revision 1.20  1991/11/16  00:33:28  ian
   Remove ?: operator between string literal and variable

   Revision 1.19  1991/11/14  03:40:10  ian
   Try to figure out whether stdin is a TCP port

   Revision 1.18  1991/11/14  03:20:13  ian
   Added seven-bit and reliable commands to help when selecting protocols

   Revision 1.17  1991/11/13  23:08:40  ian
   Expand remote pathnames in uucp and uux; fix up uux special cases

   Revision 1.16  1991/11/12  19:47:04  ian
   Add called-chat set of commands to run a chat script on an incoming call

   Revision 1.15  1991/11/12  18:25:33  ian
   Added 't' protocol

   Revision 1.14  1991/11/11  23:47:24  ian
   Added chat-program to run a program to do a chat script

   Revision 1.13  1991/11/11  19:32:03  ian
   Added breceive_char to read characters through protocol buffering

   Revision 1.12  1991/11/11  18:55:52  ian
   Get protocol parameters from port and dialer for incoming calls

   Revision 1.11  1991/11/11  16:59:05  ian
   Eliminate fread_port_info, allow NULL pflock arg to ffind_port

   Revision 1.10  1991/11/11  04:21:16  ian
   Added 'f' protocol

   Revision 1.9  1991/11/10  19:24:22  ian
   Added pffile protocol entry point for file level control

   Revision 1.8  1991/11/09  18:53:07  ian
   Reworked protocol interface

   Revision 1.7  1991/11/07  18:15:38  ian
   Chip Salzenberg: move CMAXRETRIES to conf.h for easy configuration

   Revision 1.6  1991/09/19  03:06:04  ian
   Chip Salzenberg: put BNU temporary files in system's directory

   Revision 1.5  1991/09/19  02:30:37  ian
   From Chip Salzenberg: check whether signal is ignored differently

   Revision 1.4  1991/09/19  02:22:44  ian
   Chip Salzenberg's patch to allow ";retrytime" at the end of a time string

   Revision 1.3  1991/09/12  05:04:26  ian
   Changed sense of \0 return from btime_low_grade on calltimegrade

   Revision 1.2  1991/09/11  02:33:14  ian
   Added ffork argument to fsysdep_run
  
   Revision 1.1  1991/09/10  19:40:31  ian
   Initial revision
  
   */

#include "uucp.h"

#if USE_RCS_ID
char uucico_rcsid[] = "$Id$";
#endif

#include <string.h>
#include <ctype.h>
#include <signal.h>

#include "getopt.h"

#include "port.h"
#include "prot.h"
#include "system.h"

/* The program name.  */
char abProgram[] = "uucico";

/* Define the known protocols.
   bname, ffullduplex, qcmds, pfstart, pfshutdown, pfsendcmd, pzgetspace,
   pfsenddata, pfprocess, pfwait, pffile  */

static struct sprotocol asProtocols[] =
{
  { 't', FALSE, RELIABLE_ENDTOEND | RELIABLE_RELIABLE | RELIABLE_EIGHT,
      asTproto_params, ftstart, ftshutdown, ftsendcmd, ztgetspace,
      ftsenddata, ftprocess, ftwait, ftfile },
  { 'e', FALSE, RELIABLE_ENDTOEND | RELIABLE_RELIABLE | RELIABLE_EIGHT,
      asEproto_params, festart, feshutdown, fesendcmd, zegetspace,
      fesenddata, feprocess, fewait, fefile },
  { 'g', FALSE, RELIABLE_EIGHT,
      asGproto_params, fgstart, fgshutdown, fgsendcmd, zggetspace,
      fgsenddata, fgprocess, fgwait, NULL },
  { 'f', FALSE, RELIABLE_RELIABLE,
      asFproto_params, ffstart, ffshutdown, ffsendcmd, zfgetspace,
      ffsenddata, ffprocess, ffwait, fffile },
};

#define CPROTOCOLS (sizeof asProtocols / sizeof asProtocols[0])

/* Locked system.  */

static boolean fLocked_system;
static struct ssysteminfo sLocked_system;

/* Local functions.  */

static void uusage P((void));
static sigret_t ucatch P((int));
static boolean fcall P((const struct ssysteminfo *qsys,
			struct sport *qport,
			boolean fforce, int bgrade));
static boolean fdo_call P((const struct ssysteminfo *qsys,
			   struct sport *qport,
			   struct sstatus *qstat, int cretry,
			   boolean *pfcalled, struct sport *quse));
static boolean fcall_failed P((const struct ssysteminfo *qsys,
			       enum tstatus twhy, struct sstatus *qstat,
			       int cretry));
static boolean flogin_prompt P((struct sport *qport));
static boolean faccept_call P((const char *zlogin, struct sport *qport));
static boolean fuucp P((boolean fmaster, const struct ssysteminfo *qsys,
			int bgrade, boolean fnew));
static boolean fdo_xcmd P((const struct ssysteminfo *qsys,
			   const struct scmd *qcmd));
static boolean fok_to_send P((const char *zfrom, boolean flocal,
			      boolean fcaller,
			      const struct ssysteminfo *qsys,
			      const char *zuser));
static boolean fok_to_receive P((const char *zto, boolean flocal,
				 boolean fcaller,
				 const struct ssysteminfo *qsys,
				 const char *zuser));
static boolean frequest_ok P((boolean flocal, boolean fcaller,
			      const struct ssysteminfo *qsys,
			      const char *zuser));
static long cmax_size P((const struct ssysteminfo *qsys,
			 boolean flocal, boolean fcaller, boolean fsend));
static long cmax_size_ever P((const struct ssysteminfo *qsys,
			      boolean flocal, boolean fsend));
static long cmax_size_string P((const char *z));
static boolean fsend_uucp_cmd P((const char *z));
static const char *zget_uucp_cmd P((boolean freport));
static const char *zget_typed_line P((void));

/* Long getopt options.  */

static const struct option asLongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asLongopts;

int
main (argc, argv)
     int argc;
     char **argv;
{
  /* getopt return value  */
  int iopt;
  /* Configuration file name  */
  const char *zconfig = NULL;
  /* System to call  */
  const char *zsystem = NULL;
  /* Port to use; in master mode, call out on this port.  In slave mode,
     accept logins on this port.  If port not specified, then in master
     mode figure it out for each system, and in slave mode use stdin and
     stdout.  */
  const char *zport = NULL;
  /* Port information for the port name in zport.  */
  struct sport sportinfo;
  /* Pointer to port to use, or NULL if unknown.  */
  struct sport *qport;
  /* Whether to start uuxqt when done.  */
  boolean fuuxqt = TRUE;
  /* Whether to force a call despite status of previous call  */
  boolean fforce = FALSE;
  /* Whether we are the master  */
  boolean fmaster = FALSE;
  /* Command line debugging level  */
  int idebug = -1;
  /* Whether to give a single login prompt.  */
  boolean flogin = FALSE;
  /* Whether to do an endless loop of accepting calls  */
  boolean floop = FALSE;
  /* Whether to wait for an inbound call after doing an outbound call  */
  boolean fwait = FALSE;
  boolean fret = TRUE;
  int iholddebug;

  while ((iopt = getopt (argc, argv,
			 "efI:lp:qr:s:S:x:X:w")) != EOF)
    {
      switch (iopt)
	{
	case 'e':
	  /* Do an endless loop of accepting calls.  */
	  floop = TRUE;
	  break;

	case 'f':
	  /* Force a call even if it hasn't been long enough since the last
	     failed call.  */
	  fforce = TRUE;
	  break;

	case 'I':
	  /* Set configuration file name (default is in sysdep.h).  */
	  zconfig = optarg;
	  break;

	case 'l':
	  /* Prompt for login name and password.  */
	  flogin = TRUE;
	  break;

	case 'p':
	  /* Port to use  */
	  zport = optarg;
	  break;

	case 'q':
	  /* Don't start uuxqt.  */
	  fuuxqt = FALSE;
	  break;

	case 'r':
	  /* Set mode: -r1 for master, -r0 for slave (default)  */
	  if (optarg[0] == '1' && optarg[1] == '\0')
	    fmaster = TRUE;
	  else if (optarg[0] == '0' && optarg[1] == '\0')
	    fmaster = FALSE;
	  else
	    uusage ();
	  break;
    
	case 's':
	  /* Set system name  */
	  zsystem = optarg;
	  fmaster = TRUE;
	  break;

	case 'S':
	  /* Set system name and force call like -f  */
	  zsystem = optarg;
	  fforce = TRUE;
	  fmaster = TRUE;
	  break;

	case 'x':
	case 'X':
	  /* Set debugging level  */
	  idebug = atoi (optarg);
	  break;

	case 'w':
	  /* Call out and then wait for a call in  */
	  fwait = TRUE;
	  break;

	case 0:
	  /* Long option found, and flag value set.  */
	  break;

	default:
	  uusage ();
	  break;
	}
    }

  /* Any remaining argument is the name of the port to use  */
  if (optind < argc - 1)
    uusage ();
  else if (optind < argc)
    zport = argv[optind];
  
  uread_config (zconfig);

  /* Now set debugging level from command line arguments (overriding
     configuration file).  */
  if (idebug != -1)
    iDebug = idebug;

#ifdef SIGINT
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    (void) signal (SIGINT, ucatch);
#endif
#ifdef SIGHUP
  (void) signal (SIGHUP, SIG_IGN);
#endif
#ifdef SIGQUIT
  if (signal (SIGQUIT, SIG_IGN) != SIG_IGN)
    (void) signal (SIGQUIT, ucatch);
#endif
#ifdef SIGTERM
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    (void) signal (SIGTERM, ucatch);
#endif
#ifdef SIGPIPE
  if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
    (void) signal (SIGPIPE, ucatch);
#endif
#ifdef SIGABRT
  (void) signal (SIGABRT, ucatch);
#endif

  usysdep_initialize (TRUE);

  ulog_to_file (TRUE);

  /* If a port was named, get its information.  */
  if (zport == NULL)
    qport = NULL;
  else
    {
      if (! ffind_port (zport, (long) 0, (long) 0, &sportinfo,
			(boolean (*) P((struct sport *, boolean))) NULL,
			FALSE))
	{
	  ulog (LOG_ERROR, "%s: No such port", zport);
	  ulog_close ();
	  usysdep_exit (FALSE);
	}
      qport = &sportinfo;
    }

  if (fmaster)
    {
      /* If a system was named, call it up.  Otherwise check all the
	 known systems, and call all the ones which have work to do.  */
      if (zsystem != NULL)
	{
	  if (! fread_system_info (zsystem, &sLocked_system))
	    ulog (LOG_FATAL, "Unknown system %s", zsystem);

	  ulog_system (sLocked_system.zname);

	  iholddebug = iDebug;
	  if (sLocked_system.idebug != -1)
	    iDebug = sLocked_system.idebug;

	  if (! fsysdep_lock_system (&sLocked_system))
	    {
	      ulog (LOG_ERROR, "System already locked");
	      fret = FALSE;
	    }
	  else
	    {
	      fLocked_system = TRUE;
	      fret = fcall (&sLocked_system, qport, fforce, BGRADE_HIGH);
	      (void) fsysdep_unlock_system (&sLocked_system);
	      fLocked_system = FALSE;
	    }

	  iDebug = iholddebug;

	  ulog_system ((const char *) NULL);
	}
      else
	{
	  int csystems;
	  struct ssysteminfo *pas;
	  int i;
	  char bgrade;
	  boolean fdidone;

	  fret = TRUE;
	  fdidone = FALSE;
	  uread_all_system_info (&csystems, &pas);
	  for (i = 0; i < csystems; i++)
	    {
	      if (fsysdep_has_work (&pas[i], &bgrade))
		{
		  fdidone = TRUE;

		  ulog_system (pas[i].zname);

		  iholddebug = iDebug;
		  if (pas[i].idebug != -1)
		    iDebug = pas[i].idebug;

		  if (! fsysdep_lock_system (&pas[i]))
		    {
		      ulog (LOG_ERROR, "System already locked");
		      fret = FALSE;
		    }
		  else
		    {
		      sLocked_system = pas[i];
		      fLocked_system = TRUE;
		      if (! fcall (&pas[i], qport, fforce, bgrade))
			fret = FALSE;
		      (void) fsysdep_unlock_system (&pas[i]);
		      fLocked_system = FALSE;
		    }

		  iDebug = iholddebug;

		  ulog_system ((const char *) NULL);
		}
	    }

	  if (! fdidone)
	    ulog (LOG_NORMAL, "No work");
	}

      /* If requested, wait for calls after dialing out.  */
      if (fwait)
	{
	  floop = TRUE;
	  fmaster = FALSE;
	}
    }

  if (! fmaster)
    {
      /* If a port was specified by name, we go into endless loop
	 mode.  In this mode, we wait for calls and prompt them with
	 "login:" and "Password:", so that they think we are a regular
	 UNIX system.  If we aren't in endless loop mode, we have been
	 called by some other system.  If flogin is TRUE, we prompt
	 with "login:" and "Password:" a single time.  */

      fret = TRUE;

      if (qport != NULL)
	{
	  floop = TRUE;
	  if (! fport_lock (qport, TRUE))
	    {
	      ulog (LOG_ERROR, "Port %s is locked", qport->zname);
	      fret = FALSE;
	    }
	}

      if (fret)
	{
	  if (! fport_open (qport, (long) 0, (long) 0, TRUE))
	    fret = FALSE;
	}

      if (fret)
	{
	  if (floop)
	    {
	      while (flogin_prompt (qport))
		{
		  if (fLocked_system)
		    {
		      (void) fsysdep_unlock_system (&sLocked_system);
		      fLocked_system = FALSE;
		    }
		  if (! fport_reset ())
		    break;
		}
	      fret = FALSE;
	    }
	  else
	    {
	      if (flogin)
		fret = flogin_prompt (qport);
	      else
		{
		  const char *zlogin;

		  zlogin = zsysdep_login_name ();
		  if (zlogin == NULL)
		    ulog (LOG_FATAL, "Can't get login name");
		  iholddebug = iDebug;
		  fret = faccept_call (zlogin, qport);
		  iDebug = iholddebug;
		}
	    }

	  (void) fport_close (fret);

	  if (fLocked_system)
	    {
	      (void) fsysdep_unlock_system (&sLocked_system);
	      fLocked_system = FALSE;
	    }
	}
    }

  ulog_close ();
  ustats_close ();

  if (fuuxqt)
    usysdep_exit (fsysdep_run ("uuxqt", FALSE));
  else
    usysdep_exit (fret);

  /* Avoid complaints about not returning.  */
  return 0;
}

/* Print out a usage message.  */

static void
uusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991 Ian Lance Taylor\n",
	   abVersion);
  fprintf (stderr,
	   "Usage: uucico [options] [port]\n");
  fprintf (stderr,
	   " Specifying a port implies -e on the named port\n");
  fprintf (stderr,
	   " -s,-S system: Call system (-S implies -f)\n");
  fprintf (stderr,
	   " -f: Force call despite system status\n");
  fprintf (stderr,
	   " -r state: 1 for master, 0 for slave (default)\n");
  fprintf (stderr,
	   " -p port: Specify port (implies -e)\n");
  fprintf (stderr,
	   " -l: prompt for login name and password\n");
  fprintf (stderr,
	   " -e: Endless loop of login prompts and daemon execution\n");
  fprintf (stderr,
	   " -w: After calling out, wait for incoming calls\n");
  fprintf (stderr,
	   " -q: Don't start uuxqt when done\n");
  fprintf (stderr,
	   " -x,-X debug: Set debugging level\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use (default %s%s)\n",
	   NEWCONFIGLIB, CONFIGFILE);
#endif /* HAVE_TAYLOR_CONFIG */

  exit (EXIT_FAILURE);
}

/* Catch a signal.  Clean up and die.  */

static sigret_t
ucatch (isig)
     int isig;
{
  ustats_failed ();

  ulog_system ((const char *) NULL);
  ulog_user ((const char *) NULL);

  if (! fAborting)
    ulog (LOG_ERROR, "Got signal %d", isig);

  if (qPort != NULL)
    (void) fport_close (FALSE);

  if (fLocked_system)
    {
      (void) fsysdep_unlock_system (&sLocked_system);
      fLocked_system = FALSE;
    }

  ulog_close ();
  ustats_close ();

  (void) signal (isig, SIG_DFL);

  if (fAborting)
    usysdep_exit (FALSE);
  else
    raise (isig);
}

/* Call another system, trying all the possible sets of calling
   instructions.  The fprepare_call function should already have been
   called.  The qsys argument is the system to call.  The qport
   argument is the port to use, and may be NULL.  If the fforce
   argument is TRUE, a call is forced even if not enough time has
   passed since the last failed call.  The bgrade argument is the
   highest grade of work to be done for the system.  The qstat
   argument holds the status of the system.  */

static boolean fcall (qsys, qport, fforce, bgrade)
     const struct ssysteminfo *qsys;
     struct sport *qport;
     boolean fforce;
     int bgrade;
{
  boolean fbadtime;
  struct sstatus sstat;

  if (! fsysdep_get_status (qsys, &sstat))
    return FALSE;

  /* Make sure it's been long enough since the last failed call.  */
  if (! fforce)
    {
#ifdef CMAXRETRIES
#if CMAXRETRIES > 0
      if (sstat.cretries >= CMAXRETRIES)
	{
	  ulog (LOG_ERROR, "Too many retries");
	  return FALSE;
	}
#endif /* CMAXRETRIES > 0 */
#endif /* defined (CMAXRETRIES) */

      if (sstat.ttype != STATUS_COMPLETE
	  && sstat.ilast + sstat.cwait > isysdep_time ())
	{
	  ulog (LOG_ERROR, "Retry time not reached");
	  return FALSE;
	}
    }

  fbadtime = TRUE;

  do
    {
      const struct ssysteminfo *qnext;
      int cretry;

      cretry = ccheck_time (bgrade, qsys->ztime);
      if (cretry >= 0)
	{
	  boolean fret, fcalled;
	  struct sport sportinfo;
	  
	  fbadtime = FALSE;

	  fret = fdo_call (qsys, qport, &sstat, cretry, &fcalled,
			   &sportinfo);
	  (void) fport_close (fret);
	  if (fret)
	    return TRUE;
	  if (fcalled)
	    return FALSE;
	}

      /* Look for the next alternate with different calling
	 instructions.  */
      qnext = qsys;
      do
	{
	  qnext = qnext->qalternate;
	}
      while (qnext != NULL
	     && qsys->ztime == qnext->ztime
	     && qsys->zport == qnext->zport
	     && qsys->qport == qnext->qport
	     && qsys->ibaud == qnext->ibaud
	     && qsys->zphone == qnext->zphone
	     && qsys->schat.zprogram == qnext->schat.zprogram
	     && qsys->schat.zchat == qnext->schat.zchat);

      qsys = qnext;
    }
  while (qsys != NULL);

  if (fbadtime)
    ulog (LOG_ERROR, "Wrong time to call");

  return FALSE;
}

/* Do the actual work of calling another system, such as dialing and
   logging in.  The qsys argument is the system to call, the qport
   argument is the port to use, and the qstat argument holds the
   current status of the ssystem.  If we log in successfully, set
   *pfcalled to TRUE; this is used to distinguish a failed dial from a
   failure during the call.  The quse argument is passed in because
   this function does not call fport_close, so if it reads in a port
   structure to open it must not keep it on the stack.  */

static boolean fdo_call (qsys, qport, qstat, cretry, pfcalled, quse)
     const struct ssysteminfo *qsys;
     struct sport *qport;
     struct sstatus *qstat;
     int cretry;
     boolean *pfcalled;
     struct sport *quse;
{
  const char *zstr;
  boolean fnew;
  int cdial_proto_params;
  struct sproto_param *qdial_proto_params;
  int idial_reliable;
  long istart_time;

  *pfcalled = FALSE;

  /* If no port was specified on the command line, use any port
     defined for the system.  To select the system port: 1) see if
     port information was specified directly; 2) see if a port was
     named; 3) get an available port given the baud rate.  We don't
     change the system status if a port is unavailable; i.e. we don't
     force the system to wait for the retry time.  */

  if (qport == NULL)
    qport = qsys->qport;
  if (qport != NULL)
    {
      if (! fport_lock (qport, FALSE))
	{
	  ulog (LOG_ERROR, "Port \"%s\" already locked", qport->zname);
	  return FALSE;
	}
    }
  else
    {
      if (! ffind_port (qsys->zport, qsys->ibaud, qsys->ihighbaud,
			quse, fport_lock, TRUE))
	return FALSE;
      qport = quse;
      /* The port is locked by ffind_port.  */
    }

  ulog (LOG_NORMAL, "Calling system %s", qsys->zname);

  /* Now try to call the system.  */

  if (! fport_open (qport, qsys->ibaud, qsys->ihighbaud, FALSE))
    {
      (void) fcall_failed (qsys, STATUS_PORT_FAILED, qstat, cretry);
      return FALSE;
    }

  cdial_proto_params = 0;
  qdial_proto_params = NULL;
  if (! fport_dial (qsys, &cdial_proto_params, &qdial_proto_params,
		    &idial_reliable))
    {
      (void) fcall_failed (qsys, STATUS_DIAL_FAILED, qstat, cretry);
      return FALSE;
    }

  if (! fchat (&qsys->schat, qsys, (const struct sdialer *) NULL,
	       (const char *) NULL, FALSE, qPort->zname, iport_baud ()))
    {
      (void) fcall_failed (qsys, STATUS_LOGIN_FAILED, qstat, cretry);
      return FALSE;
    }

  qstat->ttype = STATUS_TALKING;
  qstat->ilast = isysdep_time ();
  qstat->cretries = 0;
  qstat->cwait = 0;
  if (! fsysdep_set_status (qsys, qstat))
    return FALSE;

  ulog (LOG_NORMAL, "Login successful", qsys->zname);

  *pfcalled = TRUE;
  istart_time = isysdep_time ();

  /* We should now see "Shere" from the other system.  Apparently
     some systems send "Shere=foo" where foo is the remote name.  */

  zstr = zget_uucp_cmd (TRUE);
  if (zstr == NULL)
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      return FALSE;
    }

  if (strncmp (zstr, "Shere", 5) != 0)
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      ulog (LOG_ERROR, "Bad initialization string");
      return FALSE;
    }

  if (zstr[5] == '=')
    {
      if (strcmp(zstr + 6, qsys->zname) != 0)
	{
	  (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat,
			       cretry);
	  ulog (LOG_ERROR, "Called wrong system (%s)", zstr + 6);
	  return FALSE;
	}
    }
#if DEBUG > 0
  else if (zstr[5] != '\0')
      ulog (LOG_DEBUG, "fdo_call: Strange Shere: %s", zstr);
#endif

  /* We now send "S" name switches, where name is our UUCP name.  We
     send a -x switch if we are debugging.  If we are using sequence
     numbers with this system, we send a -Q argument with the sequence
     number.  If the call-timegrade command was used, we send a -p
     argument and a -vgrade= argument with the grade to send us (we
     send both argument to make it more likely that one is
     recognized).  We always send a -N (for new) switch to indicate
     that we are prepared to accept file sizes.  */
  {
    char bgrade;
    const char *zuse_local;
    char *zsend;

    /* If the call-timegrade command used, determine the grade we
       should request of the other system.  */
    if (qsys->zcalltimegrade == NULL)
      bgrade = '\0';
    else
      {
	bgrade = btime_low_grade (qsys->zcalltimegrade);
	/* A \0 in this case means that no restrictions have been made.  */
      }

    if (qsys->zlocalname != NULL)
      zuse_local = qsys->zlocalname;
    else
      zuse_local = zLocalname;

    zsend = (char *) alloca (strlen (zuse_local) + 50);
    if (! qsys->fsequence)
      {
	if (bgrade == '\0')
	  sprintf (zsend, "S%s -N", zuse_local);
	else
	  sprintf (zsend, "S%s -p%c -vgrade=%c -N", zuse_local, bgrade,
		   bgrade);
      }
    else
      {
	long iseq;

	iseq = isysdep_get_sequence (qsys);
	if (iseq < 0)
	  {
	    (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat,
				 cretry);
	    return FALSE;
	  }
	if (bgrade == '\0')
	  sprintf (zsend, "S%s -Q%ld -N", zuse_local, iseq);
	else
	  sprintf (zsend, "S%s -Q%ld -p%c -vgrade=%c -N", zuse_local, iseq,
		   bgrade, bgrade);
      }

    if (! fsend_uucp_cmd (zsend))
      {
	(void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat,
			     cretry);
	return FALSE;
      }
  }

  /* Now we should see ROK or Rreason where reason gives a cryptic
     reason for failure.  If we are talking to a counterpart, we will
     get back ROKN.  */
  zstr = zget_uucp_cmd (TRUE);
  if (zstr == NULL)
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      return FALSE;
    }

  if (zstr[0] != 'R')
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      ulog (LOG_ERROR, "Bad reponse to handshake string (%s)",
	    zstr);
      return FALSE;
    }

  if (strcmp (zstr + 1, "OKN") == 0)
    fnew = TRUE;
  else if (strcmp (zstr + 1, "OK") == 0)
    fnew = FALSE;
  else if (strcmp (zstr + 1, "CB") == 0)
    {
      ulog (LOG_NORMAL, "Remote system will call back");
      qstat->ttype = STATUS_COMPLETE;
      (void) fsysdep_set_status (qsys, qstat);
      return TRUE;
    }
  else
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      ulog (LOG_ERROR, "Handshake failed (%s)", zstr + 1);
      return FALSE;
    }

  /* The slave should now send \020Pprotos\0 where protos is a list of
     supported protocols.  Each protocol is a single character.  */

  zstr = zget_uucp_cmd (TRUE);
  if (zstr == NULL)
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      return FALSE;
    }

  if (zstr[0] != 'P')
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
      ulog (LOG_ERROR, "Bad protocol handshake (%s)", zstr);
      return FALSE;
    }

  /* Now decide which protocol to use.  The system entry may have its
     own list of protocols.  */
  {
    int i;
    char ab[5];

    i = CPROTOCOLS;
    if (qsys->zprotocols != NULL)
      {
	const char *zproto;

	for (zproto = qsys->zprotocols; *zproto != '\0'; zproto++)
	  {
	    if (strchr (zstr + 1, *zproto) != NULL)
	      {
		for (i = 0; i < CPROTOCOLS; i++)
		  if (asProtocols[i].bname == *zproto)
		    break;
		if (i < CPROTOCOLS)
		  break;
	      }
	  }
      }
    else
      {
	int ir;

	/* If the system did not specify a list of protocols, we want
	   only protocols that match the known reliability of the
	   dialer and the port.  If we have no information, we default
	   to a reliable eight bit connection.  */

	ir = 0;
	if ((qPort->ireliable & RELIABLE_SPECIFIED) != 0)
	  ir = qPort->ireliable;
	if ((idial_reliable & RELIABLE_SPECIFIED) != 0)
	  {
	    if (ir != 0)
	      ir &= idial_reliable;
	    else
	      ir = idial_reliable;
	  }
	if (ir == 0)
	  ir = RELIABLE_RELIABLE | RELIABLE_EIGHT | RELIABLE_SPECIFIED;

	for (i = 0; i < CPROTOCOLS; i++)
	  {
	    int ipr;

	    ipr = asProtocols[i].ireliable;
	    if ((ipr & ir) != ipr)
	      continue;
	    if (strchr (zstr + 1, asProtocols[i].bname) != NULL)
	      break;
	  }
      }

    if (i >= CPROTOCOLS)
      {
	(void) fsend_uucp_cmd ("UN");
	(void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
	ulog (LOG_ERROR, "No mutually supported protocols");
	return FALSE;
      }

    qProto = &asProtocols[i];

    sprintf (ab, "U%c", qProto->bname);
    if (! fsend_uucp_cmd (ab))
      {
	(void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat, cretry);
	return FALSE;
      }
  }

  /* Run any protocol parameter commands.  */

  if (qProto->qcmds != NULL)
    {
      if (qsys->cproto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     qsys->cproto_params, qsys->qproto_params);
      if (qPort->cproto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     qPort->cproto_params, qPort->qproto_params);
      if (cdial_proto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     cdial_proto_params, qdial_proto_params);
    }

  /* Turn on the selected protocol.  */

  if (! (*qProto->pfstart)(TRUE))
    {
      (void) fcall_failed (qsys, STATUS_HANDSHAKE_FAILED, qstat,
			   cretry);
      return FALSE;
    }

  /* Now we have succesfully logged in as the master.  */

  ulog (LOG_NORMAL, "Handshake successful");

  {
    boolean fret;

    fret = fuucp (TRUE, qsys, '\0', fnew);
    ulog_user ((const char *) NULL);
    usysdep_get_work_free (qsys);

    /* If we jumped out due to an error, shutdown the protocol.  */
    if (! fret)
      {
	(void) (*qProto->pfshutdown) ();
	ustats_failed ();
      }

    /* Now send the hangup message.  As the caller, we send six O's
       and expect to receive seven O's.  We send the six O's twice
       to help the other side.  We don't worry about errors here.  */
    if (fsend_uucp_cmd ("OOOOOO")
	&& fsend_uucp_cmd ("OOOOOO"))
      {
	if (fret)
	  {
	    zstr = zget_uucp_cmd (FALSE);
#if DEBUG > 0
	    if (zstr != NULL)
	      {
		/* The Ultrix UUCP only sends six O's, although I think
		   it should send seven.  Because of this, we only
		   check for six.  */
		if (strstr (zstr, "OOOOOO") == NULL)
		  ulog (LOG_DEBUG, "No hangup from remote");
	      }
#endif
	  }
      }

    ulog (LOG_NORMAL, "Call complete (%ld seconds)",
	  isysdep_time () - istart_time);

    if (! fret)
      {
	(void) fcall_failed (qsys, STATUS_FAILED, qstat, cretry);
	return FALSE;
      }
    else
      {
	qstat->ttype = STATUS_COMPLETE;
	(void) fsysdep_set_status (qsys, qstat);
	return TRUE;
      }
  }
}

/* A small helper routine to write out the system status when something
   goes wrong.  */

static boolean
fcall_failed (qsys, twhy, qstat, cretry)
     const struct ssysteminfo *qsys;
     enum tstatus twhy;
     struct sstatus *qstat;
     int cretry;
{
#if DEBUG > 2
  if (iDebug > 2)
    ulog (LOG_DEBUG, "fcall_failed: Cause %d (%s)", twhy,
	  azStatus[(int) twhy]);
#endif
  qstat->ttype = twhy;
  qstat->cretries++;
  qstat->ilast = isysdep_time ();
  if (cretry == 0)
    qstat->cwait = CRETRY_WAIT (qstat->cretries);
  else
    qstat->cwait = cretry * 60;
  return fsysdep_set_status (qsys, qstat);
}

/* Prompt for a login name and a password, and run as the slave.  */

static boolean flogin_prompt (qport)
     struct sport *qport;
{
  const char *zuser, *zpass;

#if DEBUG > 0
  if (iDebug > 0)
    ulog (LOG_DEBUG, "flogin_prompt: Waiting for login");
#endif

  do
    {
      if (! fport_write ("login: ", sizeof "login: " - 1))
	return FALSE;
      zuser = zget_typed_line ();
    }
  while (zuser != NULL && *zuser == '\0');

  if (zuser != NULL)
    {
      char *zhold;

      zhold = (char *) alloca (strlen (zuser) + 1);
      strcpy (zhold, zuser);

      if (! fport_write ("Password:", sizeof "Password:" - 1))
	return FALSE;

      zpass = zget_typed_line ();
      if (zpass != NULL)
	{
	  if (fcheck_login (zhold, zpass))
	    {
	      int iholddebug;

	      /* We ignore the return value of faccept_call because we
		 really don't care whether the call succeeded or not.
		 We are going to reset the port anyhow.  */
	      iholddebug = iDebug;
	      (void) faccept_call (zhold, qport);
	      iDebug = iholddebug;
	    }
	}
    }

  return TRUE;
}

/* Accept a call from a remote system.  */

static boolean faccept_call (zlogin, qport)
     const char *zlogin;
     struct sport *qport;
{
  long istart_time;
  int cdial_proto_params;
  struct sproto_param *qdial_proto_params;
  int idial_reliable;
  boolean ftcp_port;
  const char *zport;
  char *zsend, *zspace;
  const char *zstr;
  struct ssysteminfo ssys;
  const struct ssysteminfo *qsys;
  boolean fnew;
  char bgrade;
  const char *zuse_local;
#if HAVE_TAYLOR_CONFIG
  struct sport sportinfo;
#endif

  ulog (LOG_NORMAL, "Incoming call");
  istart_time = isysdep_time ();

  /* Figure out protocol parameters determined by the port.  If no
     port was specified we're reading standard input, so try to get
     the port name and read information from the port file.  We only
     use the port information to get protocol parameters; we don't
     want to start treating the port as though it were a modem, for
     example.  */

  if (qport != NULL)
    {
      zport = qport->zname;
      ftcp_port = FALSE;
    }
  else
    {
      zport = zsysdep_port_name (&ftcp_port);

      /* If the ``portfile'' command was not used to change the
	 default portfile, and the default portfile does not exist,
	 then don't try to look up the port.  This keeps a slave
	 uucico from putting an error message in the log file saying
	 that the port file does not exist.  The information we want
	 from the port is only known for HAVE_TAYLOR_CONFIG, so if we
	 don't have that don't even bother to look up the port.  */

#if HAVE_TAYLOR_CONFIG

      if (zport != NULL)
	{
	  boolean fcheck;

	  fcheck = FALSE;
	  if (fsysdep_file_exists (zPortfile))
	    fcheck = TRUE;
	  else
	    {
	      char *zportfile;

	      zportfile = (char *) alloca (sizeof NEWCONFIGLIB
					   + sizeof PORTFILE);
	      sprintf (zportfile, "%s%s", NEWCONFIGLIB, PORTFILE);
	      if (strcmp (zportfile, zPortfile) != 0)
		fcheck = TRUE;
	    }

	  if (fcheck
	      && ffind_port (zport, (long) 0, (long) 0, &sportinfo,
			     (boolean (*) P((struct sport *, boolean))) NULL,
			     FALSE))
	    qport = &sportinfo;
	}

#endif /* HAVE_TAYLOR_CONFIG */

      if (zport == NULL)
	zport = "unknown";
    }

  /* If we've managed to figure out that this is a modem port, now try
     to get protocol parameters from the dialer.  */

  cdial_proto_params = 0;
  qdial_proto_params = NULL;
  idial_reliable = 0;
  if (qport != NULL)
    {
      if (qport->ttype == PORTTYPE_MODEM)
	{
	  if (qport->u.smodem.zdialer != NULL)
	    {
	      char *zcopy;
	      char *zdial;
	      struct sdialer sdialerinfo;

	      /* We use the first dialer in the sequence.  */
	      zcopy = (char *) alloca (strlen (qport->u.smodem.zdialer)
				       + 1);
	      strcpy (zcopy, qport->u.smodem.zdialer);

	      zdial = strtok (zcopy, " \t");
	      if (fread_dialer_info (zdial, &sdialerinfo))
		{
		  cdial_proto_params = sdialerinfo.cproto_params;
		  qdial_proto_params = sdialerinfo.qproto_params;
		  idial_reliable = sdialerinfo.ireliable;
		}
	    }
	  else if (qport->u.smodem.qdialer != NULL)
	    {
	      cdial_proto_params = qport->u.smodem.qdialer->cproto_params;
	      qdial_proto_params = qport->u.smodem.qdialer->qproto_params;
	      idial_reliable = qport->u.smodem.qdialer->ireliable;
	    }
	}	  
#if HAVE_TCP
      else if (qport->ttype == PORTTYPE_TCP)
	ftcp_port = TRUE;
#endif
    }

  /* If it's a TCP port, it's fully reliable.  Even if HAVE_TCP is not
     supported, zsysdep_port_name may be able to figure this out (not
     on Unix, though).  */
  if (ftcp_port)
    idial_reliable = (RELIABLE_SPECIFIED | RELIABLE_ENDTOEND
		      | RELIABLE_RELIABLE | RELIABLE_EIGHT);

  /* We have to check to see whether some system uses this login name
     to indicate a different local name.  Obviously, this means that
     any system which uses this login name must expect the alternate
     system name.  */
  zuse_local = NULL;
  if (fUnknown_ok)
    {
      for (qsys = &sUnknown; qsys != NULL;  qsys = qsys->qalternate)
	{
	  if (qsys->zlocalname != NULL
	      && qsys->zcalled_login != NULL
	      && strcmp (qsys->zcalled_login, zlogin) == 0)
	    {
	      zuse_local = qsys->zlocalname;
	      break;
	    }
	}
    }
  if (zuse_local == NULL)
    {
      struct ssysteminfo *pas;
      int isys, csystems;

      zuse_local = zLocalname;

      uread_all_system_info (&csystems, &pas);
      for (isys = 0; isys < csystems; isys++)
	{
	  for (qsys = &pas[isys]; qsys != NULL; qsys = qsys->qalternate)
	    {
	      if (qsys->zlocalname != NULL
		  && qsys->zcalled_login != NULL
		  && strcmp (qsys->zcalled_login, zlogin) == 0)
		{
		  zuse_local = qsys->zlocalname;
		  break;
		}
	    }
	  if (qsys != NULL)
	    break;
	}
    }

  /* Tell the remote system who we are.   */
  zsend = (char *) alloca (strlen (zuse_local) + 10);
  sprintf (zsend, "Shere=%s", zuse_local);
  if (! fsend_uucp_cmd (zsend))
    return FALSE;

  zstr = zget_uucp_cmd (TRUE);
  if (zstr == NULL)
    return FALSE;

  if (zstr[0] != 'S')
    {
      ulog (LOG_ERROR, "Bad introduction string");
      return FALSE;
    }
  ++zstr;

  zspace = strchr (zstr, ' ');
  if (zspace != NULL)
    *zspace = '\0';
  if (fread_system_info (zstr, &ssys))
    qsys = &ssys;
  else
    {
      /* We have no information on this system.  */
      if (! fUnknown_ok)
	{
	  (void) fsend_uucp_cmd ("RYou are unknown to me");
	  ulog (LOG_ERROR, "Call from unknown system %s", zstr);
	  return FALSE;
	}

      /* We have to translate the name to a canonical form for the
	 benefit of systems which only allow short system names.  */
      sUnknown.zname = ztranslate_system (zstr);
      if (sUnknown.zname == NULL)
	{
	  (void) fsend_uucp_cmd ("RYou are unknown to me");
	  return FALSE;
	}

      qsys = &sUnknown;
    }

  if (! fcheck_validate (zlogin, qsys->zname))
    {
      (void) fsend_uucp_cmd ("RLOGIN");
      ulog (LOG_ERROR, "System %s used wrong login name %s",
	    zstr, zlogin);
      return FALSE;
    }

  if (qsys->zcalled_login != NULL)
    {
      const struct ssysteminfo *qany;

      /* Choose an alternate system definition based on the
	 login name.  */
      qany = NULL;
      for (; qsys != NULL; qsys = qsys->qalternate)
	{
	  if (qsys->zcalled_login != NULL)
	    {
	      if (qany == NULL
		  && strcmp (qsys->zcalled_login, "ANY") == 0)
		qany = qsys;
	      else if (strcmp (qsys->zcalled_login, zlogin) == 0)
		break;
	    }
	}

      if (qsys == NULL)
	{
	  if (qany == NULL)
	    {
	      (void) fsend_uucp_cmd ("RLOGIN");
	      ulog (LOG_ERROR, "System %s used wrong login name %s",
		    zstr, zlogin);
	      return FALSE;
	    }
	  qsys = qany;
	}
    }

  ulog_system (qsys->zname);

  if (qsys->idebug != -1)
    iDebug = qsys->idebug;

  /* See if we are supposed to call the system back.  This will queue
     up an empty command.  It would be better to actually call back
     directly at this point as well.  */
  if (qsys->fcallback)
    {
      (void) fsend_uucp_cmd ("RCB");
      ulog (LOG_NORMAL, "Will call back");
      (void) fsysdep_spool_commands (qsys, BGRADE_HIGH, 0,
				     (const struct scmd *) NULL);
      return TRUE;
    }

  /* We only permit one call at a time from a remote system.  Lock it.  */
  if (! fsysdep_lock_system (qsys))
    {
      (void) fsend_uucp_cmd ("RLCK");
      ulog (LOG_ERROR, "System already locked");
      return FALSE;
    }
  sLocked_system = *qsys;
  fLocked_system = TRUE;

  /* Check the arguments of the remote system.  We accept -x# to set
     out debugging level and -Q# for a sequence number.  We may insist
     on a sequence number.  The -p and -vgrade= arguments are taken to
     specify the lowest job grade that we should transfer; I think
     this is the traditional meaning, but I don't know.  The -N switch
     means that we are talking to another instance of ourselves.  */

  fnew = FALSE;
  bgrade = BGRADE_LOW;

  if (zspace == NULL)
    {
      if (qsys != NULL && qsys->fsequence)
	{
	  (void) fsend_uucp_cmd ("RBADSEQ");
	  ulog (LOG_ERROR, "No sequence number (call rejected)");
	  return FALSE;
	}
    }
  else
    {
      ++zspace;
      while (isspace (BUCHAR (*zspace)))
	++zspace;

      while (*zspace != '\0')
	{
	  boolean frecognized;
	  char *znext;
	  
	  frecognized = FALSE;
	  if (*zspace == '-')
	    {
	      switch (zspace[1])
		{
		case 'x':
		  frecognized = TRUE;
		  iDebug = atoi (zspace + 2);
		  ulog (LOG_NORMAL, "Setting debugging mode to %d",
			iDebug);
		  break;
		case 'Q':
		  frecognized = TRUE;
		  {
		    long iseq;

		    if (! qsys->fsequence)
		      break;
		    iseq = atol (zspace + 2);
		    if (iseq != isysdep_get_sequence (qsys))
		      {
			(void) fsend_uucp_cmd ("RBADSEQ");
			ulog (LOG_ERROR, "Out of sequence call rejected");
			return FALSE;
		      }
		  }
		  break;
		case 'p':
		  /* We don't accept a space between the -p and the
		     grade, although we should.  */
		  frecognized = TRUE;
		  if (FGRADE_LEGAL (zspace[2]))
		    bgrade = zspace[2];
		  break;
		case 'v':
		  if (strncmp (zspace + 1, "vgrade=",
			       sizeof "vgrade=" - 1) == 0)
		    {
		      frecognized = TRUE;
		      if (FGRADE_LEGAL (zspace[sizeof "vgrade="]))
			bgrade = zspace[sizeof "vgrade="];
		    }
		  break;
		case 'N':
		  frecognized = TRUE;
		  fnew = TRUE;
		  break;
		default:
		  break;
		}
	    }

	  znext = zspace;
	  while (*znext != '\0' && ! isspace (BUCHAR (*znext)))
	    ++znext;

	  if (! frecognized)
	    {
	      int clen;
	      char *zcopy;

	      /* We could just use %.*s for this, but it's probably
		 not portable.  */
	      clen = znext - zspace;
	      zcopy = (char *) alloca (clen + 1);
	      strncpy (zcopy, zspace, clen);
	      zcopy[clen] = '\0';
	      ulog (LOG_NORMAL, "Unrecognized argument %s", zcopy);
	    }

	  zspace = znext;
	  while (isspace (BUCHAR (*zspace)))
	    ++zspace;
	}
    }

  /* We recognized the system, and the sequence number (if any) was
     OK.  Send an ROK, and send a list of protocols.  If we got the -N
     switch, send ROKN to confirm it.  */

  if (! fsend_uucp_cmd (fnew ? "ROKN" : "ROK"))
    return FALSE;

  {
    int i;
   
    if (qsys->zprotocols != NULL)
      {
	zsend = (char *) alloca (strlen (qsys->zprotocols) + 2);
	sprintf (zsend, "P%s", qsys->zprotocols);
      }
    else
      {
	char *zset;
	int ir;

	zsend = (char *) alloca (CPROTOCOLS + 2);
	zset = zsend;
	*zset++ = 'P';

	/* If the system did not specify a list of protocols, we want
	   only protocols that match the known reliability of the
	   dialer and the port.  If we have no information, we default
	   to a reliable eight bit connection.  */

	ir = 0;
	if (qport != NULL
	    && (qport->ireliable & RELIABLE_SPECIFIED) != 0)
	  ir = qport->ireliable;
	if ((idial_reliable & RELIABLE_SPECIFIED) != 0)
	  {
	    if (ir != 0)
	      ir &= idial_reliable;
	    else
	      ir = idial_reliable;
	  }
	if (ir == 0)
	  ir = RELIABLE_RELIABLE | RELIABLE_EIGHT | RELIABLE_SPECIFIED;

	for (i = 0; i < CPROTOCOLS; i++)
	  {
	    int ipr;

	    ipr = asProtocols[i].ireliable;
	    if ((ipr & ir) != ipr)
	      continue;
	    *zset++ = asProtocols[i].bname;
	  }
	*zset = '\0';
      }

    if (! fsend_uucp_cmd (zsend))
      return FALSE;
    
    /* The master will now send back the selected protocol.  */
    zstr = zget_uucp_cmd (TRUE);
    if (zstr == NULL)
      return FALSE;

    if (zstr[0] != 'U' || zstr[2] != '\0')
      {
	ulog (LOG_ERROR, "Bad protocol response string");
	return FALSE;
      }

    if (zstr[1] == 'N')
      {
	ulog (LOG_ERROR, "No supported protocol");
	return FALSE;
      }

    for (i = 0; i < CPROTOCOLS; i++)
      if (asProtocols[i].bname == zstr[1])
	break;

    if (i >= CPROTOCOLS)
      {
	ulog (LOG_ERROR, "No supported protocol");
	return FALSE;
      }

    qProto = &asProtocols[i];
  }

  /* Run the chat script for when a call is received.  */

  if (! fchat (&qsys->scalled_chat, qsys, (const struct sdialer *) NULL,
	       (const char *) NULL, FALSE, zport, iport_baud ()))
    return FALSE;

  /* Run any protocol parameter commands.  There should be a way to
     read the dialer information if there is any to permit modem
     specific protocol parameters, but for now there isn't.  */
  
  if (qProto->qcmds != NULL)
    {
      if (qsys->cproto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     qsys->cproto_params, qsys->qproto_params);
      if (qport != NULL
	  && qport->cproto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     qport->cproto_params, qport->qproto_params);
      if (cdial_proto_params != 0)
	uapply_proto_params (qProto->bname, qProto->qcmds,
			     cdial_proto_params, qdial_proto_params);
    }

  /* Turn on the selected protocol.  */

  if (! (*qProto->pfstart)(FALSE))
    return FALSE;

  ulog (LOG_NORMAL, "Handshake successful");

  {
    boolean fret;

    fret = fuucp (FALSE, qsys, bgrade, fnew);
    ulog_user ((const char *) NULL);
    usysdep_get_work_free (qsys);

    /* If we bombed out due to an error, shut down the protocol.  */
    if (! fret)
      {
	(void) (*qProto->pfshutdown) ();
	ustats_failed ();
      }

    /* Hangup.  As the answerer, we send seven O's and expect to see
       six.  */
    if (fsend_uucp_cmd ("OOOOOOO")
	&& fsend_uucp_cmd ("OOOOOOO"))
      {
	if (fret)
	  {
	    zstr = zget_uucp_cmd (FALSE);
#if DEBUG > 0
	    if (zstr != NULL)
	      {
		if (strstr (zstr, "OOOOOO") == NULL)
		  ulog (LOG_DEBUG, "No hangup from remote");
	      }
#endif
	  }
      }

    ulog (LOG_NORMAL, "Call complete (%ld seconds)",
	  isysdep_time () - istart_time);
    return fret;
  }
}

/* This function runs the main UUCP protocol.  It is called when the
   two systems have succesfully connected.  It transfers files back
   and forth until neither system has any more work to do.  The
   traditional UUCP protocol has a master which sends files to the
   slave or requests files from the slave (a single file is requested
   with the R command; a wildcarded file name is requested with the X
   command).  The slave simply obeys the commands of the master.  When
   the master has done all its work, it requests a hangup.  If the
   slave has work to do it refuses the hangup and becomes the new
   master.

   This is essentially a half-duplex connection, in that files are
   only transferred in one direction at a time.  This is not
   unreasonable, since generally one site is receiving a lot of news
   from the other site, and I believe that Telebit modems are
   basically half-duplex in that it takes a comparatively long time to
   turn the line around.  However, it is possible to design a
   full-duplex protocol which would be useful in some situtations when
   using V.32 (or a network) and this function attempts to support
   this possibility.

   Traditionally the work to be done is kept in a set of files whose
   names begin with C.[system][grade][pid], where system is the remote
   system name, grade is the grade of transfer, and pid makes the file
   name unique.  Each line in these files is a command, and each line
   can be treated independently.  We let the system dependent layer
   handle all of this.  This will let us use some other scheme on
   systems in which the fourteen character filename length limit
   restricts the name of the remote system to seven characters (the
   usual restriction cited is six characters; I do not yet know where
   this comes from).

   Here are the types of commands, along with the definitions of the
   variables they use in the fuucp function.

   'S' -- Send a file from master to slave.
     zfrom -- master file name
     zto -- slave file name
     zuser -- user who requested the transfer
     zoptions -- list of options
     ztemp -- temporary file name on master (used unless option c)
     imode -- mode to give file
     znotify -- user to notify (if option n)

     The options are:
     C -- file copied to spool (use ztemp rather than zfrom)
     c -- file not copied to spool (use zfrom rather than ztemp)
     d -- create directories if necessary
     f -- do not create directories
     m -- notify originator (in zuser) when complete
     n -- notify recipient (in znotify) when complete

     I assume that the n option is implemented by the remote system.

   'R' -- Retrieve a file from slave to master.
     zfrom -- slave file name
     zto -- master file name
     zuser -- user who requested the transfer
     zoptions -- list of options

     The options are the same as in case 'S', except that option n is
     not supported.  If zto is a directory, we must create a file in
     that directory using the last component of zfrom.

   'X' -- Execute wildcard transfer from slave to master.
     zfrom -- wildcard file name
     zto -- local file (hopefully a directory)
     zuser -- user who requested the transfer
     zoptions -- list of options

     The options are presumably the same as in case 'R'.  It may be
     permissible to have no zuser or zoptions.  The zto name will have
     local! prepended to it already (where local is the local system
     name).

     This command is merely sent over to the remote system, where it
     is executed.  When the remote system becomes the master, it sends
     the files back.

   'H' -- Hangup
     This is used by the master to indicate a transfer of control.  If
     slave has nothing to do, it responds with HY and the conversation
     is finished.  Otherwise, the slave becomes the master, and
     vice-versa.  */

static boolean
fuucp (fmaster, qsys, bgrade, fnew)
     boolean fmaster;
     const struct ssysteminfo *qsys;
     int bgrade;
     boolean fnew;
{
  boolean fcaller, fmasterdone, fnowork;
  long cmax;

  fcaller = fmaster;

  fmasterdone = FALSE;
  if (! qProto->ffullduplex && ! fmaster)
    fmasterdone = TRUE;

  /* Make sure we have a spool directory for this system.  */
  if (! fsysdep_make_spool_dir (qsys))
    return FALSE;

  /* If we are not the caller, the grade will be passed in as an
     argument.  If we are the caller, we compute the grade in this
     function so that we can recompute if time has passed.  */

  if (fcaller)
    bgrade = btime_low_grade (qsys->ztime);

  if (bgrade == '\0')
    fnowork = TRUE;
  else
    {
      if (! fsysdep_get_work_init (qsys, bgrade))
	return FALSE;
      fnowork = FALSE;
    }

  while (TRUE)
    {
#if ! HAVE_ALLOCA
      /* This only works if we know that no caller of this function is
	 holding an alloca'ed pointer.  */
      uclear_alloca ();
#endif

      /* We send a command to the remote system if
	 we are the master or
	 this is full duplex protocol which is ready for a command and
	 we haven't finished executing commands.  */
      if (fmaster ||
	  (qProto->ffullduplex && ! fmasterdone))
	{
	  struct scmd s;
	  const char *zmail, *zuse;
	  boolean fspool, fnever;
	  openfile_t e = EFILECLOSED;

	  /* Get the next work line for this system.  All the arguments
	     are left pointing into a static buffer, so they must be
	     copied out before the next call.  */
	  ulog_user ((const char *) NULL);
	  if (fnowork)
	    s.bcmd = 'H';
	  else
	    {
	      s.zuser = NULL;
	      if (! fsysdep_get_work (qsys, bgrade, &s))
		return FALSE;
	      ulog_user (s.zuser);
	    }

	  switch (s.bcmd)
	    {
	    case 'S':
	      /* Send a file.  */

	      fspool = fspool_file (s.zfrom);

	      if (! fspool)
		{
		  zuse = zsysdep_real_file_name (qsys, s.zfrom,
						 (const char *) NULL);
		  if (zuse == NULL)
		    {
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "cannot form file name",
					     s.zfrom, zLocalname,
					     s.zto, qsys->zname);
		      break;
		    }

		  if (! fok_to_send (zuse, TRUE, fcaller, qsys,
				     s.zuser))
		    {
		      ulog (LOG_ERROR, "Not permitted to send %s", zuse);
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "not permitted to send",
					     s.zfrom, zLocalname,
					     s.zto, qsys->zname);
		      break;
		    }

		  /* The 'C' option means that the file has been
		     copied to the spool directory.  If it hasn't been
		     copied, we use the real mode of the file rather
		     than what was recorded in the command file.  */
		  if (strchr (s.zoptions, 'C') != NULL)
		    fspool = TRUE;
		  else
		    e = esysdep_open_send (qsys, zuse, &s.imode,
					   &s.cbytes);
		}

	      if (fspool)
		{
		  unsigned int idummy;

		  zuse = zsysdep_spool_file_name (qsys, s.ztemp);
		  if (zuse == NULL)
		    {
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "cannot form file name",
					     s.zfrom, zLocalname,
					     s.zto, qsys->zname);
		      break;
		    }
		  e = esysdep_open_send (qsys, zuse, &idummy, &s.cbytes);
		}

	      if (! ffileisopen (e))
		{
		  (void) fsysdep_did_work (s.pseq);
		  (void) fmail_transfer (FALSE, s.zuser,
					 (const char *) NULL,
					 "cannot open file",
					 s.zfrom, zLocalname,
					 s.zto, qsys->zname);
		  break;
		}

	      if (s.cbytes != -1)
		{
		  cmax = cmax_size (qsys, TRUE, fcaller, TRUE);
		  if (cmax != -1 && cmax < s.cbytes)
		    {
		      cmax = cmax_size_ever (qsys, TRUE, TRUE);
		      if (cmax == -1 || cmax >= s.cbytes)
			ulog (LOG_ERROR, "File %s is too large to send now",
			      s.zfrom);
		      else
			{
			  ulog (LOG_ERROR, "File %s is too large to send",
				s.zfrom);
			  (void) fsysdep_did_work (s.pseq);
			  (void) fmail_transfer (FALSE, s.zuser,
						 (const char *) NULL,
						 "too large to send",
						 s.zfrom, zLocalname,
						 s.zto, qsys->zname);
			}
		      (void) ffileclose (e);
		      break;
		    }
		}

	      ulog (LOG_NORMAL, "Sending %s", s.zfrom);

	      /* The send file function is responsible for notifying
		 the user upon success (if option m) or failure, and
		 for closing the file.  This allows it to not complete
		 immediately.  */
	      if (strchr (s.zoptions, 'm') == NULL)
		zmail = NULL;
	      else
		zmail = s.zuser;
				      
	      if (! fsend_file (TRUE, e, &s, zmail, qsys->zname, fnew))
		return FALSE;

	      break;

	    case 'R':
	      /* Receive a file.  */

	      if (fspool_file (s.zto))
		{
		  /* Normal users are not allowed to receive files in
		     the spool directory, and to make it particularly
		     difficult we require a special option '9'.  This
		     is used only by uux when a file must be requested
		     from one system and then sent to another.  */
		  if (strchr (s.zoptions, '9') == NULL)
		    {
		      ulog (LOG_ERROR, "Not permitted to receive %s",
			    s.zto);
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "not permitted to receive",
					     s.zfrom, qsys->zname,
					     s.zto, zLocalname);
		      break;
		    }

		  zuse = zsysdep_spool_file_name (qsys, s.zto);
		  if (zuse == NULL)
		    {
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "cannot form file name",
					     s.zfrom, qsys->zname,
					     s.zto, zLocalname);
		      break;
		    }
		}
	      else
		{
		  zuse = zsysdep_real_file_name (qsys, s.zto, s.zfrom);
		  if (zuse == NULL)
		    {
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "cannot form file name",
					     s.zfrom, qsys->zname,
					     s.zto, zLocalname);
		      break;
		    }

		  /* Check permissions.  */
		  if (! fok_to_receive (zuse, TRUE, fcaller, qsys, s.zuser))
		    {
		      ulog (LOG_ERROR, "Not permitted to receive %s",
			    s.zto);
		      (void) fsysdep_did_work (s.pseq);
		      (void) fmail_transfer (FALSE, s.zuser,
					     (const char *) NULL,
					     "not permitted to receive",
					     s.zfrom, qsys->zname,
					     s.zto, zLocalname);
		      break;
		    }

		  /* The 'f' option means that directories should not
		     be created if they do not already exist.  */
		  if (strchr (s.zoptions, 'f') != NULL)
		    {
		      if (! fsysdep_make_dirs (zuse, TRUE))
			{
			  (void) fsysdep_did_work (s.pseq);
			  (void) fmail_transfer (FALSE, s.zuser,
						 (const char *) NULL,
						 "cannot create directories",
						 s.zfrom, qsys->zname,
						 s.zto, zLocalname);
			  break;
			}
		    }
		}

	      e = esysdep_open_receive (qsys, zuse, &s.ztemp, &s.cbytes);
	      if (! ffileisopen (e))
		{
		  (void) fsysdep_did_work (s.pseq);
		  (void) fmail_transfer (FALSE, s.zuser,
					 (const char *) NULL,
					 "cannot open file",
					 s.zfrom, qsys->zname,
					 s.zto, zLocalname);
		  break;
		}

	      /* Here s.cbytes represents the amount of free space we
		 have.  We want to adjust it by the amount of free
		 space permitted for this system.  If there is a
		 maximum transfer size, we may want to use that as an
		 amount of free space.  */
	      if (s.cbytes != -1)
		{
		  s.cbytes -= qsys->cfree_space;
		  if (s.cbytes < 0)
		    s.cbytes = 0;
		}

	      cmax = cmax_size (qsys, TRUE, fcaller, FALSE);
	      if (cmax != -1
		  && (s.cbytes == -1 || cmax < s.cbytes))
		s.cbytes = cmax;

	      ulog (LOG_NORMAL, "Receiving %s", zuse);
	      s.zto = zuse;

	      /* As with the send routine, this function is
		 responsible for mailing a message to the user on
		 failure or on success if the m option is set, and is
		 also responsible for closing the file.  */
	      if (strchr (s.zoptions, 'm') == NULL)
		zmail = NULL;
	      else
		zmail = s.zuser;

	      /* The imode argument (passed as 0666) will be corrected
		 with information from the remote system.  */
	      s.imode = 0666;

	      if (! freceive_file (TRUE, e, &s, zmail, qsys->zname, fnew))
		return FALSE;

	      break;

	    case 'X':
	      /* Request a file copy.  This is used to request a file
		 to be sent to another machine, as well as to get a
		 wildcarded filespec.  */
	      ulog (LOG_NORMAL, "Requesting work: %s to %s", s.zfrom,
		    s.zto);

	      if (! fxcmd (&s, &fnever))
		return FALSE;

	      (void) fsysdep_did_work (s.pseq);
	      if (fnever)
		(void) fmail_transfer (FALSE, s.zuser,
				       (const char *) NULL,
				       "wildcard request denied",
				       s.zfrom, qsys->zname,
				       s.zto, zLocalname);
	      break;

	    case 'H':
	      /* There is nothing left to do; hang up.  If we are not the
		 master, take no action (this allows for two-way
		 protocols.  */

	      fmasterdone = TRUE;
	      if (fmaster)
		{
		  if (! fhangup_request ())
		    {
		      ulog (LOG_ERROR, "Hangup failed");
		      return FALSE;
		    }

		  fmaster = FALSE;
		}
	      break;

	    default:
	      ulog (LOG_ERROR, "Unknown command '%c'", s.bcmd);
	      break;
	    }
	}

      /* We look for a command from the other system if we are the
	 slave or this is a full-duplex protocol and the slave still
	 has work to do.  */
      if (! fmaster || qProto->ffullduplex)
	{
	  struct scmd s;
	  const char *zuse, *zmail;
	  openfile_t e;
	  char bhave_grade;
	  long cbytes;

	  /* We are the slave.  Get the next command from the other
	     system.  */
	  ulog_user ((const char *) NULL);
	  if (! fgetcmd (fmaster, &s))
	    return FALSE;

	  if (s.bcmd != 'H' && s.bcmd != 'Y')
	    ulog_user (s.zuser);

	  switch (s.bcmd)
	    {
	    case 'S':
	      /* The master wants to send a file to us.  */

	      if (fspool_file (s.zto))
		{
		  zuse = zsysdep_spool_file_name (qsys, s.zto);
		  if (zuse == NULL)
		    {
		      if (! ftransfer_fail ('S', FAILURE_PERM))
			return FALSE;
		      break;
		    }
		}
	      else
		{
		  zuse = zsysdep_real_file_name (qsys, s.zto, s.zfrom);
		  if (zuse == NULL)
		    {
		      if (! ftransfer_fail ('S', FAILURE_PERM))
			return FALSE;
		      break;
		    }

		  /* Check permissions.  */
		  if (! fok_to_receive (zuse, FALSE, fcaller, qsys,
					s.zuser))
		    {
		      ulog (LOG_ERROR, "Not permitted to receive %s", zuse);
		      if (! ftransfer_fail ('S', FAILURE_PERM))
			return FALSE;
		      break;
		    }

		  if (strchr (s.zoptions, 'f') != NULL)
		    {
		      if (! fsysdep_make_dirs (zuse, TRUE))
			{
			  if (! ftransfer_fail ('S', FAILURE_PERM))
			    return FALSE;
			  break;
			}
		    }
		}

	      e = esysdep_open_receive (qsys, zuse, &s.ztemp, &cbytes);
	      if (! ffileisopen (e))
		{
		  if (! ftransfer_fail ('S', FAILURE_OPEN))
		    return FALSE;
		  break;
		}

	      /* Adjust the number of bytes we are prepared to receive
		 according to the amount of free space we are supposed
		 to leave available and the maximum file size we are
		 permitted to transfer.  */
	      if (cbytes != -1)
		{
		  cbytes -= qsys->cfree_space;
		  if (cbytes < 0)
		    cbytes = 0;
		}
	      
	      cmax = cmax_size (qsys, FALSE, fcaller, FALSE);
	      if (cmax != -1
		  && (cbytes == -1 || cmax < cbytes))
		cbytes = cmax;

	      /* If the number of bytes we are prepared to receive
		 is less than the file size, we must fail.  */

	      if (s.cbytes != -1
		  && cbytes != -1
		  && cbytes < s.cbytes)
		{
		  ulog (LOG_ERROR, "%s is too big to receive", zuse);
		  (void) ffileclose (e);
		  if (! ftransfer_fail ('S', FAILURE_SIZE))
		    return FALSE;
		  break;
		}

	      ulog (LOG_NORMAL, "Receiving %s", zuse);
	      s.zto = zuse;

	      if (strchr (s.zoptions, 'n') == NULL)
		zmail = NULL;
	      else
		zmail = s.znotify;
	      s.pseq = NULL;
	      if (! freceive_file (FALSE, e, &s, zmail, qsys->zname, fnew))
		return FALSE;

	      break;

	    case 'R':
	      /* The master wants to get a file from us.  */

	      if (fspool_file (s.zfrom))
		{
		  ulog (LOG_ERROR, "No permission to send %s", s.zfrom);
		  if (! ftransfer_fail ('R', FAILURE_PERM))
		    return FALSE;
		  break;
		}

	      zuse = zsysdep_real_file_name (qsys, s.zfrom,
					     (const char *) NULL);
	      if (zuse == NULL)
		{
		  if (! ftransfer_fail ('R', FAILURE_PERM))
		    return FALSE;
		  break;
		}

	      if (! fok_to_send (zuse, FALSE, fcaller, qsys, s.zuser))
		{
		  ulog (LOG_ERROR, "No permission to send %s", zuse);
		  if (! ftransfer_fail ('R', FAILURE_PERM))
		    return FALSE;
		  break;
		}

	      e = esysdep_open_send (qsys, zuse, &s.imode, &cbytes);
	      if (! ffileisopen (e))
		{
		  if (! ftransfer_fail ('R', FAILURE_OPEN))
		    return FALSE;
		  break;
		}

	      /* Get the maximum size we are prepared to send.  */
	      cmax = cmax_size (qsys, FALSE, fcaller, TRUE);

	      /* If the file is larger than the amount of space
		 the other side reported, we can't send it.  */
	      if (cbytes != -1
		  && ((s.cbytes != -1 && s.cbytes < cbytes)
		      || (cmax != -1 && cmax < cbytes)))
		{
		  ulog (LOG_ERROR, "%s is too large to send", zuse);
		  if (! ftransfer_fail ('R', FAILURE_SIZE))
		    return FALSE;
		  (void) ffileclose (e);
		  break;
		}

	      ulog (LOG_NORMAL, "Sending %s", zuse);

	      /* Pass in the real size of the file.  */
	      s.cbytes = cbytes;

	      if (! fsend_file (FALSE, e, &s, (const char *) NULL,
				qsys->zname, fnew))
		return FALSE;

	      break;

	    case 'X':
	      /* This is an execution request.  We are being asked to
		 send one or more files to a destination on either the
		 local or a remote system.  We do this by spooling up
		 commands for the destination system.  */
	      ulog (LOG_NORMAL, "Work requested: %s to %s", s.zfrom,
		    s.zto);

	      if (fdo_xcmd (qsys, &s))
		{
		  if (! fxcmd_confirm ())
		    return FALSE;
		}
	      else
		{
		  if (! ftransfer_fail ('X', FAILURE_PERM))
		    return FALSE;
		}

	      break;

	    case 'H':
	      /* The master wants to hang up.  If we have something to
		 do, become the master.  Otherwise, agree to hang up.
		 We recheck the grades allowed at this time, since a
		 lot of time may have passed.  */
	      if (fcaller)
		bgrade = btime_low_grade (qsys->ztime);
	      if (bgrade != '\0'
		  && fsysdep_has_work (qsys, &bhave_grade)
		  && igradecmp (bgrade, bhave_grade) >= 0)
		{
		  if (fmasterdone)
		    {
		      if (! fsysdep_get_work_init (qsys, bgrade))
			return FALSE;
		      fnowork = FALSE;
		    }

		  fmasterdone = FALSE;
		  
		  if (! fhangup_reply (FALSE))
		      return FALSE;
		  fmaster = TRUE;
		}
	      else
		{
		  /* The hangup_reply function will shut down the
		     protocol.  */
		  return fhangup_reply (TRUE);
		}
	      break;

	    case 'Y':
	      /* This is returned when a hangup has been confirmed and
		 the protocol has been shut down.  */
	      return TRUE;

	    default:
	      ulog (LOG_ERROR, "Unknown command %c", s.bcmd);
	      break;
	    }
	}
    }
}

/* Do an 'X' request for another system.  The other system has
   basically requested us to execute a uucp command for them.  */

static boolean
fdo_xcmd (qsys, q)
     const struct ssysteminfo *qsys;
     const struct scmd *q;
{
  const char *zexclam;
  const char *zdestfile;
  char *zcopy;
  struct ssysteminfo sdestsys;
  const struct ssysteminfo *qdestsys;
  char *zuser = NULL;
  char aboptions[5];
  char *zoptions = NULL;
  boolean fmkdirs;
  const char *zfile;

  zexclam = strchr (q->zto, '!');
  if (zexclam == NULL
      || zexclam == q->zto
      || strncmp (zLocalname, q->zto, zexclam - q->zto) == 0)
    {
      /* The files are supposed to be copied to the
	 local system.  */
      qdestsys = NULL;
      if (zexclam == NULL)
	zdestfile = q->zto;
      else
	zdestfile = zexclam + 1;
    }
  else
    {
      int clen;

      clen = zexclam - q->zto;
      zcopy = (char *) alloca (clen + 1);
      strncpy (zcopy, q->zto, clen);
      zcopy[clen] = '\0';

      if (! fread_system_info (zcopy, &sdestsys))
	{
	  if (! fUnknown_ok)
	    {
	      ulog (LOG_ERROR, "Destination system %s unknown",
		    zcopy);
	      return FALSE;
	    }
	  sdestsys = sUnknown;
	  sdestsys.zname = zcopy;
	}
      qdestsys = &sdestsys;
      zdestfile = zexclam + 1;
    }

  if (qdestsys != NULL)
    {
      zuser = (char *) alloca (strlen (qdestsys->zname)
			       + strlen (q->zuser) + sizeof "!");
      sprintf (zuser, "%s!%s", qdestsys->zname,
	       q->zuser);
      zoptions = aboptions;
      *zoptions++ = 'C';
      if (strchr (q->zoptions, 'd') != NULL)
	*zoptions++ = 'd';
      if (strchr (q->zoptions, 'm') != NULL)
	*zoptions++ = 'm';
      *zoptions = '\0';
      fmkdirs = TRUE;
    }
  else
    fmkdirs = strchr (q->zoptions, 'f') != NULL;

  /* Now we have to process each source file.  The
     source specification may or may use wildcards.  */
  if (! fsysdep_wildcard_start (qsys, q->zfrom))
    return FALSE;

  while ((zfile = zsysdep_wildcard (qsys, q->zfrom)) != NULL)
    {
      const char *zto;
      char abtname[CFILE_NAME_LEN];

      zcopy = (char *) alloca (strlen (zfile) + 1);
      strcpy (zcopy, zfile);
      zfile = zcopy;

      /* Make sure the remote system is permitted to read the
	 specified file.  */
      if (! fin_directory_list (qsys, zfile, qsys->zremote_send))
	{
	  ulog (LOG_ERROR, "Not permitted to send %s", zfile);
	  (void) fsysdep_wildcard_end ();
	  return FALSE;
	}

      if (qdestsys != NULL)
	{
	  /* We really should get the original grade here.  */
	  zto = zsysdep_data_file_name (qdestsys, BDEFAULT_UUCP_GRADE,
					abtname, (char *) NULL,
					(char *) NULL);
	}
      else
	{
	  zto = zsysdep_real_file_name (qsys, zexclam + 1, zfile);
	  if (zto == NULL)
	    {
	      (void) fsysdep_wildcard_end ();
	      return FALSE;
	    }
	  /* We only accept a local destination if the remote system
	     has the right to create files there.  */
	  if (! fin_directory_list (qsys, zto, qsys->zremote_receive))
	    {
	      ulog (LOG_ERROR, "Not permitted to receive %s", zto);
	      (void) fsysdep_wildcard_end ();
	      return FALSE;
	    }
	}

      /* Copy the file either to the final destination or to the
	 spool directory.  */
      if (! fcopy_file (zfile, zto, qdestsys == NULL, fmkdirs))
	{
	  (void) fsysdep_wildcard_end ();
	  return FALSE;
	}

      /* If there is a destination system, queue it up.  */
      if (qdestsys != NULL)
	{
	  struct scmd ssend;

	  ssend.bcmd = 'S';
	  ssend.pseq = NULL;
	  ssend.zfrom = zfile;
	  ssend.zto = zdestfile;
	  ssend.zuser = zuser;
	  ssend.zoptions = aboptions;
	  ssend.ztemp = abtname;
	  ssend.imode = isysdep_file_mode (zfile);
	  if (ssend.imode == 0)
	    {
	      (void) fsysdep_wildcard_end ();
	      return FALSE;
	    }
	  ssend.znotify = "";
	  ssend.cbytes = -1;

	  if (! fsysdep_spool_commands (qdestsys, BDEFAULT_UUCP_GRADE,
					1, &ssend))
	    {
	      (void) fsysdep_wildcard_end ();
	      return FALSE;
	    }
	}
    }

  if (! fsysdep_wildcard_end ())
    return FALSE;

  return TRUE;
}

/* See whether it's OK to send a file to another system, according to
   the permissions recorded for that system.

   zfile -- file to send
   flocal -- TRUE if the send was requested locally
   fcaller -- TRUE if the local system called the other system
   qsys -- remote system information
   zuser -- user who requested the action (not currently used)  */

/*ARGSUSED*/
static boolean
fok_to_send (zfile, flocal, fcaller, qsys, zuser)
     const char *zfile;
     boolean flocal;
     boolean fcaller;
     const struct ssysteminfo *qsys;
     const char *zuser;
{
  const char *z;

  if (! frequest_ok (flocal, fcaller, qsys, zuser))
    return FALSE;

  if (flocal)
    z = qsys->zlocal_send;
  else
    z = qsys->zremote_send;

  return fin_directory_list (qsys, zfile, z);
}

/* See whether it's OK to receive a file from another system.  */

/*ARGSUSED*/
static boolean
fok_to_receive (zto, flocal, fcaller, qsys, zuser)
     const char *zto;
     boolean flocal;
     boolean fcaller;
     const struct ssysteminfo *qsys;
     const char *zuser;
{
  const char *z;

  if (! frequest_ok (flocal, fcaller, qsys, zuser))
    return FALSE;

  if (flocal)
    z = qsys->zlocal_receive;
  else
    z = qsys->zremote_receive;

  return fin_directory_list (qsys, zto, z);
}

/* See whether a request is OK.  This depends on which system placed
   the call and which system made the request.  */

/*ARGSUSED*/
static boolean
frequest_ok (flocal, fcaller, qsys, zuser)
     boolean flocal;
     boolean fcaller;
     const struct ssysteminfo *qsys;
     const char *zuser;
{
  if (flocal)
    {
      if (fcaller)
	return qsys->fcall_transfer;
      else
	return qsys->fcalled_transfer;
    }
  else
    {
      if (fcaller)
	return qsys->fcall_request;
      else
	return qsys->fcalled_request;
    }
}

/* Get the maximum size we are permitted to transfer now.  */

/*ARGSUSED*/
static long
cmax_size (qsys, flocal, fcaller, fsend)
     const struct ssysteminfo *qsys;
     boolean flocal;
     boolean fcaller;
     boolean fsend;
{
  const char *z;
  char *zcopy;
  char *znext;
  long cret;

  if (flocal)
    {
      if (fcaller)
	z = qsys->zcall_local_size;
      else
	z = qsys->zcalled_local_size;
    }
  else
    {
      if (fcaller)
	z = qsys->zcall_remote_size;
      else
	z = qsys->zcalled_remote_size;
    }

  if (z == NULL)
    return (long) -1;

  zcopy = (char *) alloca (strlen (z) + 1);
  strcpy (zcopy, z);

  znext = strtok (zcopy, " ");
  cret = 0;

  while (znext != NULL)
    {
      char *ztime;

      ztime = strtok ((char *) NULL, " ");
#if DEBUG > 0
      if (ztime == NULL)
	ulog (LOG_FATAL, "cmax_size: Can't happen");
#endif
      if (ftime_now (ztime))
	{
	  long c;

	  c = strtol (znext, (char **) NULL, 10);
	  if (c > cret)
	    cret = c;
	}
      znext = strtok ((char *) NULL, " ");
    }

  return cret;
}
	  
/* Get the maximum size we are ever permitted to transfer.  */

/*ARGSUSED*/
static long
cmax_size_ever (qsys, flocal, fsend)
     const struct ssysteminfo *qsys;
     boolean flocal;
     boolean fsend;
{
  const char *z1, *z2;
  long cret;
  long c;

  if (flocal)
    {
      z1 = qsys->zcall_local_size;
      z2 = qsys->zcalled_local_size;
    }
  else
    {
      z1 = qsys->zcall_remote_size;
      z2 = qsys->zcalled_remote_size;
    }

  cret = (long) -1;

  if (z1 != NULL)
    {
      c = cmax_size_string (z1);
      if (c > cret)
	cret = c;
    }

  if (z2 != NULL)
    {
      c = cmax_size_string (z2);
      if (c > cret)
	cret = c;
    }

  return cret;
}

/* Get the maximum size which can be found in a time size string.  */

static long
cmax_size_string (z)
     const char *z;
{
  char *zcopy;
  char *znext;
  long cret;

  zcopy = (char *) alloca (strlen (z) + 1);
  strcpy (zcopy, z);

  znext = strtok (zcopy, " ");
  cret = 0;

  while (znext != NULL)
    {
      char *ztime;
      long c;

      ztime = strtok ((char *) NULL, " ");
#if DEBUG > 0
      if (ztime == NULL)
	ulog (LOG_FATAL, "cmax_size_string: Can't happen");
#endif
      /* We probably should check whether the time string can
	 ever occur.  */

      c = strtol (znext, (char **) NULL, 10);
      if (c > cret)
	cret = c;

      znext = strtok ((char *) NULL, " ");
    }

  return cret;
}

/* Send a string to the other system beginning with a DLE
   character and terminated with a null byte.  This is only
   used when no protocol is in force.  */

static boolean
fsend_uucp_cmd (z)
     const char *z;
{
  char *zalc;
  int cwrite;

  cwrite = strlen (z) + 2;

  zalc = (char *) alloca (cwrite);
  sprintf (zalc, "\020%s", z);

  return fport_write (zalc, cwrite);
}

/* Get a UUCP command beginning with a DLE character and ending with a
   null byte.  This is only used when no protocol is in force.  This
   implementation has the potential of being seriously slow.  It also
   doesn't have any real error recovery.  The freport argument is
   passed as TRUE if we should report a timeout error; we don't want
   to report one if we're closing down the connection anyhow.  */

#define CTIMEOUT (120)
#define CINCREMENT (10)

static const char *
zget_uucp_cmd (freport)
     boolean freport;
{
  static char *zalc;
  static int calc;
  int cgot;
  long iendtime;

  iendtime = isysdep_time () + CTIMEOUT;

  cgot = -1;
  while (TRUE)
    {
      int b;
      
      b = breceive_char ((int) (iendtime - isysdep_time ()), freport);
      /* Now b == -1 on timeout, -2 on error.  */
      if (b < 0)
	{
	  if (b == -1 && freport)
	    ulog (LOG_ERROR, "Timeout");
	  return NULL;
	}

      if (cgot < 0)
	{
	  if (b != '\020')
	    continue;
	  cgot = 0;
	  continue;
	}

      /* If we see another DLE, something has gone wrong; continue
	 as though this were the first one we saw.  */
      if (b == '\020')
	{
	  cgot = 0;
	  continue;
	}

      if (cgot >= calc)
	{
	  calc += CINCREMENT;
	  zalc = (char *) xrealloc ((pointer) zalc, calc);
	}

      zalc[cgot] = b;
      ++cgot;

      if (b == '\0')
	return zalc;
    }
}

/* Read a sequence of characters up to a newline or carriage return, and
   return the line without the line terminating character.  */

static const char *
zget_typed_line ()
{
  static char *zalc;
  static int calc;
  int cgot;

  cgot = 0;
  while (TRUE)
    {
      int b;
      
      b = breceive_char (CTIMEOUT, FALSE);
      /* Now b == -1 on timeout, -2 on error.  */
      if (b == -2)
	return NULL;
      if (b == -1)
	continue;

      if (cgot >= calc)
	{
	  calc += CINCREMENT;
	  zalc = (char *) xrealloc ((pointer) zalc, calc);
	}

      if (b == '\r' || b == '\n')
	b = '\0';

      zalc[cgot] = b;
      ++cgot;

      if (b == '\0')
	return zalc;
    }
}
