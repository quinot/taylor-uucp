/* uucp.h
   Header file for the UUCP package.

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

/* Define alloca as suggested by David MacKenzie.  AIX requires this
   to be the first thing in the file.  I really hate system dependent
   cruft like this, but I guess that's the price of using alloca.  */
#define HAVE_ALLOCA 1
#ifdef __GNUC__
#ifndef __NeXT__
#define alloca __builtin_alloca
#endif /* ! defined (__NeXT__) */
#else /* ! defined(__GNUC__) */
#ifdef sparc
#include <alloca.h>
#else /* ! defined (sparc) */
#ifdef _AIX
 #pragma alloca
#else /* ! defined (_AIX) */
/* We may not be using a real alloca.  */
#undef HAVE_ALLOCA
#define HAVE_ALLOCA 0
#endif /* ! defined (_AIX) */
#endif /* ! defined (sparc) */
#endif /* ! defined (__GNUC__) */

/* Get the system configuration parameters.  */
#include "conf.h"
#include "policy.h"

/* Get a definition for ANSI_C if we weren't given one.  */
#ifndef ANSI_C
#ifdef __STDC__
#define ANSI_C 1
#else /* ! defined (__STDC__) */
#define ANSI_C 0
#endif /* ! defined (__STDC__) */
#endif /* ! defined (ANSI_C) */

/* Pass this definition into uuconf.h.  */
#define UUCONF_ANSI_C ANSI_C

/* We always include some standard header files.  We need <signal.h>
   to define sig_atomic_t.  */
#if HAVE_STDDEF_H
#include <stddef.h>
#endif
#include <stdio.h>
#include <signal.h>

/* On some systems we need <sys/types.h> to get sig_atomic_t or
   size_t or time_t.  */
#if ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && HAVE_SIG_ATOMIC_T_IN_TYPES_H
#define USE_TYPES_H 1
#else
#if ! HAVE_SIZE_T_IN_STDDEF_H && HAVE_SIZE_T_IN_TYPES_H
#define USE_TYPES_H 1
#else
#if ! HAVE_TIME_T_IN_TIME_H && HAVE_TIME_T_IN_TYPES_H
#define USE_TYPES_H 1
#endif
#endif
#endif

#ifndef USE_TYPES_H
#define USE_TYPES_H 0
#endif

#if USE_TYPES_H
#include <sys/types.h>
#endif

/* Make sure we have sig_atomic_t.  */
#if ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && ! HAVE_SIG_ATOMIC_T_IN_TYPES_H
#ifndef SIG_ATOMIC_T
/* There is no portable definition for sig_atomic_t.  */
#define SIG_ATOMIC_T char
#endif /* ! defined (SIG_ATOMIC_T) */
typedef SIG_ATOMIC_T sig_atomic_t;
#endif /* ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && ! HAVE_SIG_ATOMIC_T_IN_TYPES_H */

/* Make sure we have size_t.  We use int as the default because the
   main use of this type is to provide an argument to malloc and
   realloc.  On a system which does not define size_t, int is
   certainly the correct type to use.  */
#if ! HAVE_SIZE_T_IN_STDDEF_H && ! HAVE_SIZE_T_IN_TYPES_H
#ifndef SIZE_T
#define SIZE_T int
#endif /* ! defined (SIZE_T) */
typedef SIZE_T size_t;
#endif /* ! HAVE_SIZE_T_IN_STDDEF_H && ! HAVE_SIZE_T_IN_TYPES_H */

/* Make sure we have time_t.  We use long as the default.  We don't
   bother to let conf.h override this, since on a system which doesn't
   define time_t long must be correct.  */
#if ! HAVE_TIME_T_IN_TIME_H && ! HAVE_TIME_T_IN_TYPES_H
typedef long time_t;
#endif

/* Set up some definitions for both ANSI C and Classic C.

   P() -- for function prototypes (e.g. extern int foo P((int)) ).
   pointer -- for a generic pointer (i.e. void *).
   constpointer -- for a generic pointer to constant data.
   BUCHAR -- to convert a character to unsigned.  */
