/* uucp.h
   Header file for the UUCP package.

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
   Revision 1.11  1991/11/21  21:20:41  ian
   Brian Campbell: offer str{n}icmp as an alternative to str{n}casecmp

   Revision 1.10  1991/11/12  19:47:04  ian
   Add called-chat set of commands to run a chat script on an incoming call

   Revision 1.9  1991/11/11  23:47:24  ian
   Added chat-program to run a program to do a chat script

   Revision 1.8  1991/11/11  19:32:03  ian
   Added breceive_char to read characters through protocol buffering

   Revision 1.7  1991/11/10  20:05:44  ian
   Changed ffilerewind to use fseek rather than rewind

   Revision 1.6  1991/11/10  19:24:22  ian
   Added pffile protocol entry point for file level control

   Revision 1.5  1991/11/07  20:32:04  ian
   Chip Salzenberg: allow ANSI_C to be defined in conf.h

   Revision 1.4  1991/11/07  18:21:47  ian
   Chip Salzenberg: move CMAXRETRIES to conf.h for easy configuration

   Revision 1.3  1991/09/19  17:43:48  ian
   Chip Salzenberg: undef TRUE and FALSE in case system defines them

   Revision 1.2  1991/09/19  02:22:44  ian
   Chip Salzenberg's patch to allow ";retrytime" at the end of a time string

   Revision 1.1  1991/09/10  19:47:55  ian
   Initial revision

   */

#ifndef UUCP_H

#define UUCP_H

#ifdef __GNUC__
 #pragma once
#endif

#include <stdio.h>
#include <unistd.h>

#include "conf.h"

/* Get a definition for ANSI_C if we weren't given one.  */

#ifndef ANSI_C
#ifdef __STDC__
#define ANSI_C 1
#else /* ! defined (__STDC__) */
#define ANSI_C 0
#endif /* ! defined (__STDC__) */
#endif /* ! defined (ANSI_C) */

/* The macro P is used when declaring prototypes, to allow a somewhat
   readable syntax for both ANSI and Classic C.  */

#if ANSI_C
#undef HAVE_VOID
#define HAVE_VOID 1
typedef void *pointer;
typedef const void *constpointer;
#define P(x) x
#define BUCHAR(b) ((unsigned char) (b))
#else /* ! ANSI_C */
#define const
#if ! HAVE_VOID
#define void int
#endif
typedef char *pointer;
typedef const char *constpointer;
#define P(x) ()
/* This isn't necessarily right, but what else can I do?  I need to
   get an unsigned char to safely pass to the ctype macros, and not
   all Classic C compilers support unsigned char.  This will work on
   all normal machines.  */
#define BUCHAR(b) ((b) & 0xff)
#endif /* ! ANSI_C */

/* Use builtin alloca if we can, and only use inline with gcc.  */

#ifdef __GNUC__
#define alloca __builtin_alloca
#undef HAVE_ALLOCA
#define HAVE_ALLOCA 1
#else /* ! __GNUC__ */
#define __inline__
#if HAVE_ALLOCA
#if NEED_ALLOCA_H
#include <alloca.h>
#else /* ! NEED_ALLOCA_H */
extern pointer alloca P((int));
#endif /* ! NEED_ALLOCA_H */
#else /* ! HAVE_ALLOCA */
extern pointer alloca P((int));
extern void uclear_alloca P((void));
#endif /* ! HAVE_ALLOCA */
#endif /* ! __GNUC__ */

/* Get what we need from <stdlib.h>.  */

#if HAVE_STDLIB_H
#include <stdlib.h>
#else /* ! HAVE_STDLIB_H */
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
extern pointer malloc (), realloc (), bsearch ();
extern void free (), exit (), perror (), abort (), qsort ();
extern long atol (), strtol ();
extern int atoi ();
extern char *getenv ();
#if HAVE_MEMORY_H
#include <memory.h>
#endif
#endif /* ! HAVE_STDLIB_H */

/* The boolean type holds boolean values.  */

typedef int boolean;

#undef TRUE
#undef FALSE
#define TRUE (1)
#define FALSE (0)

/* The openfile_t type holds an open file.  This depends on whether we
   are using stdio or not.  */

