/* cu.c
   Call up a remote system.

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
   c/o AIRS, P.O. Box 520, Waltham, MA 02254.

   $Log$
   */

#include "uucp.h"

#if USE_RCS_ID
char cu_rcsid[] = "$Id$";
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "cu.h"
#include "sysdep.h"
#include "port.h"
#include "prot.h"
#include "system.h"
#include "getopt.h"

/* Here are the user settable variables.  The user is permitted to
   change these while running the program, using ~s.  */

/* The escape character used to introduce a special command.  The
   escape character is the first character of this string.  */
const char *zCuvar_escape = "~";

/* Whether to delay for a second before printing the host name after
   seeing an escape character.  */
boolean fCuvar_delay = TRUE;

/* The input characters which finish a line.  The escape character is
   only recognized following one of these characters.  The default is
   carriage return, ^U, ^C, ^O, ^D, ^S, ^Q, ^R, which I got from the
   Ultrix /etc/remote file.  */
const char *zCuvar_eol = "\r\003\017\004\023\021\022";

/* Whether to transfer binary data (nonprintable characters other than
   newline and tab) when sending a file.  If this is FALSE, then
   newline is changed to carriage return.  */
boolean fCuvar_binary = FALSE;

/* A prefix string to use before sending a binary character from a
   file; this is only used if fCuvar_binary is TRUE.  The default is
   ^Z. */
const char *zCuvar_binary_prefix = "\026";

/* Whether to check for echoes of characters sent when sending a file.
   This is ignored if fCuvar_binary is TRUE.  */
boolean fCuvar_echocheck = TRUE;

/* A character to look for after each newline is sent when sending a
   file.  The character is the first character in this string, except
   that a '\0' means that no echo check is done.  */
const char *zCuvar_echonl = "";

/* The timeout to use when looking for an character.  */
int cCuvar_timeout = 30;

/* The character to use to kill a line if an echo check fails.  The
   first character in this string is sent.  The default is ^U.  */
const char *zCuvar_kill = "\025";

/* The number of times to try resending a line if the echo check keeps
   failing.  */
int cCuvar_resend = 10;

/* The string to send at the end of a file sent with ~>.  The default
   is ^D.  */
const char *zCuvar_eofwrite = "\004";

/* The string to look for to finish a file received with ~<.  For tip
   this is a collection of single characters, but I don't want to do
   that because it means that there are characters which cannot be
   received.  The default is a guess at a typical shell prompt.  */
const char *zCuvar_eofread = "$";

/* Whether to provide verbose information when sending or receiving a
   file.  */
boolean fCuvar_verbose = FALSE;

/* The table used to give a value to a variable, and to print all the
   variable values.  */

static const struct scmdtab asCuvars[] =
{
  { "escape", CMDTABTYPE_STRING, (pointer) &zCuvar_escape, NULL },
  { "delay", CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_delay, NULL },
  { "eol", CMDTABTYPE_STRING, (pointer) &zCuvar_eol, NULL },
  { "binary", CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_binary, NULL },
  { "binary-prefix", CMDTABTYPE_STRING, (pointer) &zCuvar_binary_prefix,
      NULL },
  { "echocheck", CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_echocheck, NULL },
  { "echonl", CMDTABTYPE_STRING, (pointer) &zCuvar_echonl, NULL },
  { "timeout", CMDTABTYPE_INT, (pointer) &cCuvar_timeout, NULL },
  { "kill", CMDTABTYPE_STRING, (pointer) &zCuvar_kill, NULL },
  { "resend", CMDTABTYPE_INT, (pointer) &cCuvar_resend, NULL },
  { "eofwrite", CMDTABTYPE_STRING, (pointer) &zCuvar_eofwrite, NULL },
  { "eofread", CMDTABTYPE_STRING, (pointer) &zCuvar_eofread, NULL },
  { "verbose", CMDTABTYPE_BOOLEAN, (pointer) &fCuvar_verbose, NULL },
  { NULL, 0, NULL, NULL}
};

/* The program name.  */
char abProgram[] = "cu";

/* Local variables.  */

/* The string we print when the user is once again connected to the
   port after transferring a file or taking some other action.  */
static const char abCuconnected[] = "[connected]";

/* Hold line option so that fcuport_lock can examine it.  */
static const char *zCuline;

/* Whether we need to restore the terminal.  */
static boolean fCurestore_terminal;

/* Whether we are doing local echoing.  */
static boolean fCulocalecho;

/* Whether we need to call fsysdep_cu_finish.  */
static boolean fCustarted;

/* Local functions.  */

static void ucuusage P((void));
static void ucuabort P((void));
static void uculog_start P((void));
static void uculog_end P((void));
static boolean fcuport_lock P((struct sport *qport, boolean fin));
static boolean fcudo_cmd P((int bcmd));
static boolean fcuset_var P((char *zline));
static void uculist_vars P((void));
static boolean fcudo_subcmd P((char *zline));
static boolean fcusend_buf P((const char *zbuf, int cbuf));

#define ucuputs(zline) \
       do { if (! fsysdep_terminal_puts (zline)) ucuabort (); } while (0)

/* Long getopt options.  */

static const struct option asLongopts[] = { { NULL, 0, NULL, 0 } };