#if ANSI_C
#if ! HAVE_VOID || ! HAVE_UNSIGNED_CHAR
 #error ANSI C compiler without void or unsigned char
#endif
#define P(x) x
typedef void *pointer;
typedef const void *constpointer;
#define BUCHAR(b) ((unsigned char) (b))
#else /* ! ANSI_C */
/* Handle uses of const, volatile and void in Classic C.  */
#define const
#define volatile
#if ! HAVE_VOID
#define void int
#endif
#define P(x) ()
typedef char *pointer;
typedef const char *constpointer;
#if HAVE_UNSIGNED_CHAR
#define BUCHAR(b) ((unsigned char) (b))
#else /* ! HAVE_UNSIGNED_CHAR */
/* This should work on most systems, but not necessarily all.  */
#define BUCHAR(b) ((b) & 0xff)
#endif /* ! HAVE_UNSIGNED_CHAR */
#endif /* ! ANSI_C */

/* Now that we have pointer, define alloca.  */
#if ! HAVE_ALLOCA
extern pointer alloca ();
#endif

/* Make sure we have a definition for offsetof.  */
#ifndef offsetof
#define offsetof(type, field) \
  ((size_t) ((char *) &(((type *) 0)->field) - (char *) (type *) 0))
#endif

/* Only use inline with gcc.  */
#ifndef __GNUC__
#define __inline__
#endif

/* Get the string functions, which are used throughout the code.  */
#if HAVE_MEMORY_H
#include <memory.h>
#else
/* We really need a definition for memchr, and this should not
   conflict with anything in <string.h>.  I hope.  */
extern pointer memchr ();
#endif

#if HAVE_STRING_H
#include <string.h>
#else /* ! HAVE_STRING_H */
#if HAVE_STRINGS_H
#include <strings.h>
#else /* ! HAVE_STRINGS_H */
extern char *strcpy (), *strncpy (), *strchr (), *strrchr (), *strtok ();
extern char *strcat (), *strerror (), *strstr ();
extern size_t strlen (), strspn (), strcspn ();
#if ! HAVE_MEMORY_H
extern pointer memcpy (), memmove (), memchr ();
#endif /* ! HAVE_MEMORY_H */
#endif /* ! HAVE_STRINGS_H */
#endif /* ! HAVE_STRING_H */

/* Get what we need from <stdlib.h>.  */
#if HAVE_STDLIB_H
#include <stdlib.h>
#else /* ! HAVE_STDLIB_H */
extern pointer malloc (), realloc (), bsearch ();
extern long strtol ();
extern char *getenv ();
#endif /* ! HAVE_STDLIB_H */

/* NeXT uses <libc.h> to declare a bunch of functions.  */
#if HAVE_LIBC_H
#include <libc.h>
#endif

/* Make sure we have the EXIT_ macros.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (1)
#endif

/* If we need to declare errno, do so.  I don't want to always do
   this, because some system might theoretically have a different
   declaration for errno.  On a POSIX system this is sure to work.  */
#if ! HAVE_ERRNO_DECLARATION
extern int errno;
#endif

/* If the system has the socket call, guess that we can compile the
   TCP code.  */
#define HAVE_TCP HAVE_SOCKET

/* If the system has the t_open call, guess that we can compile the
   TLI code.  */
#define HAVE_TLI HAVE_T_OPEN

/* The boolean type holds boolean values.  */
typedef int boolean;
#undef TRUE
#undef FALSE
#define TRUE (1)
#define FALSE (0)

/* The prototypes below need the uuconf structures.  */
#include "uuconf.h"

/* The openfile_t type holds an open file.  This depends on whether we
   are using stdio or not.  */
#if USE_STDIO