#if USE_STDIO
typedef FILE *openfile_t;
#define EFILECLOSED ((FILE *) NULL)
#define ffileisopen(e) ((e) != NULL)
#define cfileread(e, z, c) fread ((z), 1, (c), (e))
#define ffilereaderror(e, c) ferror (e)
#define cfilewrite(e, z, c) fwrite ((z), 1, (c), (e))
#define ffilerewind(e) (fseek (e, (long) 0, SEEK_SET) == 0)
#define ffileclose(e) (fclose (e) == 0)
#else
typedef int openfile_t;
#define EFILECLOSED (-1)
#define ffileisopen(e) ((e) >= 0)
#define cfileread(e, z, c) read ((e), (z), (c))
#define ffilereaderror(e, c) ((c) < 0)
#define cfilewrite(e, z, c) write ((e), (z), (c))
#define ffilerewind(e) (lseek (e, (long) 0, SEEK_SET) >= 0)
#define ffileclose(e) (close (e) >= 0)
#endif

/* The sigret_t type holds the return type of a signal function.  We
   use #define rather than typedef because at least one compiler
   doesn't like typedef void.  */

#if HAVE_INT_SIGNALS
#define sigret_t int
#else
#define sigret_t void
#endif

/* Define the time_t type.  This still won't help if they don't have
   time or ctime.  */

#if ! HAVE_TIME_T && ! HAVE_SYS_TIME_T
typedef long time_t;
#endif

/* The types of entries allowed in a command table (struct scmdtab).
   If CMDTABTYPE_FN is used, it should be or'ed with the number of
   arguments permitted, or 0 if there is no single number.  */

#define CMDTABTYPE_BOOLEAN (0x12)
#define CMDTABTYPE_INT (0x22)
#define CMDTABTYPE_LONG (0x32)
#define CMDTABTYPE_STRING (0x40)
#define CMDTABTYPE_FULLSTRING (0x50)
#define CMDTABTYPE_FN (0x60)

/* These flags are or'red together to form an argument to
   uprocesscmds.  */
#define CMDFLAG_WARNUNRECOG (0x1)
#define CMDFLAG_CASESIGNIFICANT (0x2)
#define CMDFLAG_BACKSLASH (0x4)

/* The enumeration returned by functions called by uprocesscmds.  */

enum tcmdtabret
{
  CMDTABRET_CONTINUE,
  CMDTABRET_FREE,
  CMDTABRET_EXIT,
  CMDTABRET_FREE_AND_EXIT
};

/* This structure holds the argument to uprocesscmds.  */

struct scmdtab
{
  /* Command name.  */
  const char *zcmd;
  /* Command type (one of CMDTABTYPE_...).  */
  int itype;
  /* This is the address of the variable if not CMDTABTYPE_FN.  */
  pointer pvar;
  /* This is used if CMDTABTYPE_FN.  */
  enum tcmdtabret (*ptfn) P((int argc, char **argv, pointer par,
			     const char *zerr));
};

/* This structure holds a set of special commands executed for
   particular protocols.  */

struct sproto_param
{
  /* Protocol.  */
  char bproto;
  /* Number of entries.  */
  int centries;
  /* Specific entries.  */
  struct sproto_param_entry
    {
      int cargs;
      char **azargs;
    } *qentries;
};

/* The ssysteminfo structure holds information about a remote system.  */

