/* policy.h
   Configuration file for policy decisions.  To be edited on site.

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
   */

/* This header file contains macro definitions which must be set by
   each site before compilation.  The first few are system
   characteristics that can not be easily discovered by the
   configuration script.  Most are configuration decisions that must
   be made by the local administrator.  */

/* System characteristics.  */

/* This code tries to use several ANSI C features, including
   prototypes, stdarg.h, the const qualifier and the types void
   (including void * pointers) and unsigned char.  By default it will
   use these features if the compiler defines __STDC__.  If your
   compiler supports these features but does not define __STDC__, you
   should define ANSI_C to 1.  If your compiler does not support these
   features but defines __STDC__ (no compiler should do this, in my
   opinion), you should define ANSI_C to 0.  In most cases (or if
   you're not sure) just leave the line below commented out.  */
/* #define ANSI_C 1 */

/* Set USE_STDIO to 1 if data files should be read using the stdio
   routines (fopen, fread, etc.) rather than the UNIX unbuffered I/O
   calls (open, read, etc.).  Unless you know your stdio is really
   rotten, you should leave this as 1.  */
#define USE_STDIO 1

/* Exactly one of the following macros must be set to 1.  Many modern
   systems support more than one of these choices through some form of
   compilation environment, in which case the setting will depend on
   the compilation environment you use.  If you have a reasonable
   choice between options, I suspect that TERMIO or TERMIOS will be
   more efficient than TTY, but I have not done any head to head
   comparisons.

   If you don't set any of these macros, the code below will guess.
   It will doubtless be wrong on some systems.

   HAVE_BSD_TTY -- Use the 4.2BSD tty routines
   HAVE_SYSV_TERMIO -- Use the System V termio routines
   HAVE_POSIX_TERMIOS -- Use the POSIX termios routines
   */
#define HAVE_BSD_TTY 0
#define HAVE_SYSV_TERMIO 0
#define HAVE_POSIX_TERMIOS 0

/* This code tries to guess which terminal driver to use if you did
   not make a choice above.  It is in this file to make it easy to
   figure out what's happening if something goes wrong.  */

#if HAVE_BSD_TTY + HAVE_SYSV_TERMIO + HAVE_POSIX_TERMIOS == 0
#if HAVE_CBREAK
#undef HAVE_BSD_TTY
#define HAVE_BSD_TTY 1
#else
#undef HAVE_SYSV_TERMIO
#define HAVE_SYSV_TERMIO 1
#endif
#endif

/* Set TIMES_TICK to the fraction of a second which times(2) returns
   (for example, if times returns 100ths of a second TIMES_TICK should
   be set to 100).  On a true POSIX system TIMES_TICK may simply be
   defined as CLK_TCK.  On some systems the environment variable HZ is
   what you want for TIMES_TICK, but on some other systems HZ has the
   wrong value; check the man page.  If you leave this set to 0, the
   code will try to guess; it will doubtless be wrong on some systems.
   If TIMES_TICK is wrong the code may report incorrect file transfer
   times in the statistics file, but on many systems times(2) will
   actually not be used and this value will not matter at all.  */
#define TIMES_TICK 0

/* Set PS_PROGRAM to the program to run to get a process status,
   including the arguments to pass it.  This is used by ``uustat -p''.
   Set HAVE_PS_MULTIPLE to 1 if a comma separated list of process
   numbers may be appended (e.g. ``ps -flp1,10,100'').  Otherwise ps
   will be invoked multiple times, with a single process number append
   each time.  The default definitions should work on most systems,
   although some may complain about the 'p' option.  The commented out
   definitions are appropriate for System V.  */
#define PS_PROGRAM "/bin/ps -lp"
#define HAVE_PS_MULTIPLE 0
/* #define PS_PROGRAM "/bin/ps -flp" */
/* #define HAVE_PS_MULTIPLE 1 */

/* If you use other programs that also lock devices, such as cu or
   uugetty, the other programs and UUCP must agree on whether a device
   is locked.  This is typically done by creating a lock file in a
   specific directory.  The lock file is named LCK.. followed by the
   name of the device (UUCP and some versions of cu also lock systems
   this way).  If the LOCKDIR macro is defined, these lock files will
   be placed in the named directory; otherwise they will be placed in
   the default spool directory.  On some BNU systems the lock files
   are placed in /etc/locks.  On some they are placed in
   /usr/spool/locks.  */
/* #define LOCKDIR "/etc/locks" */
/* #define LOCKDIR "/usr/spool/locks" */