typedef FILE *openfile_t;
#define EFILECLOSED ((FILE *) NULL)
#define ffileisopen(e) ((e) != NULL)
#define ffileeof(e) feof (e)
#define cfileread(e, z, c) fread ((z), 1, (c), (e))
#define ffilereaderror(e, c) ferror (e)
#define cfilewrite(e, z, c) fwrite ((z), 1, (c), (e))
#ifdef SEEK_SET
#define ffileseek(e, i) (fseek ((e), (long) (i), SEEK_SET) == 0)
#define ffilerewind(e) (fseek ((e), (long) 0, SEEK_SET) == 0)
#else
#define ffileseek(e, i) (fseek ((e), (long) (i), 0) == 0)
#define ffilerewind(e) (fseek ((e), (long) 0, 0) == 0)
#endif
#define ffileclose(e) (fclose (e) == 0)

#else /* ! USE_STDIO */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

typedef int openfile_t;
#define EFILECLOSED (-1)
#define ffileisopen(e) ((e) >= 0)
#define ffileeof(e) (FALSE)
#define cfileread(e, z, c) read ((e), (z), (c))
#define ffilereaderror(e, c) ((c) < 0)
#define cfilewrite(e, z, c) write ((e), (z), (c))
#ifdef SEEK_SET
#define ffileseek(e) (lseek ((e), (long) 0, SEEK_SET) >= 0)
#define ffilerewind(e) (lseek ((e), (long) 0, SEEK_SET) >= 0)
#else
#define ffileseek(e) (lseek ((e), (long) 0, 0) >= 0)
#define ffilerewind(e) (lseek ((e), (long) 0, 0) >= 0)
#endif
#define ffileclose(e) (close (e) >= 0)

#endif /* ! USE_STDIO */

/* This structure represents a connection.  The connection routines
   are described in conn.h, but some functions need this structure
   here so that they can refer to it in prototypes.  */

struct sconnection
{
  /* Pointer to command table for this type of connection.  */
  const struct sconncmds *qcmds;
  /* Pointer to system dependent information.  */
  pointer psysdep;
  /* Pointer to system independent information.  */
  struct uuconf_port *qport;
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
  LOG_FATAL
#if DEBUG > 1
    ,
  /* Debugging log entry.  */
  LOG_DEBUG,
  /* Start debugging log entry.  */
  LOG_DEBUG_START,
  /* Continue debugging log entry.  */
  LOG_DEBUG_CONTINUE,
  /* End debugging log entry.  */
  LOG_DEBUG_END
#endif
};

/* The tstatus_type enumeration holds the kinds of status information
   we put in the status file.  The order of entries here corresponds
   to the order of entries in the azStatus array.  */

enum tstatus_type
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
  STATUS_TALKING,
  /* Wrong time to call.  */
  STATUS_WRONG_TIME,
  /* Number of status values.  */
  STATUS_VALUES
};

/* An array to convert status entries to strings.  If more status entries
   are added, this array must be extended.  */
extern const char *azStatus[];

/* The sstatus structure holds the contents of a system status file.  */

struct sstatus
{
  /* Current status of conversation.  */
  enum tstatus_type ttype;
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
  /* Command ('S' for send, 'R' for receive, 'X' for execute, 'E' for
     simple execution, 'H' for hangup, 'Y' for hangup confirm, 'N' for
     hangup deny).  */
  char bcmd;
  /* At least one compiler needs an explicit padding byte here.  */
  char bdummy;
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
  /* Temporary file name ('S' and 'E').  */
  const char *ztemp;
  /* Mode to give newly created file ('S' and 'E').  */
  unsigned int imode;
  /* User to notify on remote system (optional; 'S' and 'E').  */
  const char *znotify;
  /* File size (-1 if not supplied) ('S', 'E' and 'R').  */
  long cbytes;
  /* Command to execute ('E').  */
  const char *zcmd;
};

#if DEBUG > 1

/* We allow independent control over several different types of
   debugging output, using a bit string with individual bits dedicated
   to particular debugging types.  */

/* The bit string is stored in iDebug.  */
extern int iDebug;