struct ssysteminfo
{
  /* System name.  */
  const char *zname;
  /* List of aliases separated by ' '.  */
  char *zalias;
  /* Linked list of alternate sets of call information.  */
  struct ssysteminfo *qalternate;
  /* Legal times to call.  A grade, a time string, an optional ';' and
     retry time, ' ', repeated.  */
  char *ztime;
  /* Grade to request of other system and associated time.  A grade, a
     time string, ' ', repeated.  */
  char *zcalltimegrade;
  /* Sizes for local requests and calls.  A size, ' ', a time string,
     ' ', repeated.  */
  char *zcall_local_size;
  /* Sizes for remote requests and local calls.  */
  char *zcall_remote_size;
  /* Sizes for local requests when called.  */
  char *zcalled_local_size;
  /* Sizes for remote requests when called.  */
  char *zcalled_remote_size;
  /* Baud rate (all right, so it's really bps).  */
  long ibaud;
  /* High baud rate, if a range is permitted (0 if no range).  */
  long ihighbaud;
  /* Port name, if qport is not used.  */
  char *zport;
  /* Specific port information, if zport is not used.  */
  struct sport *qport;
  /* Phone number.  */
  char *zphone;
  /* Chat program.  */
  const char *zchat_program;
  /* Chat script.  */
  char *zchat;
  /* Timeout to use for the chat script.  */
  int cchat_timeout;
  /* Failure strings for the chat script.  */
  char *zchat_fail;
  /* Login name to use when calling the remote system.  */
  const char *zcall_login;
  /* Password to use when calling the remote system.  */
  const char *zcall_password;
  /* Login name that must be used by the other system when calling in.  */
  const char *zcalled_login;
  /* Whether to call back the remote system.  */
  boolean fcallback;
  /* Whether system uses sequencing.  */
  boolean fsequence;
  /* List of protocols to use for this system (may be NULL).  */
  const char *zprotocols;
  /* Number of entries in protocol parameters array.  */
  int cproto_params;
  /* Protocol parameters array.  */
  struct sproto_param *qproto_params;
  /* Chat program to run when called.  */
  const char *zcalled_chat_program;
  /* Chat script to run when called.  */
  char *zcalled_chat;
  /* Timeout to use for the chat script to run when called.  */
  int ccalled_chat_timeout;
  /* Failure strings for the chat script to run when called.  */
  char *zcalled_chat_fail;
  /* Whether the other system may request things when we call them.  */
  boolean fcall_request;
  /* Whether the other system may request things when they call us.  */
  boolean fcalled_request;
  /* Whether we may request things when we call.  */
  boolean fcall_transfer;
  /* Whether we may request things when they call.  */
  boolean fcalled_transfer;
  /* List of directories that may be sent by local request.  */
  const char *zlocal_send;
  /* List of directories that may be sent by remote request.  */
  const char *zremote_send;
  /* List of directories that may be received into by local request.  */
  const char *zlocal_receive;
  /* List of directories that may be received into by remote request.  */
  const char *zremote_receive;
  /* Path to use for command execution.  */
  const char *zpath;
  /* List of commands that may be executed.  */
  const char *zcmds;
  /* Amount of free space to leave.  */
  long cfree_space;
  /* List of systems that may be forwarded to.  */
  const char *zforwardto;
  /* The public directory to use for this sytem.  */
  const char *zpubdir;
  /* The local name to use for this remote system.  */
  const char *zlocalname;
};

/* The tfailure enumeration holds reasons for failure to be passed to
   the pffail function of a protocol.  */

enum tfailure
{
  /* No failure.  */
  FAILURE_NONE,
  /* No permission for operation.  */
  FAILURE_PERM,
  /* Can't open necessary file.  */
  FAILURE_OPEN,
  /* Not enough space to receive file.  */
  FAILURE_SIZE
};

/* The tlog enumeration holds the different types of logging.  */

enum tlog
{
  /* Normal log entry.  */
  LOG_NORMAL,
  /* Error log entry.  */
  LOG_ERROR,
  /* Fatal log entry.  */
  LOG_FATAL,
#if DEBUG > 0
  /* Debugging log entry.  */
  LOG_DEBUG
#endif
};

/* The tstatus enumeration holds the kinds of status information we
   put in the status file.  */

enum tstatus
{
  /* Conversation complete.  */
  STATUS_COMPLETE,
  /* Port unavailable.  */
  STATUS_PORT_FAILED,
  /* Dial failed.  */
  STATUS_DIAL_FAILED,
  /* Login failed.  */
  STATUS_LOGIN_FAILED,
  /* Handshake failed.  */
  STATUS_HANDSHAKE_FAILED,
  /* Failed after logging in.  */
  STATUS_FAILED,
  /* Talking to remote system.  */
  STATUS_TALKING
};

/* An array to convert status entries to strings.  If more status entries
   are added, this array must be extended.  */
extern const char *azStatus[];

/* The sstatus structure holds the contents of a system status file.  */

struct sstatus
{
  /* Current status of conversation.  */
  enum tstatus ttype;
  /* Number of failed retries.  */
  int cretries;
  /* Time of last call in seconds since epoch (determined by
     isysdep_time).  */
  long ilast;
  /* Number of seconds until a retry is permitted.  */
  int cwait;
};

/* How long we have to wait for the next call, given the number of retries
   we have already made.  This should probably be configurable.  */
#define CRETRY_WAIT(c) ((c) * 10 * 60)

/* The scmd structure holds a complete UUCP command.  */

struct scmd
{
  /* Command ('S' for send, 'R' for receive, 'X' for execute, 'H' for
     hangup, 'Y' for hangup confirm, 'N' for hangup deny).  */
  char bcmd;
  /* Sequence handle for fsysdep_did_work.  */
  pointer pseq;
  /* File name to transfer from.  */
  const char *zfrom;
  /* File name to transfer to.  */
  const char *zto;
  /* User who requested transfer.  */
  const char *zuser;
  /* Options.  */
  const char *zoptions;
  /* Temporary file name ('S' and pfreceive protocol function).  */
  const char *ztemp;
  /* Mode to give newly created file ('S' and pfreceive protocol fn).  */
  unsigned int imode;
  /* User to notify on remote system (optional; 'S' only).  */
  const char *znotify;
  /* File size (-1 if not supplied) ('S' and pfreceive protocol fn).  */
  long cbytes;
};