const struct option *_getopt_long_options = asLongopts;

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  /* -c: phone number.  */
  const char *zphone = NULL;
  /* -e: even parity.  */
  boolean feven = FALSE;
  /* -l: line.  */
  const char *zline = NULL;
  /* -n: prompt for phone number.  */
  boolean fprompt = FALSE;
  /* -o: odd parity.  */
  boolean fodd = FALSE;
  /* -p: port name.  */
  const char *zport = NULL;
  /* -s: speed.  */
  long ibaud = 0L;
  /* -t: map cr to crlf.  */
  boolean fmapcr = FALSE;
  /* -z: system.  */
  const char *zsystem = NULL;
  /* -I: configuration file name.  */
  const char *zconfig = NULL;
  int i;
  struct ssysteminfo ssys;
  const struct ssysteminfo *qsys = NULL;
  struct sport sportinfo;
  struct sport *qport = NULL;
  long ihighbaud;
  int cdummy;
  struct sproto_param *qdummy;
  int idummy;
  char bcmd;

  /* We want to accept -# as a speed.  It's easiest to look through
     the arguments, replace -# with -s#, and let getopt handle it.  */
  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-'
	  && isdigit (BUCHAR (argv[i][1])))
	{
	  char *z;

	  z = (char *) alloca (strlen (argv[i]) + 2);
	  z[0] = '-';
	  z[1] = 's';
	  strcpy (z + 2, argv[i] + 1);
 	  argv[i] = z;
	}
    }

  while ((iopt = getopt (argc, argv, "a:c:dehnI:l:op:s:tx:z:")) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  /* Phone number.  */
	  zphone = optarg;
	  break;

	case 'd':
	  /* Set debugging level to maximum.  */
#if DEBUG > 1
	  iDebug = DEBUG_MAX;
#endif
	  break;

	case 'e':
	  /* Even parity.  */
	  feven = TRUE;
	  break;

	case 'h':
	  /* Local echo.  */
	  fCulocalecho = TRUE;
	  break;

	case 'n':
	  /* Prompt for phone number.  */
	  fprompt = TRUE;
	  break;

	case 'l':
	  /* Line name.  */
	  zline = optarg;
	  break;

	case 'o':
	  /* Odd parity.  */
	  fodd = TRUE;
	  break;

	case 'p':
	case 'a':
	  /* Port name (-a is for compatibility).  */
	  zport = optarg;
	  break;

	case 's':
	  /* Speed.  */
	  ibaud = atol (optarg);
	  break;

	case 't':
	  /* Map cr to crlf.  */
	  fmapcr = TRUE;
	  break;

	case 'z':
	  /* System name.  */
	  zsystem = optarg;
	  break;

	case 'I':
	  /* Configuration file name.  */
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
	  ucuusage ();
	  break;
	}
    }

  /* There can be one more argument, which is either a system name, a
     phone number, or "dir".  We decide which it is based on the first
     character.  To call a UUCP system whose name begins with a digit,
     or one which is named "dir", you must use -z.  */

  if (optind != argc)
    {
      if (optind != argc - 1
	  || zsystem != NULL
	  || zphone != NULL)
	ucuusage ();
      if (strcmp (argv[optind], "dir") != 0)
	{
	  if (isdigit (BUCHAR (argv[optind][0])))
	    zphone = argv[optind];
	  else
	    zsystem = argv[optind];
	}
    }

  /* If the user doesn't give a system, port, line or speed, then
     there's no basis on which to select a port.  */
  if (zsystem == NULL
      && zport == NULL
      && zline == NULL
      && ibaud == 0L)
    ucuusage ();

  if (fprompt != NULL)
    {
      printf ("Phone number: ");
      (void) fflush (stdout);
      zphone = zfgets (stdin, FALSE);
      if (zphone == NULL || *zphone == '\0')
	{
	  fprintf (stderr, "%s: No phone number entered\n", abProgram);
	  exit (EXIT_FAILURE);
	}
    }

  uread_config (zconfig);

  usysdep_initialize (INIT_NOCHDIR);

  ulog_fatal_fn (ucuabort);
  pfLstart = uculog_start;
  pfLend = uculog_end;

#ifdef SIGINT
  usysdep_signal (SIGINT);
#endif
#ifdef SIGHUP
  usysdep_signal (SIGHUP);
#endif
#ifdef SIGQUIT
  usysdep_signal (SIGQUIT);
#endif
#ifdef SIGTERM
  usysdep_signal (SIGTERM);
#endif
#ifdef SIGPIPE
  usysdep_signal (SIGPIPE);