/* Debug abnormal events.  */
#define DEBUG_ABNORMAL (01)
/* Debug chat scripts.  */
#define DEBUG_CHAT (02)
/* Debug initial handshake.  */
#define DEBUG_HANDSHAKE (04)
/* Debug UUCP protocol.  */
#define DEBUG_UUCP_PROTO (010)
/* Debug protocols.  */
#define DEBUG_PROTO (020)
/* Debug port actions.  */
#define DEBUG_PORT (040)
/* Debug configuration files.  */
#define DEBUG_CONFIG (0100)
/* Debug spool directory actions.  */
#define DEBUG_SPOOLDIR (0200)
/* Debug executions.  */
#define DEBUG_EXECUTE (0400)
/* Debug incoming data.  */
#define DEBUG_INCOMING (01000)
/* Debug outgoing data.  */
#define DEBUG_OUTGOING (02000)

/* Maximum possible value for iDebug.  */
#define DEBUG_MAX (03777)

/* Intializer for array of debug names.  The index of the name in the
   array is the corresponding bit position in iDebug.  We only check
   for prefixes, so these names only need to be long enough to
   distinguish each name from every other.  The last entry must be
   NULL.  The string "all" is also recognized to turn on all
   debugging.  */
#define DEBUG_NAMES \
  { "a", "ch", "h", "u", "pr", "po", "co", "s", "e", "i", "o", NULL }

/* The prefix to use to turn off all debugging.  */
#define DEBUG_NONE "n"

/* Check whether a particular type of debugging is being done.  */
#define FDEBUGGING(i) ((iDebug & (i)) != 0)

/* These macros are used to output debugging information.  I use
   several different macros depending on the number of arguments
   because no macro can take a variable number of arguments and I
   don't want to use double parentheses.  */
#define DEBUG_MESSAGE0(i, z) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z)); } while (0)
#define DEBUG_MESSAGE1(i, z, a1) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z), (a1)); } while (0)
#define DEBUG_MESSAGE2(i, z, a1, a2) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z), (a1), (a2)); } while (0)
#define DEBUG_MESSAGE3(i, z, a1, a2, a3) \
  do \
    { \
      if (FDEBUGGING (i)) \
	ulog (LOG_DEBUG, (z), (a1), (a2), (a3)); \
    } \
  while (0)
#define DEBUG_MESSAGE4(i, z, a1, a2, a3, a4) \
  do \
    { \
      if (FDEBUGGING (i)) \
	ulog (LOG_DEBUG, (z), (a1), (a2), (a3), (a4)); \
    } \
  while (0)

#else /* DEBUG <= 1 */

/* If debugging information is not being compiled, provide versions of
   the debugging macros which just disappear.  */
#define DEBUG_MESSAGE0(i, z)
#define DEBUG_MESSAGE1(i, z, a1)
#define DEBUG_MESSAGE2(i, z, a1, a2)
#define DEBUG_MESSAGE3(i, z, a1, a2, a3)
#define DEBUG_MESSAGE4(i, z, a1, a2, a3, a4)

#endif /* DEBUG <= 1 */

/* Functions.  */

/* Given an unknown system name, return information for an unknown
   system.  If unknown systems are not permitted, this returns FALSE.
   Otherwise, it translates the name as necessary for the spool
   directory, and fills in *qsys.  */
extern boolean funknown_system P((pointer puuconf, const char *zsystem,
				  struct uuconf_system *qsys));

/* See whether a file belongs in the spool directory.  */
extern boolean fspool_file P((const char *zfile));

/* See if the current time matches a time span.  If not, return FALSE.
   Otherwise, return TRUE and set *pival and *pcretry to the values
   from the matching element of the span.  */
extern boolean ftimespan_match P((const struct uuconf_timespan *qspan,
				  long *pival, int *pcretry));

/* Determine the maximum size that may ever be transferred, given a
   timesize span.  If there are any time gaps larger than 1 hour not
   described by the timesize span, this returns -1.  Otherwise it
   returns the largest size that may be transferred at some time.  */
extern long cmax_size_ever P((const struct uuconf_timespan *qtimesize));

/* Send mail about a file transfer.  */
extern boolean fmail_transfer P((boolean fok, const char *zuser,
				 const char *zmail, const char *zwhy,
				 const char *zfrom, const char *zfromsys,
				 const char *zto, const char *ztosys,
				 const char *zsaved));