/* The highest grade.  */
#define BGRADE_HIGH ('0')

/* The lowest grade.  */
#define BGRADE_LOW ('z')

/* Whether a character is a legal grade.  */
#define FGRADE_LEGAL(b) (isalnum (BUCHAR (b)))

/* The texecute_mail enumeration tells whether to mail the completation
   status of a command executed by uux.  */

enum texecute_mail
{
  /* Never mail completion status.  */
  EXECUTE_MAIL_NEVER,
  /* Mail completion status if an error occurrs.  */
  EXECUTE_MAIL_ERROR,
  /* Mail completion status if the command succeeds.  */
  EXECUTE_MAIL_SUCCESS,
  /* Always mail completion status.  */
  EXECUTE_MAIL_ALWAYS
};

/* Functions.  */

/* Read the configuration file.  */
extern void uread_config P((const char *zname));

/* Read information about all systems.  */
extern void uread_all_system_info P((int *pc, struct ssysteminfo **ppas));

/* Read information about a specific system.  */
extern boolean fread_system_info P((const char *zsystem,
				    struct ssysteminfo *qsys));

/* Set the default values for an uninitialized system.  */
extern void uset_system_defaults P((struct ssysteminfo *qsys));

/* Start getting commands for unknown systems.  */
extern void uiunknown_start P((void));

/* Process a command defining unknown systems.  */
extern enum tcmdtabret tiunknown P((int argc, char **argv,
				    pointer pvar, const char *zerr));

/* Finish getting commands for unknown systems.  */
extern void uiunknown_end P((void));

/* Set up the sLocalsys structure.  */
extern void uisetup_localsys P((void));

/* Open a set of files and pretend that they were all catenated
   together.  */
extern struct smulti_file *qmulti_open P((const char *znames));

/* Close a set of files opened by qmulti_open.  */
extern boolean fmulti_close P((struct smulti_file *q));

/* Process a set of commands.  */
extern void uprocesscmds P((FILE *e, struct smulti_file *qmulti,
			    const struct scmdtab *qcmds,
			    const char *zerr, int iflags));

/* Process a single command.  */
extern enum tcmdtabret tprocess_one_cmd P((int cargs, char **azargs,
					   const struct scmdtab *qcmds,
					   const char *zerr,
					   int iflags));

/* Translate an unknown system name into something acceptable for the
   spool directory stuff.  */
extern const char *ztranslate_system P((const char *zsystem));     

/* Check whether the we may call a system now if we have work of a
   given grade.  This returns the retry time, which may be zero to
   indicate use of the default retry backoff, or -1 if the system may
   not be called.  */
extern int ccheck_time P((int bgrade, const char *ztime));

/* Get the lowest grade of work we are permitted to do at the current
   time according to a time string.  Returns a null byte if no grades
   are legal.  */
extern char btime_low_grade P((const char *ztime));     

/* Check whether the current time matches a time string.  This
   routine actually doesn't use the current time, but computes
   a time when it is first called and uses it thereafter.  */
extern boolean ftime_now P((const char *ztime));

/* Add a new timetable.  This takes control over both its arguments;
   they must not be freed up or on the stack.  */
extern void uaddtimetable P((const char *zname, const char *ztime));

/* Check login name and password.  */
extern boolean fcheck_login P((const char *zuser, const char *zpass));

/* Get one character from the remote system, going through the
   procotol buffering.  The ctimeout argument is the timeout in
   seconds, and the freport argument is TRUE if errors should be
   reported (when closing a connection it is pointless to report
   errors).  This returns a character or -1 on a timeout or -2 on an
   error.  */
extern int breceive_char P((int ctimeout, boolean freport));

/* See whether a file belongs in the spool directory.  */
extern boolean fspool_file P((const char *zfile));

/* Store information about a file being sent.  */
extern boolean fstore_sendfile P((openfile_t e, pointer pseq,
				  const char *zfrom, const char *zto,
				  const char *ztosys, const char *zuser,
				  const char *zmail));

/* Finish sending a file.  */
extern boolean fsent_file P((boolean freceived, long cbytes));