/* You must also specify the format of the lock files by setting
   exactly one of the following macros to 1.  The BNU style is to
   write the locking process ID in ASCII, passed to ten characters,
   followed by a newline.  The V2 style is to write the locking
   process ID as four binary bytes in the host byte order.  Check an
   existing lock file to decide which of these choices is more
   appropriate.  */
#define HAVE_V2_LOCKFILES 0
#define HAVE_BNU_LOCKFILES 1

/* Adminstrative decisions.  */

/* Set USE_RCS_ID to 1 if you want the RCS ID strings compiled into
   the executable.  Leaving them out will decrease the executable
   size.  Leaving them in will make it easier to determine which
   version you are running.  */
#define USE_RCS_ID 1

/* Set DEBUG if you want to compile debugging information into the
   executable.  Defining it as 0 will not compile any debugging
   messages or checks.  Increasing numbers add more messages, up to 9.
   Unless you want the smallest possible executable file, you should
   leave this as 9.  */
#define DEBUG 9

/* Set the default grade to use for a uucp command if the -g option is
   not used.  The grades, from highest to lowest, are 0 to 9, A to Z,
   a to z.  */
#define BDEFAULT_UUCP_GRADE ('n')

/* Set the default grade to use for a uux command if the -g option is
   not used.  */
#define BDEFAULT_UUX_GRADE ('A')

/* The maximum number of times to retry calling a system which is not
   answering.  If this many calls to the system have failed, the
   system will not be called again until the status file has been
   removed (on a Unix system the status file is in the .Status
   subdirectory of the main spool directory, and has the same name as
   the system name).  If this is set to 0 the system may be called
   regardless of how many previous calls have failed.  */
#define CMAXRETRIES (26)

/* To compile in use of the new style of configuration files described
   in the documentation, set HAVE_TAYLOR_CONFIG to 1.  */
#define HAVE_TAYLOR_CONFIG 1

/* To compile in use of V2 style configuration files (L.sys, L-devices
   and so on), set HAVE_V2_CONFIG to 1.  To compile in use of BNU
   style configuration files (Systems, Devices and so on) set
   HAVE_BNU_CONFIG to 1.  The files will be looked up in the
   oldconfigdir directory as defined in the Makefile.  */
#define HAVE_V2_CONFIG 0
#define HAVE_BNU_CONFIG 0

/* Exactly one of the following macros must be set to 1.  The exact
   format of the spool directories is explained in sys3.unx.

   SPOOLDIR_V2 -- Use a Version 2 (original UUCP) style spool directory
   SPOOLDIR_BSD42 -- Use a BSD 4.2 style spool directory
   SPOOLDIR_BSD43 -- Use a BSD 4.3 style spool directory
   SPOOLDIR_BNU -- Use a BNU (HDB) style spool directory
   SPOOLDIR_ULTRIX -- Use an Ultrix style spool directory
   SPOOLDIR_TAYLOR -- Use a new style spool directory

   If you are not worried about compatibility with a currently running
   UUCP, use SPOOLDIR_TAYLOR.  */
#define SPOOLDIR_V2 0
#define SPOOLDIR_BSD42 0
#define SPOOLDIR_BSD43 0
#define SPOOLDIR_BNU 0
#define SPOOLDIR_ULTRIX 0
#define SPOOLDIR_TAYLOR 1

/* You must select which type of logging you want by setting exactly
   one of the following to 1.  These control output to the log file
   and to the statistics file.

   If you define HAVE_TAYLOR_LOGGING, each line in the log file will
   look something like this:

   uucico uunet uucp (1991-12-10 09:04:34.45 16390) Receiving uunet/D./D.uunetSwJ72

   and each line in the statistics file will look something like this:

   uucp uunet (1991-12-10 09:04:40.20) received 2371 bytes in 5 seconds (474 bytes/sec)

   If you define HAVE_V2_LOGGING, each line in the log file will look
   something like this:

   uucico uunet uucp (12/10-09:04 16390) Receiving uunet/D./D.uunetSwJ72

   and each line in the statistics file will look something like this:

   uucp uunet (12/10-09:04 16390) (692373862) received data 2371 bytes 5 seconds

   If you define HAVE_BNU_LOGGING, each program will by default use a
   separate log file.  For uucico talking to uunet, for example, it
   will be /usr/spool/uucp/.Log/uucico/uunet.  Each line will look
   something like this:

   uucp uunet (12/10-09:04:22,16390,1) Receiving uunet/D./D.uunetSwJ72

   and each line in the statistics file will look something like this:

   uunet!uucp M (12/10-09:04:22) (C,16390,1) [ttyXX] <- 2371 / 5.000 secs, 474 bytes/sec

   The main reason to prefer one format over another is that you may
   have shell scripts which expect the files to have a particular
   format.  If you have none, choose whichever format you find more
   appealing.  */