#endif

  if (zsystem != NULL)
    {
      if (! fread_system_info (zsystem, &ssys))
	ulog (LOG_FATAL, "%s: Unknown system", zsystem);
      qsys = &ssys;
    }

  /* The ffind_port function only takes a name (zport) and a speed
     (ibaud) as arguments.  To select based on the line name, we pass
     a static function down as the locking routine which makes the
     check.  If we can't find any defined port, and the user specified
     a line name but did not specify a port name or a system or a
     phone number, then we fake a direct port with that line name (we
     don't fake a port if a system or phone number were given because
     if we fake a port we have no way to place a call; perhaps we
     should automatically look up a particular dialer).  This permits
     users to say cu -lttyd0 without having to put ttyd0 in the ports
     file, provided they have read and write access to the port.  */

  if (zport != NULL || zline != NULL || ibaud != 0L)
    {
      boolean fnoline;

      zCuline = zline;
      fnoline = (zline == NULL
		 || zport != NULL
		 || zphone != NULL
		 || qsys != NULL);
      if (! ffind_port (zport, ibaud, 0L, &sportinfo,
			fcuport_lock, fnoline))
	{
	  if (fnoline)
	    ucuabort ();

	  sportinfo.zname = zline;
	  sportinfo.ttype = PORTTYPE_DIRECT;
	  sportinfo.zprotocols = NULL;
	  sportinfo.cproto_params = 0;
	  sportinfo.qproto_params = NULL;
	  sportinfo.ireliable = 0;
	  sportinfo.zlockname = NULL;
	  sportinfo.u.sdirect.zdevice = zline;
	  sportinfo.u.sdirect.ibaud = ibaud;
#ifdef SYSDEP_DIRECT_INIT
	  SYSDEP_DIRECT_INIT (&sportinfo.u.sdirect.s);
#endif

	  if (! fsysdep_port_access (&sportinfo))
	    ulog (LOG_FATAL, "%s: Permission denied", zline);

	  if (! fport_lock (&sportinfo, FALSE))
	    ulog (LOG_FATAL, "%s: Port not available", zline);
	}

      qport = &sportinfo;
      ihighbaud = 0L;
    }
  else
    {
      for (; qsys != NULL; qsys = qsys->qalternate)
	{
	  if (qsys->qport != NULL)
	    {
	      if (fport_lock (qsys->qport, FALSE))
		{
		  qport = qsys->qport;
		  break;
		}
	    }
	  else
	    {
	      if (ffind_port (qsys->zport, qsys->ibaud, qsys->ihighbaud,
			      &sportinfo, fport_lock, FALSE))
		{
		  qport = &sportinfo;
		  break;
		}
	    }
	}

      if (qsys == NULL)
	ulog (LOG_FATAL, "%s: No ports available", zsystem);

      ibaud = qsys->ibaud;
      ihighbaud = qsys->ihighbaud;
    }

  /* Here we have locked a port to use.  */

  if (! fport_open (qport, ibaud, ihighbaud, FALSE))
    ucuabort ();

  if (FGOT_SIGNAL ())
    ucuabort ();

  /* Set up the port.  */
  {
    enum tparitysetting tparity;
    enum tstripsetting tstrip;

    if (fodd && feven)
      {
	tparity = PARITYSETTING_NONE;
	tstrip = STRIPSETTING_SEVENBITS;
      }
    else if (fodd)
      {
	tparity = PARITYSETTING_ODD;
	tstrip = STRIPSETTING_SEVENBITS;
      }
    else if (feven)
      {
	tparity = PARITYSETTING_EVEN;
	tstrip = STRIPSETTING_SEVENBITS;
      }
    else
      {
	tparity = PARITYSETTING_DEFAULT;
	tstrip = STRIPSETTING_DEFAULT;
      }

    if (! fport_set (tparity, tstrip, XONXOFF_ON))
      ucuabort ();
  }

  if (qsys != NULL)
    zphone = qsys->zphone;

  if (qsys != NULL || zphone != NULL)
    {
      if (! fport_dial (qsys, zphone, &cdummy, &qdummy, &idummy))
	ucuabort ();
    }
  else
    {
      /* If no system or phone number was specified, we connect
	 directly to the modem.  We only permit this if the user has
	 access to the port, since it permits various shenanigans such
	 as reprogramming the automatic callbacks.  */
      if (! fsysdep_port_access (qport))
	ulog (LOG_FATAL, "Access to port denied");
    }

  if (FGOT_SIGNAL ())
    ucuabort ();

  /* Here we have connected, and can start the main cu protocol.  The
     program spends most of its time in system dependent code, and
     only comes out when a special command is received from the
     terminal.  */

  printf ("Connected.\n");

  if (! fsysdep_terminal_raw (fCulocalecho))
    ucuabort ();

  fCurestore_terminal = TRUE;

  if (! fsysdep_cu_init ())
    ucuabort ();

  fCustarted = TRUE;

  while (fsysdep_cu (&bcmd))
    {
#if ! HAVE_ALLOCA
      (void) alloca (0);
#endif
      if (! fcudo_cmd (bcmd))
	break;
    }

  fCustarted = FALSE;
  if (! fsysdep_cu_finish ())
    ucuabort ();

  fCurestore_terminal = FALSE;
  (void) fsysdep_terminal_restore ();

  (void) fport_close (TRUE);
    
  printf ("\nDisconnected.\n");

  ulog_close ();

  usysdep_exit (TRUE);

  /* Avoid errors about not returning a value.  */
  return 0;
}

/* Print a usage message and die.  */

static void
ucuusage ()
{
  fprintf (stderr,
	   "Taylor UUCP version %s, copyright (C) 1991, 1992 Ian Lance Taylor\n",
	   abVersion);
  fprintf (stderr,
	   "Usage: cu [options] [system or phone-number]\n");
  fprintf (stderr,
	   " -a port, -p port: Use named port\n");
  fprintf (stderr,
	   " -l line: Use named device (e.g. tty0)\n");
  fprintf (stderr,
	   " -s speed, -#: Use given speed\n");
  fprintf (stderr,
	   " -c phone: Phone number to call\n");
  fprintf (stderr,
	   " -z system: System to call\n");
  fprintf (stderr,
	   " -e: Set even parity\n");
  fprintf (stderr,
	   " -o: Set odd parity\n");
  fprintf (stderr,
	   " -h: Echo locally\n");
  fprintf (stderr,
	   " -t: Map carriage return to carriage return/linefeed\n");
  fprintf (stderr,
	   " -n: Prompt for phone number\n");
  fprintf (stderr,
	   " -d: Set maximum debugging level\n");
  fprintf (stderr,
	   " -x debug: Set debugging type\n");
#if HAVE_TAYLOR_CONFIG
  fprintf (stderr,
	   " -I file: Set configuration file to use (default %s%s)\n",
	   NEWCONFIGLIB, CONFIGFILE);
#endif /* HAVE_TAYLOR_CONFIG */

  exit (EXIT_FAILURE);
}

/* This function is called when a fatal error occurs.  */