/* Note an error sending a file.  Do not call fsent_file after this.  */
extern void usendfile_error P((void));

/* Store information about a file being received.  */
extern boolean fstore_recfile P((openfile_t e, pointer pseq,
				 const char *zfrom, const char *zto,
				 const char *zfromsys, const char *zuser,
				 unsigned int imode, const char *zmail,
				 const char *ztemp));

/* Finish receiving a file.  */
extern boolean freceived_file P((boolean fsent, long cbytes));

/* Note an error receiving a file.  The function freceived_file must
   still be called after this is called.  */
extern void urecfile_error P((void));

/* Prepare to receive a file again by discarding the previous
   contents.  */
extern boolean frecfile_rewind P((void));

/* See whether a file is one of a list of directories.  The qsys and
   zsystem arguments are passed down to allow ~ expansion.  */
extern boolean fin_directory_list P((const struct ssysteminfo *qsys,
				     const char *zfile,
				     const char *zdirs));

/* Get the login name and password to use when calling a system out
   of the call out login file.  The pzlog and pzpass arguments are set
   to point to malloc'ed strings which must be freed after they have
   been used.  */
extern boolean fcallout_login P((const struct ssysteminfo *qsys,
				 char **pzlog, char **pzpass));

/* Add a string to the end of another.  */
extern void uadd_string P((char **pz, const char *z, int bsep));

/* Add a protocol parameter entry.  */
extern enum tcmdtabret tadd_proto_param P((int *pc,
					   struct sproto_param **pq,
					   const char *zerr, int cargs,
					   char **azargs));

/* Apply protocol parameters.  */
extern void uapply_proto_params P((int bproto, struct scmdtab *qcmds,
				   int c, struct sproto_param *pas));

/* Parse a command string.  */
extern boolean fparse_cmd P((char *zcmd, struct scmd *qcmd));

/* Specify which machines may be accepted for a login name.  */
extern void uadd_validate P((const char *zlogname, int cmachines,
			     const char **pazmachines));

/* Check whether a login name/machine name pair is acceptable.  */
extern boolean fcheck_validate P((const char *zlogname,
				  const char *zmachine));

/* Compare the execution times of two grades.  Return < 0 if the first
   argument should be executed first, 0 if they are the same, > 0 if
   the second argument should be executed first.  */
extern int igradecmp P((int b1, int b2));

/* Make log entry.  */
extern void ulog P((enum tlog ttype, const char *zfmt, ...));

/* Set the program reported by the logging functions.  */
extern void ulog_program P((const char *zprogram));

/* Set the ID number used by the logging functions.  */
extern void ulog_id P((int iid));

/* Set the system name used by the logging functions.  */
extern void ulog_system P((const char *zsystem));

/* Set the system and user name used by the logging functions.  */
extern void ulog_user P((const char *zuser));

/* Close the log file.  */
extern void ulog_close P((void));

/* Close the statistics file.  */
extern void ustats_close P((void));

/* Return the date and time in the form used for a log entry (this
   returns a pointer to a static buffer).  */
extern const char *zdate_and_time P((void));

/* Copy one file to another.  */
extern boolean fcopy_file P((const char *zfrom, const char *zto,
			     boolean fpublic));

/* Read a line from a set of files opened by qmulti_open.  The return
   value is an malloc'ed buffer.  This will return NULL when all the
   files have been exhausted.  If pffirst is not NULL, it will be set
   to TRUE if this is the first line of a file.  If pzname is not
   NULL, it will be set to the file name from which the line was read.
   If fbackslash is TRUE, lines may be continued by using a backslash
   as the last character before the newline.  */
extern char *zmulti_gets P((struct smulti_file *q, boolean *pffirst,
			    const char **pzname, boolean fbackslash));

/* Read an arbitrary length string from a file, returning an malloc'ed
   buffer.  If the fbackslash argument is true, lines may be continued
   by using a backslash as the last character before the newline.  */
extern char *zfgets P((FILE *e, boolean fbackslash));

/* Compare strings case insensitively.  */
extern int strcasecmp P((const char *z1, const char *z2));

/* Compare strings case insensitively up to a point.  */
extern int strncasecmp P((const char *z1, const char *z2, int c));

/* Copy a string into memory without fail.  */
extern char *xstrdup P((const char *));

/* Allocate memory without fail.  */
extern pointer xmalloc P((int));

/* Realloc memory without fail.  */
extern pointer xrealloc P((pointer, int));     

/* Free memory (accepts NULL pointers, which some libraries erroneously
   do not).  */