/* See whether a file is in one of a list of directories.  The zpubdir
   argument is used to pass the directory names to zsysdep_local_file.
   If fcheck is FALSE, this does not check accessibility.  Otherwise,
   if freadable is TRUE, the user zuser must have read access to the
   file and all appropriate directories; if freadable is FALSE zuser
   must have write access to the appropriate directories.  The zuser
   argument may be NULL, in which case all users must have the
   appropriate access (this is used for a remote request).  */
extern boolean fin_directory_list P((const char *zfile,
				     char **pzdirs,
				     const char *zpubdir,
				     boolean fcheck,
				     boolean freadable,
				     const char *zuser));

/* Parse a command string.  */
extern boolean fparse_cmd P((char *zcmd, struct scmd *qcmd));

/* Make a log entry.  */
#ifdef __GNUC__
#define GNUC_VERSION __GNUC__
#else
#define GNUC_VERSION 0
#endif

#if ANSI_C && HAVE_VFPRINTF
extern void ulog P((enum tlog ttype, const char *zfmt, ...))
#if GNUC_VERSION > 1
     __attribute__ ((format (printf, 2, 3)))
#endif
     ;
#else
extern void ulog ();
#endif

#undef GNUC_VERSION

/* Report an error returned by one of the uuconf routines.  */
extern void ulog_uuconf P((enum tlog ttype, pointer puuconf,
			   int iuuconf));

/* Set the function to call if a fatal error occurs.  */
extern void ulog_fatal_fn P((void (*pfn) P((void))));

/* If ffile is TRUE, send log entries to the log file rather than to
   stderr.  */
extern void ulog_to_file P((pointer puuconf, boolean ffile));

/* Set the ID number used by the logging functions.  */
extern void ulog_id P((int iid));

/* Set the system name used by the logging functions.  */
extern void ulog_system P((const char *zsystem));

/* Set the system and user name used by the logging functions.  */
extern void ulog_user P((const char *zuser));

/* Set the device name used by the logging functions.  */
extern void ulog_device P((const char *zdevice));

/* Close the log file.  */
extern void ulog_close P((void));

/* Make an entry in the statistics file.  */
extern void ustats P((boolean fsucceeded, const char *zuser,
		      const char *zsystem, boolean fsent,
		      long cbytes, long csecs, long cmicros));

/* We have lost the connection; record any in progress file transfers
   in the statistics file.  */
extern void ustats_failed P((const struct uuconf_system *qsys));

/* Close the statistics file.  */
extern void ustats_close P((void));

#if DEBUG > 1
/* A debugging routine to output a buffer.  This outputs zhdr, the
   buffer length clen, and the contents of the buffer in quotation
   marks.  */
extern void udebug_buffer P((const char *zhdr, const char *zbuf,
			     size_t clen));

/* A debugging routine to make a readable version of a character.
   This takes a buffer at least 5 bytes long, and returns the length
   of the string it put into it (not counting the null byte).  */
extern size_t cdebug_char P((char *z, int ichar));

/* Parse a debugging option string.  This can either be a number or a
   comma separated list of debugging names.  If the code is compiled
   without debugging this is a dummy function.  This returns a value
   for iDebug.  */
extern int idebug_parse P((const char *));

/* Parse a debugging option in a configuration file.  This is used for
   the ``debug'' command.  */
extern enum tcmdtabret tidebug_parse P((int argc, char **argv,
					pointer pvar, const char *zerr));

#endif /* DEBUG <= 1 */

/* Copy one file to another.  */
extern boolean fcopy_file P((const char *zfrom, const char *zto,
			     boolean fpublic, boolean fmkdirs));

/* Translate escape sequences in a buffer, leaving the result in the
   same buffer and returning the length.  */
extern size_t cescape P((char *zbuf));

/* Get a buffer to hold a string of a given size.  The buffer should
   be freed with ubuffree.  */
extern char *zbufalc P((size_t csize));