#define HAVE_TAYLOR_LOGGING 1
#define HAVE_V2_LOGGING 0
#define HAVE_BNU_LOGGING 0

/* The name of the default spool directory.  If HAVE_TAYLOR_CONFIG is
   set to 1, this may be overridden by the ``spool'' command in the
   configuration file.  */
#define SPOOLDIR "/usr/spool/uucp"

/* The name of the default public directory.  If HAVE_TAYLOR_CONFIG is
   set to 1, this may be overridden by the ``pubdir'' command in the
   configuration file.  Also, a particular system may be given a
   specific public directory by using the ``pubdir'' command in the
   system file.  */
#define PUBDIR "/usr/spool/uucppublic"

/* The default command path, specifying which directories the commands
   to be executed must be located in.  */
#define CMDPATH "/bin /usr/bin /usr/local/bin"

/* The default amount of free space to require for systems that do not
   specify an amount with the ``free-space'' command.  This is only
   used when talking to another instance of Taylor UUCP; if accepting
   a file would not leave at least this many bytes free on the disk,
   it will be refused.  */
#define DEFAULT_FREE_SPACE (50000)

/* It is possible for an execute job to request to be executed using
   sh(1), rather than execve(2).  This is such a security risk, it is
   being disabled by default; to allow such jobs, set the following
   macro to 1.  */
#define ALLOW_SH_EXECUTION 0

/* If a command executed on behalf of a remote system takes a filename
   as an argument, a security breach may be possible (note that on my
   system neither of the default commands, rmail and rnews, take
   filename arguments).  If you set ALLOW_FILENAME_ARGUMENTS to 0, all
   arguments to a command will be checked; if any argument
   1) starts with ../
   2) contains the string /../
   3) begins with a / but does not name a file that may be sent or
      received (according to the specified ``remote-send'' and
      ``remote-receive'')
   the command will be rejected.  By default, any argument is
   permitted. */
#define ALLOW_FILENAME_ARGUMENTS 1

#if HAVE_TAYLOR_LOGGING

/* The default log file when using HAVE_TAYLOR_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``logfile''
   command in the configuration file.  */
#define LOGFILE "/usr/spool/uucp/Log"

/* The default statistics file when using HAVE_TAYLOR_LOGGING.  When
   using HAVE_TAYLOR_CONFIG, this may be overridden by the
   ``statfile'' command in the configuration file.  */
#define STATFILE "/usr/spool/uucp/Stats"

/* The default debugging file when using HAVE_TAYLOR_LOGGING.  When
   using HAVE_TAYLOR_CONFIG, this may be overridden by the
   ``debugfile'' command in the configuration file.  */
#define DEBUGFILE "/usr/spool/uucp/Debug"

#endif /* HAVE_TAYLOR_LOGGING */

#if HAVE_V2_LOGGING

/* The default log file when using HAVE_V2_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``logfile''
   command in the configuration file.  */
#define LOGFILE "/usr/spool/uucp/LOGFILE"

/* The default statistics file when using HAVE_V2_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``statfile''
   command in the configuration file.  */
#define STATFILE "/usr/spool/uucp/SYSLOG"

/* The default debugging file when using HAVE_V2_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``debugfile''
   command in the configuration file.  */
#define DEBUGFILE "/usr/spool/uucp/DEBUG"

#endif /* HAVE_V2_LOGGING */

#if HAVE_BNU_LOGGING

/* The default log file when using HAVE_BNU_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``logfile''
   command in the configuration file.  The first %s in the string will
   be replaced by the program name (e.g. uucico); the second %s will
   be replaced by the system name (if there is no appropriate system,
   "ANY" will be used).  No other '%' character may appear in the
   string.  */
#define LOGFILE "/usr/spool/uucp/.Log/%s/%s"

/* The default statistics file when using HAVE_BNU_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``statfile''
   command in the configuration file.  */
#define STATFILE "/usr/spool/uucp/.Admin/xferstats"

/* The default debugging file when using HAVE_BNU_LOGGING.  When using
   HAVE_TAYLOR_CONFIG, this may be overridden by the ``debugfile''
   command in the configuration file.  */
#define DEBUGFILE "/usr/spool/uucp/.Admin/audit.local"

#endif /* HAVE_BNU_LOGGING */