extern void xfree P((pointer));

#if ! HAVE_REMOVE
/* Erase a file.  */
extern int remove P((const char *zfile));
#endif

#if ! HAVE_RAISE
/* Raise a signal.  */
extern int raise P((int isig));
#endif

#if ! HAVE_STRDUP
/* Copy a string into memory.  */
extern char *strdup P((const char *z));
#endif

#if ! HAVE_STRSTR
/* Look for one string within another.  */
extern char *strstr P((const char *zouter, const char *zinner));
#endif

#if HAVE_STRICMP
/* Use macros to access stricmp and strnicmp as strcasecmp and
   strncasecmp.  */
#define strcasecmp stricmp
#define strncasecmp strnicmp
#endif

#if ! HAVE_STRCASECMP && ! HAVE_STRICMP
/* Use our own case insensitive string comparisons.  */
extern int strcasecmp P((const char *z1, const char *z2));
extern int strncasecmp P((const char *z1, const char *z2, int clen));
#endif

#if ! HAVE_STRERROR
/* Get a string corresponding to an error message.  */
extern char *strerror P((int ierr));
#endif

/* Move a memory block safely.  Go through xmemmove to allow for
   systems which have the prototype (using size_t, which we don't want
   to use since some systems won't have it) but not the function.  */
#if HAVE_MEMMOVE
#define xmemmove memmove
#else /* ! HAVE_MEMMOVE */
extern pointer xmemmove P((pointer, constpointer, int));
#endif /* ! HAVE_MEMMOVE */

/* Global variables.  */

/* Version number string.  */
extern const char abVersion[];

/* Level of debugging.  */
extern int iDebug;

/* Local UUCP name.  */
extern const char *zLocalname;

/* System information file names.  */
extern char *zSysfile;

/* Port information file names.  */
extern char *zPortfile;

/* Dialer information file names.  */
extern char *zDialfile;

/* Local spool directory.  */
extern const char *zSpooldir;

/* Public directory.  */
extern const char *zPubdir;

/* Log file name.  */
extern const char *zLogfile;

/* Statistics file name.  */
extern const char *zStatfile;

/* Debugging file name.  */
extern const char *zDebugfile;

/* Files containing login names and passwords to use when calling out.  */
extern char *zCallfile;

/* Files containing login names and passwords to check when somebody
   calls in.  */
extern char *zPwdfile;

/* Files containing dialcodes.  */
extern char *zDialcodefile;

#if HAVE_V2_CONFIG
/* TRUE if we should read V2 configuration files.  */
extern boolean fV2;

/* The names of the various V2 files.  */
#define ZV2_SYSTEMS "L.sys"
#define ZV2_DEVICES "L-devices"
#define ZV2_USERFILE "USERFILE"
#define ZV2_CMDS "L.cmds"
#define ZV2_DIALCODES "L-dialcodes"

/* Read the V2 L.sys file.  */
extern void uv2_read_systems P((int *pc, struct ssysteminfo **ppas));

#endif /* HAVE_V2_CONFIG */

#if HAVE_BNU_CONFIG
/* TRUE if we should read BNU configuration files.  */
extern boolean fBnu;

/* The names of the BNU system files to read.  */
extern char *zBnu_systems;

/* The names of the BNU dialers files to read.  */
extern char *zBnu_dialers;

/* The names of the BNU devices files to read.  */
extern char *zBnu_devices;

/* The default names of the various BNU files.  */
#define ZBNU_SYSFILES "Sysfiles"
#define ZBNU_SYSTEMS "Systems"
#define ZBNU_DIALERS "Dialers"
#define ZBNU_DEVICES "Devices"
#define ZBNU_PERMISSIONS "Permissions"
#define ZBNU_DIALCODES "Dialcodes"

/* Routines to read BNU files.  */
extern void ubnu_read_sysfiles P((void));
extern void ubnu_read_systems P((int *pc, struct ssysteminfo **ppas));

#endif /* HAVE_BNU_CONFIG */

/* TRUE if we accept calls from unknown system.  */
extern boolean fUnknown_ok;

/* The ssysteminfo structure we use for unknown systems.  */
extern struct ssysteminfo sUnknown;

/* The ssysteminfo structure we use for the local system.  */
extern struct ssysteminfo sLocalsys;

/* File being sent.  */
extern openfile_t eSendfile;

/* File being received.  */
extern openfile_t eRecfile;

/* TRUE if we are aborting because somebody used LOG_FATAL.  */
extern boolean fAborting;

#endif