/* Call zbufalc to allocate a buffer and copy a string into it.  */
extern char *zbufcpy P((const char *z));

/* Free up a buffer returned by zbufalc or zbufcpy.  */
extern void ubuffree P((char *z));

/* Allocate memory without fail.  */
extern pointer xmalloc P((size_t));

/* Realloc memory without fail.  */
extern pointer xrealloc P((pointer, size_t));

/* Free memory (accepts NULL pointers, which some libraries erroneously
   do not).  */
extern void xfree P((pointer));

/* Some standard routines which we only define if they are not present
   on the system we are compiling on.  */

#if ! HAVE_GETLINE
/* Read a line from a file.  */
extern int getline P((char **pz, size_t *pc, FILE *e));
#endif

#if ! HAVE_REMOVE
/* Erase a file.  */
#undef remove
extern int remove P((const char *zfile));
#endif

#if ! HAVE_STRDUP
/* Copy a string into memory.  */
extern char *strdup P((const char *z));
#endif

#if ! HAVE_STRSTR
/* Look for one string within another.  */
extern char *strstr P((const char *zouter, const char *zinner));
#endif

#if ! HAVE_STRCASECMP
#if HAVE_STRICMP
#define strcasecmp stricmp
#else /* ! HAVE_STRICMP */
extern int strcasecmp P((const char *z1, const char *z2));
#endif /* ! HAVE_STRICMP */
#endif /* ! HAVE_STRCASECMP */

#if ! HAVE_STRNCASECMP
#if HAVE_STRNICMP
#define strncasecmp strnicmp
#else /* ! HAVE_STRNICMP */
extern int strncasecmp P((const char *z1, const char *z2, size_t clen));
#endif /* ! HAVE_STRNICMP */
#endif /* ! HAVE_STRNCASECMP */

#if ! HAVE_STRERROR
/* Get a string corresponding to an error message.  */
extern char *strerror P((int ierr));
#endif

/* Get the appropriate definitions for memcmp, memcpy, memchr and
   bzero.  */
#if ! HAVE_MEMCMP
#if HAVE_BCMP
#define memcmp(p1, p2, c) bcmp ((p1), (p2), (c))
#else /* ! HAVE_BCMP */
extern int memcmp P((constpointer p1, constpointer p2, size_t c));
#endif /* ! HAVE_BCMP */
#endif /* ! HAVE_MEMCMP */

#if ! HAVE_MEMCPY
#if HAVE_BCOPY
#define memcpy(pto, pfrom, c) bcopy ((pfrom), (pto), (c))
#else /* ! HAVE_BCOPY */
extern pointer memcpy P((pointer pto, constpointer pfrom, size_t c));
#endif /* ! HAVE_BCOPY */
#endif /* ! HAVE_MEMCPY */

#if ! HAVE_MEMCHR
extern pointer memchr P((constpointer p, int b, size_t c));
#endif

#if HAVE_BZERO
#else /* ! HAVE_BZERO */
#if HAVE_MEMSET
#define bzero(p, c) memset ((p), 0, (c))
#else /* ! HAVE_MEMSET */
extern void bzero P((pointer p, int c));
#endif /* ! HAVE_MEMSET */
#endif /* ! HAVE_BZERO */

/* Move a memory block safely.  Go through xmemmove to allow for
   systems which have the prototype (using size_t, which we don't want
   to use since some systems won't have it) but not the function.  */
#if HAVE_MEMMOVE
#define xmemmove memmove
#else /* ! HAVE_MEMMOVE */
extern pointer xmemmove P((pointer, constpointer, int));
#endif /* ! HAVE_MEMMOVE */

/* Look up a character in a string.  */
#if ! HAVE_STRCHR
#if HAVE_INDEX
#define strchr index
extern char *index ();
#else /* ! HAVE_INDEX */
extern char *strchr P((const char *z, int b));
#endif /* ! HAVE_INDEX */
#endif /* ! HAVE_STRCHR */