static void
ucuabort ()
{
  if (fCustarted)
    {
      fCustarted = FALSE;
      (void) fsysdep_cu_finish ();
    }

  if (fCurestore_terminal)
    {
      fCurestore_terminal = FALSE;
      (void) fsysdep_terminal_restore ();
    }

  if (qPort != NULL)
    (void) fport_close (FALSE);

  ulog_close ();

  printf ("\nDisconnected.\n");

  usysdep_exit (FALSE);
}

/* This variable is just used to communicate between uculog_start and
   uculog_end.  */
static boolean fCulog_restore;

/* This function is called by ulog before it output anything.  We use
   it to restore the terminal, if necessary.  ulog is only called for
   errors or debugging in cu, so it's not too costly to do this.  If
   we didn't do it, then at least on Unix each line would leave the
   cursor in the same column rather than wrapping back to the start,
   since CRMOD will not be on.  */

static void
uculog_start ()
{
  if (! fCurestore_terminal)
    fCulog_restore = FALSE;
  else
    {
      fCulog_restore = TRUE;
      fCurestore_terminal = FALSE;
      if (! fsysdep_terminal_restore ())
	ucuabort ();
    }
}

/* This function is called by ulog after everything is output.  It
   sets the terminal back, if necessary.  */

static void
uculog_end ()
{
  if (fCulog_restore)
    {
      if (! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
    }
}

/* Check to see if this port has the desired line, to handle the -l
   option.  If it does, or if no line was specified, call fport_lock
   to try to lock it.  */

static boolean
fcuport_lock (qport, fin)
     struct sport *qport;
     boolean fin;
{
  if (zCuline != NULL
      && ! fsysdep_port_is_line (qport, zCuline))
    return FALSE;
  return fport_lock (qport, fin);
}

/* Execute a cu escape command.  Return TRUE if the connection should
   continue, or FALSE if the connection should be terminated.  */

static boolean
fcudo_cmd (bcmd)
     int bcmd;
{
  char *zline;
  char *z;
  char abescape[5];
  char abbuf[100];

  /* Some commands take a string up to the next newline character.  */
  switch (bcmd)
    {
    default:
      zline = NULL;
      break;
    case '!':
    case '$':
    case '%':
    case '|':
    case '+':
    case '>':
    case '<':
    case 'c':
    case 'p':
    case 't':
    case 's':
      {
	const char *zsys;

	zsys = zsysdep_terminal_line ((const char *) NULL);
	if (zsys == NULL)
	  ucuabort ();
	zline = (char *) alloca (strlen (zsys) + 1);
	strcpy (zline, zsys);
	zline[strcspn (zline, "\n")] = '\0';
      }
      break;
    }

  switch (bcmd)
    {
    default:
      if (! isprint (*zCuvar_escape))
	sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
      else
	{
	  abescape[0] = *zCuvar_escape;
	  abescape[1] = '\0';
	}
      sprintf (abbuf, "[Unrecognized.  Use %s%s to send %s]",
	       abescape, abescape, abescape);
      ucuputs (abbuf);
      return TRUE;

    case '.':
      /* Hangup.  */
      return FALSE;

    case '!':
    case '$':
    case '|':
    case '+':
      /* Shell out.  */
      if (! fsysdep_cu_copy (FALSE)
	  || ! fsysdep_terminal_restore ())
	ucuabort ();
      fCurestore_terminal = FALSE;
      {
	enum tshell_cmd t;

	switch (bcmd)
	  {
	  default:
	  case '!': t = SHELL_NORMAL; break;
	  case '$': t = SHELL_STDOUT_TO_PORT; break;
	  case '|': t = SHELL_STDIN_FROM_PORT; break;
	  case '+': t = SHELL_STDIO_ON_PORT; break;
	  }
	  
	(void) fsysdep_shell (zline, t);
      }
      if (! fsysdep_cu_copy (TRUE)
	  || ! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
      return TRUE;

    case '%':
      return fcudo_subcmd (zline);

    case '#':
      if (! fport_break ())
	ucuabort ();
      return TRUE;

    case 'c':
      (void) fsysdep_chdir (zline);
      return TRUE;

    case '>':
    case '<':
    case 'p':
    case 't':
      z = (char *) alloca (strlen (zline) + 2);
      sprintf (z, "%c %s", bcmd, zline);
      return fcudo_subcmd (z);

    case 'z':
      if (! fsysdep_cu_copy (FALSE)
	  || ! fsysdep_terminal_restore ())
	ucuabort ();
      fCurestore_terminal = FALSE;
      if (! fsysdep_suspend ())
	ucuabort ();
      if (! fsysdep_cu_copy (TRUE)
	  || ! fsysdep_terminal_raw (fCulocalecho))
	ucuabort ();
      fCurestore_terminal = TRUE;
      return TRUE;
      
    case 's':
      return fcuset_var (zline);

    case 'v':
      uculist_vars ();
      return TRUE;

    case '?':
      if (! isprint (*zCuvar_escape))
	sprintf (abescape, "\\%03o", (unsigned int) *zCuvar_escape);
      else
	{
	  abescape[0] = *zCuvar_escape;
	  abescape[1] = '\0';
	}
      ucuputs ("");
      ucuputs ("[Escape sequences]");
      sprintf (abbuf,
	       "[%s. hangup]                   [%s!CMD run shell]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s$CMD stdout to remote]      [%s|CMD stdin from remote]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s+CMD stdin and stdout to remote]",
	       abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s# send break]               [%scDIR change directory]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s> send file]                [%s< receive file]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%spFROM TO send to Unix]      [%stFROM TO receive from Unix]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%ssVAR VAL set variable]      [%ssVAR set boolean]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%ss!VAR unset boolean]        [%sv list variables]",
	       abescape, abescape);
      ucuputs (abbuf);
#ifdef SIGTSTP
      sprintf (abbuf,
	       "[%sz suspend]",
	       abescape);
      ucuputs (abbuf);
#endif
      sprintf (abbuf,
	       "[%s%%break send break]         [%s%%cd DIR change directory]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s%%put FROM TO send file]    [%s%%take FROM TO receive file]",
	       abescape, abescape);
      ucuputs (abbuf);
      sprintf (abbuf,
	       "[%s%%nostop no XON/XOFF]       [%s%%stop use XON/XOFF]",
	       abescape, abescape);
      ucuputs (abbuf);
      return TRUE;
    }
}

/* Set a variable.  */

static boolean
fcuset_var (zline)
     char *zline;
{
  char *zvar, *zval;
  char *azargs[2];

  zvar = strtok (zline, "= \t");
  if (zvar == NULL)
    {
      ucuputs (abCuconnected);
      return TRUE;
    }

  zval = strtok ((char *) NULL, " \t");

  if (zval == NULL)
    {
      azargs[0] = zvar;
      azargs[1] = (char *) alloca (2);
      if (azargs[0][0] != '!')
	azargs[1][0] = 't';
      else
	{
	  ++azargs[0];
	  azargs[1][0] = 'f';
	}
      azargs[1][1] = '\0';
    }
  else
    {
      azargs[0] = zvar;
      azargs[1] = zval;
    }

  (void) tprocess_one_cmd (2, azargs, asCuvars, azargs[0],
			   CMDFLAG_WARNUNRECOG);
  return TRUE;
}

/* List all the variables with their values.  */

static void
uculist_vars ()
{
  const struct scmdtab *q;
  char abbuf[100];

  ucuputs ("");
  for (q = asCuvars; q->zcmd != NULL; q++)
    {
      switch (TTYPE_CMDTABTYPE (q->itype))
	{
	case TTYPE_CMDTABTYPE (CMDTABTYPE_BOOLEAN):
	  if (*(boolean *) q->pvar)
	    sprintf (abbuf, "%s true", q->zcmd);
	  else
	    sprintf (abbuf, "%s false", q->zcmd);
	  break;

	case TTYPE_CMDTABTYPE (CMDTABTYPE_INT):
	  sprintf (abbuf, "%s %d", q->zcmd, *(int *) q->pvar);
	  break;

	case TTYPE_CMDTABTYPE (CMDTABTYPE_LONG):
	  sprintf (abbuf, "%s %ld", q->zcmd, *(long *) q->pvar);
	  break;

	case TTYPE_CMDTABTYPE (CMDTABTYPE_STRING):
	case TTYPE_CMDTABTYPE (CMDTABTYPE_FULLSTRING):
	  {
	    const char *z;
	    char abchar[5];
	    int clen;

	    sprintf (abbuf, "%s ", q->zcmd);
	    clen = strlen (abbuf);
	    for (z = *(const char **) q->pvar; *z != '\0'; z++)
	      {
		int cchar;

		if (! isprint (*z))
		  {
		    sprintf (abchar, "\\%03o", (unsigned int) *z);
		    cchar = 4;
		  }
		else
		  {
		    abchar[0] = *z;
		    abchar[1] = '\0';
		    cchar = 1;
		  }
		if (clen + cchar < sizeof (abbuf))
		  strcat (abbuf, abchar);
		clen += cchar;
	      }
	  }
	  break;

	default:
	  sprintf (abbuf, "%s [unprintable type]", q->zcmd);
	  break;
	}

      ucuputs (abbuf);
    }
}

/* Subcommands.  These are commands that begin with ~%.  */

/* This variable is only used so that we can pass a non-NULL address
   in pvar.  It is never assigned to or examined.  */

static char bCutype;

/* The command table for the subcommands.  */

static enum tcmdtabret tcubreak P((int argc, char **argv, pointer pvar,
				   const char *zerr));
static enum tcmdtabret tcudebug P((int argc, char **argv, pointer pvar,
				   const char *zerr));
static enum tcmdtabret tcuchdir P((int argc, char **argv, pointer pvar,
				   const char *zerr));
static enum tcmdtabret tcuput P((int argc, char **argv, pointer pvar,
				 const char *zerr));
static enum tcmdtabret tcutake P((int argc, char **argv, pointer pvar,
				  const char *zerr));
static enum tcmdtabret tcunostop P((int argc, char **argv, pointer pvar,
				    const char *zerr));

static struct scmdtab asCucmds[] =
{
  { "break", CMDTABTYPE_FN | 1, NULL, tcubreak },
  { "b", CMDTABTYPE_FN | 1, NULL, tcubreak },
  { "cd", CMDTABTYPE_FN | 0, NULL, tcuchdir },
  { "d", CMDTABTYPE_FN | 1, NULL, tcudebug },
  { "put", CMDTABTYPE_FN | 0, NULL, tcuput },
  { "take", CMDTABTYPE_FN | 0, NULL, tcutake },
  { "nostop", CMDTABTYPE_FN | 1, NULL, tcunostop },
  { "stop", CMDTABTYPE_FN | 1, &bCutype, tcunostop },
  { ">", CMDTABTYPE_FN | 0, &bCutype, tcuput },
  { "<", CMDTABTYPE_FN | 0, &bCutype, tcutake },
  { "p", CMDTABTYPE_FN | 0, NULL, tcuput },
  { "t", CMDTABTYPE_FN | 0, NULL, tcutake },
  { NULL, 0, NULL, NULL }
};

/* Do a subcommand.  This is called by commands beginning with ~%.  */

static boolean
fcudo_subcmd (zline)
     char *zline;
{
  char *azargs[3];
  int iarg;

  for (iarg = 0; iarg < 3; iarg++)
    {
      azargs[iarg] = strtok (iarg == 0 ? zline : (char *) NULL, " \t\n");
      if (azargs[iarg] == NULL)
	break;
    }

  if (iarg == 0)
    {
      ucuputs (abCuconnected);
      return TRUE;
    }

  (void) tprocess_one_cmd (iarg, azargs, asCucmds, "",
			   CMDFLAG_WARNUNRECOG);
  return TRUE;
}

/* Send a break.  */

static enum tcmdtabret
tcubreak (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
  if (! fport_break ())
    ucuabort ();
  return CMDTABRET_CONTINUE;
}

/* Change directories.  */

static enum tcmdtabret
tcuchdir (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
  const char *zarg;

  if (argc <= 1)
    zarg = NULL;
  else
    zarg = argv[1];
  (void) fsysdep_chdir (zarg);
  return CMDTABRET_CONTINUE;
}

/* Toggle debugging.  */

static enum tcmdtabret
tcudebug (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
#if DEBUG > 1
  if (iDebug != 0)
    iDebug = 0;
  else
    iDebug = DEBUG_MAX;
#else
  ucuputs ("[compiled without debugging]");
#endif
  return CMDTABRET_CONTINUE;
}

/* Control whether the port does xon/xoff handshaking.  If pvar is not
   NULL, this is "stop"; otherwise it is "nostop".  */

static enum tcmdtabret
tcunostop (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
  if (! fport_set (PARITYSETTING_DEFAULT, STRIPSETTING_DEFAULT,
		   pvar == NULL ? XONXOFF_OFF : XONXOFF_ON))
    ucuabort ();
  return CMDTABRET_CONTINUE;
}

/* Send a file to the remote system.  The first argument is the file
   to send.  If that argument is not present, it is prompted for.  The
   second argument is to file name to use on the remote system.  If
   that argument is not present, the basename of the local filename is
   used.  If pvar is not NULL, then this is ~>, which is used to send
   a command to a non-Unix system.  We treat is the same as ~%put,
   except that we assume the user has already entered the appropriate
   command (for ~%put, we force ``cat >to'' to the other side).  */

static enum tcmdtabret
tcuput (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
  const char *zfrom, *zto;
  char *zalc;
  FILE *e;
  int cline;

  if (argc > 1)
    zfrom = argv[1];
  else
    {
      zfrom = zsysdep_terminal_line ("File to send: ");
      if (zfrom == NULL)
	ucuabort ();

      zalc = (char *) alloca (strlen (zfrom) + 1);
      strcpy (zalc, zfrom);
      zalc[strcspn (zalc, " \t\n")] = '\0';
      zfrom = zalc;

      if (*zfrom == '\0')
	{
	  ucuputs (abCuconnected);
	  return CMDTABRET_CONTINUE;
	}
    }

  if (argc > 2)
    zto = argv[2];
  else
    {
      const char *zconst;
      char *zbase;
      char *zprompt;

      zconst = zsysdep_base_name (zfrom);
      if (zconst == NULL)
	ucuabort ();

      zbase = (char *) alloca (strlen (zconst) + 1);
      strcpy (zbase, zconst);

      zprompt = (char *) alloca (sizeof "Remote file name []: " +
				 strlen (zbase));
      sprintf (zprompt, "Remote file name [%s]: ", zbase);
      zto = zsysdep_terminal_line (zprompt);
      if (zto == NULL)
	ucuabort ();

      zalc = (char *) alloca (strlen (zto) + 1);
      strcpy (zalc, zto);
      zalc[strcspn (zalc, " \t\n")] = '\0';
      zto = zalc;

      if (*zto == '\0')
	zto = zbase;
    }

  e = fopen (zfrom, fCuvar_binary ? BINREAD : "r");
  if (e == NULL)
    {
      const char *zerrstr;

      zerrstr = strerror (errno);
      zalc = (char *) alloca (strlen (zfrom) + sizeof ": "
			      + strlen (zerrstr));
      sprintf (zalc, "%s: %s", zfrom, zerrstr);
      ucuputs (zalc);
      ucuputs (abCuconnected);
      return CMDTABRET_CONTINUE;
    }

  /* Tell the system dependent layer to stop copying data from the
     port to the terminal.  We want to read the echoes ourself.  Also
     permit the local user to generate signals.  */
  if (! fsysdep_cu_copy (FALSE)
      || ! fsysdep_terminal_signals (TRUE))
    ucuabort ();

  /* If pvar is NULL, then we are sending a file to a Unix system.  We
     send over the command "cat > TO" to prepare it to receive.  If
     pvar is not NULL, the user is assumed to have set up whatever
     action was needed to receive the file.  */

  if (pvar == NULL)
    {
      zalc = (char *) alloca (sizeof "cat > \n" + strlen (zto));
      sprintf (zalc, "cat > %s\n", zto);
      if (! fcusend_buf (zalc, strlen (zalc)))
	{
	  (void) fclose (e);
	  if (! fsysdep_cu_copy (TRUE)
	      || ! fsysdep_terminal_signals (FALSE))
	    ucuabort ();
	  ucuputs (abCuconnected);
	  return CMDTABRET_CONTINUE;
	}
    }

  cline = 0;

  while (TRUE)
    {
      char abbuf[512];
      char *zline;
      int c;

      if (fCuvar_binary)
	{
	  c = fread (abbuf, sizeof (char), sizeof abbuf, e);
	  if (c == 0)
	    {
	      if (ferror (e))
		ucuputs ("[file read error]");
	      break;
	    }
	  zline = abbuf;
	}
      else
	{
	  zline = zfgets (e, FALSE);
	  if (zline == NULL)
	    break;
	  c = strlen (zline);
	}

      if (fCuvar_verbose)
	{
	  ++cline;
	  printf ("%d ", cline);
	  (void) fflush (stdout);
	}

      if (! fcusend_buf (zline, c))
	{
	  if (! fCuvar_binary)
	    xfree ((pointer) zline);
	  (void) fclose (e);
	  if (! fsysdep_cu_copy (TRUE)
	      || ! fsysdep_terminal_signals (FALSE))
	    ucuabort ();
	  ucuputs (abCuconnected);
	  return CMDTABRET_CONTINUE;
	}

      if (! fCuvar_binary)
	xfree ((pointer) zline);
    }

  (void) fclose (e);

  if (pvar != NULL && *zCuvar_eofwrite != '\0')
    {
      if (! fport_write (zCuvar_eofwrite, strlen (zCuvar_eofwrite)))
	ucuabort ();
    }

  if (fCuvar_verbose)
    ucuputs ("");

  ucuputs ("[file transfer complete]");

  if (! fsysdep_cu_copy (TRUE)
      || ! fsysdep_terminal_signals (FALSE))
    ucuabort ();

  ucuputs (abCuconnected);
  return CMDTABRET_CONTINUE;
}

/* Get a file from the remote side.  This is ~%take, or ~t, or ~<.
   The first two are assumed to be taking the file from a Unix system,
   so we force the command "cat FROM; echo  */

static enum tcmdtabret
tcutake (argc, argv, pvar, zerr)
     int argc;
     char **argv;
     pointer pvar;
     const char *zerr;
{
  const char *zfrom, *zto, *zcmd, *zeof;
  char *zalc;
  FILE *e;
  char bcr;
  int ceoflen;
  char *zlook = NULL;
  int ceofhave;

  if (argc > 1)
    zfrom = argv[1];
  else
    {
      zfrom = zsysdep_terminal_line ("Remote file to retreive: ");
      if (zfrom == NULL)
	ucuabort ();

      zalc = (char *) alloca (strlen (zfrom) + 1);
      strcpy (zalc, zfrom);
      zalc[strcspn (zalc, " \t\n")] = '\0';
      zfrom = zalc;

      if (*zfrom == '\0')
	{
	  ucuputs (abCuconnected);
	  return CMDTABRET_CONTINUE;
	}
    }

  if (argc > 2)
    zto = argv[2];
  else
    {
      const char *zconst;
      char *zbase;
      char *zprompt;

      zconst = zsysdep_base_name (zfrom);
      if (zconst == NULL)
	ucuabort ();

      zbase = (char *) alloca (strlen (zconst) + 1);
      strcpy (zbase, zconst);

      zprompt = (char *) alloca (sizeof "Local file name []: " +
				 strlen (zbase));
      sprintf (zprompt, "Local file name [%s]: ", zbase);
      zto = zsysdep_terminal_line (zprompt);
      if (zto == NULL)
	ucuabort ();

      zalc = (char *) alloca (strlen (zto) + 1);
      strcpy (zalc, zto);
      zalc[strcspn (zalc, " \t\n")] = '\0';
      zto = zalc;
      
      if (*zto == '\0')
	zto = zbase;
    }

  if (pvar != NULL)
    {
      zcmd = zsysdep_terminal_line ("Remote command to execute: ");
      if (zcmd == NULL)
	ucuabort ();

      zalc = (char *) alloca (strlen (zcmd) + 1);
      strcpy (zalc, zcmd);
      zalc[strcspn (zalc, "\n")] = '\0';
      zcmd = zalc;

      zeof = zCuvar_eofread;
    }
  else
    {
      zalc = (char *) alloca (sizeof "cat ; echo; echo ////cuend////"
			      + strlen (zfrom));
      sprintf (zalc, "cat %s; echo; echo ////cuend////", zfrom);
      zcmd = zalc;
      zeof = "\n////cuend////\n";
    }

  e = fopen (zto, fCuvar_binary ? BINWRITE : "w");
  if (e == NULL)
    {
      const char *zerrstr;

      zerrstr = strerror (errno);
      zalc = (char *) alloca (strlen (zto) + sizeof ": "
			      + strlen (zerrstr));
      sprintf (zalc, "%s: %s\n", zto, zerrstr);
      ucuputs (zalc);
      ucuputs (abCuconnected);
      return CMDTABRET_CONTINUE;
    }

  if (! fsysdep_cu_copy (FALSE)
      || ! fsysdep_terminal_signals (TRUE))
    ucuabort ();

  if (! fport_write (zcmd, strlen (zcmd)))
    ucuabort ();
  bcr = '\r';
  if (! fport_write (&bcr, 1))
    ucuabort ();

  /* Eliminated any previously echoed data to avoid confusion.  */
  iPrecstart = 0;
  iPrecend = 0;

  /* If we're dealing with a Unix system, we can reliably discard the
     command.  Otherwise, the command will probably wind up in the
     file; too bad.  */
  if (pvar == NULL)
    {
      int b;

      while ((b = breceive_char (cCuvar_timeout, TRUE)) != '\n')
	{
	  if (b == -2)
	    ucuabort ();
	  if (b < 0)
	    {
	      ucuputs ("[timed out waiting for newline]");
	      ucuputs (abCuconnected);
	      return CMDTABRET_CONTINUE;
	    }
	}
    }

  ceoflen = strlen (zeof);
  if (ceoflen > 0)
    zlook = (char *) alloca (ceoflen);
  ceofhave = 0;

  while (TRUE)
    {
      int b;

      if (FGOT_SIGNAL ())
	{
	  /* Make sure the signal is logged.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  ucuputs ("[file receive aborted]");
	  break;
	}	

      b = breceive_char (cCuvar_timeout, TRUE);
      if (b == -2)
	ucuabort ();
      if (b < 0)
	{
	  if (ceofhave > 0)
	    (void) fwrite (zlook, sizeof (char), ceofhave, e);
	  ucuputs ("[timed out]");
	  break;
	}

      if (ceoflen == 0)
	(void) putc (b, e);
      else
	{
	  zlook[ceofhave] = b;
	  ++ceofhave;
	  if (ceofhave == ceoflen)
	    {
	      if (memcmp (zeof, zlook, ceoflen) == 0)
		{
		  ucuputs ("[file transfer complete]");
		  break;
		}

	      (void) putc (*zlook, e);
	      xmemmove (zlook, zlook + 1, ceoflen - 1);
	      --ceofhave;
	    }
	}
    }

  if (ferror (e) || fclose (e) == EOF)
    ucuputs ("[file write error]");

  if (! fsysdep_cu_copy (TRUE)
      || ! fsysdep_terminal_signals (FALSE))
    ucuabort ();

  ucuputs (abCuconnected);

  return CMDTABRET_CONTINUE;
}

/* Send a buffer to the remote system.  If fCuvar_binary is FALSE,
   each buffer passed in will be a single line; in this case we can
   check the echoed characters and kill the line if they do not match.
   This returns FALSE if an echo check fails.  If a port error
   occurrs, it calls ucuabort.  */

static boolean
fcusend_buf (zbufarg, cbufarg)
     const char *zbufarg;
     int cbufarg;
{
  const char *zbuf;
  int cbuf;
  int ctries;
  int cbplen;
  char *zsendbuf;

  zbuf = zbufarg;
  cbuf = cbufarg;
  ctries = 0;

  if (fCuvar_binary)
    cbplen = strlen (zCuvar_binary_prefix);
  else
    cbplen = 1;
  zsendbuf = (char *) alloca (64 * (cbplen + 1));

  /* Loop while we still have characters to send.  The value of cbuf
     will be reset to cbufarg if an echo failure occurs while sending
     a line in non-binary mode.  */
  while (cbuf > 0)
    {
      int csend;
      char *zput;
      const char *zget;
      int i;

      if (FGOT_SIGNAL ())
	{
	  /* Make sure the signal is logged.  */
	  ulog (LOG_ERROR, (const char *) NULL);
	  ucuputs ("[file send aborted]");
	  return FALSE;
	}

      /* Discard anything we've read from the port up to now, to avoid
	 confusing the echo checking.  */
      iPrecstart = 0;
      iPrecend = 0;

      /* Send all characters up to a newline before actually sending
	 the newline.  This makes it easier to handle the special
	 newline echo checking.  Send up to 64 characters at a time
	 before doing echo checking.  */
      if (*zbuf == '\n')
	csend = 1;
      else
	{
	  const char *znl;

	  znl = memchr (zbuf, '\n', cbuf);
	  if (znl == NULL)
	    csend = cbuf;
	  else
	    csend = znl - zbuf;
	  if (csend > 64)
	    csend = 64;
	}

      /* Translate this part of the buffer.  If we are not in binary
	 mode, we translate \n to \r, and ignore any nonprintable
	 characters.  */
      zput = zsendbuf;
      for (i = 0, zget = zbuf; i < csend; i++, zget++)
	{
	  if (isprint (*zget)
	      || *zget == '\t')
	    *zput++ = *zget;
	  else if (*zget == '\n')
	    {
	      if (fCuvar_binary)
		*zput++ = '\n';
	      else
		*zput++ = '\r';
	    }
	  else if (fCuvar_binary)
	    {
	      strcpy (zput, zCuvar_binary_prefix);
	      zput += cbplen;
	      *zput++ = *zget;
	    }
	}
		
      zbuf += csend;
      cbuf -= csend;

      if (zput == zsendbuf)
	continue;

      /* Send the data over the port.  */
      if (! fsend_data (zsendbuf, zput - zsendbuf, TRUE))
	ucuabort ();

      /* We do echo checking if requested, unless we are in binary
	 mode.  Echo checking of a newline is different from checking
	 of normal characters; when we send a newline we look for
	 *zCuvar_echonl.  */
      if ((fCuvar_echocheck && ! fCuvar_binary)
	  || (*zbuf == '\n' && *zCuvar_echonl != '\0'))
	{
	  long iend;

	  iend = isysdep_time ((long *) NULL) + (long) cCuvar_timeout;
	  for (i = 0, zget = zbuf; i < csend; i++, zget++)
	    {
	      int bread;
	      int bwant;

	      if (*zget == '\n')
		{
		  bwant = *zCuvar_echonl;
		  if (bwant == '\0')
		    continue;
		}
	      else
		{
		  if (! isprint (*zget))
		    continue;
		  bwant = *zget;
		}

	      do
		{
		  bread = breceive_char (iend - isysdep_time ((long *) NULL),
					 TRUE);
		  if (bread < 0)
		    {
		      if (bread == -2)
			ucuabort ();

		      /* If we timed out, and we're not in binary
			 mode, we kill the line and try sending it
			 again from the beginning.  */
		      if (! fCuvar_binary && *zCuvar_kill != '\0')
			{
			  ++ctries;
			  if (ctries < cCuvar_resend)
			    {
			      if (fCuvar_verbose)
				{
				  printf ("R ");
				  (void) fflush (stdout);
				}
			      if (! fsend_data (zCuvar_kill, 1, TRUE))
				ucuabort ();
			      zbuf = zbufarg;
			      cbuf = cbufarg;
			      break;
			    }
			}
		      ucuputs ("[timed out looking for echo]");
		      return FALSE;
		    }
		}
	      while (bread != *zget);

	      if (bread < 0)
		break;
	    }
	}
    }

  return TRUE;
}