#if ! HAVE_STRRCHR
#if HAVE_RINDEX
#define strrchr rindex
extern char *rindex ();
#else /* ! HAVE_RINDEX */
extern char *strrchr P((const char *z, int b));
#endif /* ! HAVE_RINDEX */
#endif /* ! HAVE_STRRCHR */

/* Turn a string into a long integer.  */
#if ! HAVE_STRTOL
extern long strtol P((const char *, char **, int));
#endif

/* Lookup a key in a sorted array.  */
#if ! HAVE_BSEARCH
extern pointer bsearch P((constpointer pkey, constpointer parray,
			  size_t celes, size_t cbytes,
			  int (*pficmp) P((constpointer, constpointer))));
#endif

/* Convert a string to lower case.  */
#if ! HAVE_STRLWR
extern char *strlwr P((char *));
#endif

/* Global variables.  */

/* The name of the program being run.  This is statically initialized,
   although it should perhaps be set from argv[0].  */
extern char abProgram[];

/* When a signal occurs, the signal handlers sets the appropriate
   element of the arrays afSignal and afLog_signal to TRUE.  The
   afSignal array is used to check whether a signal occurred.  The
   afLog_signal array tells ulog to log the signal; ulog will clear
   the element after logging it, which means that if a signal comes in
   at just the right moment it will not be logged.  It will always be
   recorded in afSignal, though.  At the moment we handle 5 signals:
   SIGHUP, SIGINT, SIGQUIT, SIGTERM and SIGPIPE (the Unix code also
   handles SIGALRM).  If we want to handle more, the afSignal array
   must be extended; I see little point to handling any of the other
   ANSI C or POSIX signals, as they are either unlikely to occur
   (SIGABRT, SIGUSR1) or nearly impossible to handle cleanly (SIGILL,
   SIGSEGV).  SIGHUP is only logged if fLog_sighup is TRUE.  */
#define INDEXSIG_SIGHUP (0)
#define INDEXSIG_SIGINT (1)
#define INDEXSIG_SIGQUIT (2)
#define INDEXSIG_SIGTERM (3)
#define INDEXSIG_SIGPIPE (4)
#define INDEXSIG_COUNT (5)

extern volatile sig_atomic_t afSignal[INDEXSIG_COUNT];
extern volatile sig_atomic_t afLog_signal[INDEXSIG_COUNT];
extern boolean fLog_sighup;

/* The names of the signals to use in error messages, as an
   initializer for an array.  */
#define INDEXSIG_NAMES \
  { "hangup", "interrupt", "quit", "termination", "SIGPIPE" }

/* Check to see whether we've received a signal.  It would be nice if
   we could use a single variable for this, but we sometimes want to
   clear our knowledge of a signal and that would cause race
   conditions (clearing a single element of the array is not a race
   assuming that we don't care about a particular signal, even if it
   occurs after we've examined the array).  */
#define FGOT_SIGNAL() \
  (afSignal[INDEXSIG_SIGHUP] || afSignal[INDEXSIG_SIGINT] \
   || afSignal[INDEXSIG_SIGQUIT] || afSignal[INDEXSIG_SIGTERM] \
   || afSignal[INDEXSIG_SIGPIPE])

/* If we get a SIGINT in uucico, we continue the current communication
   session but don't start any new ones.  This macros checks for any
   signal other than SIGINT, which means we should get out
   immediately.  */
#define FGOT_QUIT_SIGNAL() \
  (afSignal[INDEXSIG_SIGHUP] || afSignal[INDEXSIG_SIGQUIT] \
   || afSignal[INDEXSIG_SIGTERM] || afSignal[INDEXSIG_SIGPIPE])

/* File being sent.  */
extern openfile_t eSendfile;

/* File being received.  */
extern openfile_t eRecfile;

/* Device name to log.  This is set by fconn_open.  It may be NULL.  */
extern char *zLdevice;

/* If not NULL, ulog calls this function before outputting anything.
   This is used to support cu.  */
extern void (*pfLstart) P((void));

/* If not NULL, ulog calls this function after outputting everything.
   This is used to support cu.  */
extern void (*pfLend) P((void));
