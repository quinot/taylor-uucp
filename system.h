/* system.h
   Header file for system dependent stuff in the Taylor UUCP package.
   This file is not itself system dependent.

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
   Revision 1.16  1992/01/21  19:39:12  ian
   Chip Salzenberg: uucp and uux start uucico for right system, not any

   Revision 1.15  1992/01/04  21:43:24  ian
   Chip Salzenberg: added ALLOW_FILENAME_ARGUMENTS to permit them

   Revision 1.14  1992/01/04  04:12:54  ian
   David J. Fiander: make sure execution arguments are not bad file names

   Revision 1.13  1991/12/29  02:59:50  ian
   Lele Gaifax: put full year in log file

   Revision 1.12  1991/12/17  07:09:58  ian
   Record statistics in fractions of a second

   Revision 1.11  1991/12/14  16:09:07  ian
   Added -l option to uux to link files into the spool directory

   Revision 1.10  1991/12/11  03:59:19  ian
   Create directories when necessary; don't just assume they exist

   Revision 1.9  1991/11/14  03:40:10  ian
   Try to figure out whether stdin is a TCP port

   Revision 1.8  1991/11/13  23:08:40  ian
   Expand remote pathnames in uucp and uux; fix up uux special cases

   Revision 1.7  1991/11/11  18:55:52  ian
   Get protocol parameters from port and dialer for incoming calls

   Revision 1.6  1991/11/10  19:24:22  ian
   Added pffile protocol entry point for file level control

   Revision 1.5  1991/11/07  20:52:33  ian
   Chip Salzenberg: pass command as single argument to /bin/sh

   Revision 1.4  1991/09/19  03:23:34  ian
   Chip Salzenberg: append to private debugging file, don't overwrite it

   Revision 1.3  1991/09/19  03:06:04  ian
   Chip Salzenberg: put BNU temporary files in system's directory

   Revision 1.2  1991/09/11  02:33:14  ian
   Added ffork argument to fsysdep_run

   Revision 1.1  1991/09/10  19:47:55  ian
   Initial revision
 
   */

#ifndef SYSTEM_H

#define SYSTEM_H

#ifdef __GNUC__
 #pragma once
#endif

/* Any function which returns an error should also report an error
   message.

   Many of the function may share a common static buffer; this is
   noted in the description of the function.  */

/* The maximum length of a remote system name.  */
extern int cSysdep_max_name_len;

/* Initialize.  If something goes wrong, this routine should just
   exit.  The argument is true if called from uucico or uuxqt.  */
extern void usysdep_initialize P((boolean fdaemon));

/* Exit the program.  The fsuccess argument indicates whether to
   return an indication of success or failure to the outer
   environment.  This routine should not return.  */
extern void usysdep_exit P((boolean fsuccess));

/* Called when a non-standard configuration file is being used, to
   avoid handing out privileged access.  If it returns FALSE, the
   default configuration file will be used.  This is called before
   the usysdep_initialize function is called.  */
extern boolean fsysdep_other_config P((const char *));

/* Get the local node name if it is not specified in the configuration
   file.  This is called before the usysdep_initialize function is
   called.  It should return NULL on error.  The return value should
   point to a static buffer.  */
extern const char *zsysdep_local_name P((void));

/* Get the login name.  This is used when uucico is started up with no
   arguments in slave mode, which causes it to assume that somebody
   has logged in.  It also used by uucp and uux for recording the user
   name.  It should return NULL on error.  The return value should
   point to a static buffer.  */
extern const char *zsysdep_login_name P((void));

/* Link two files.  On Unix this should attempt the link.  If it
   succeeds it should return TRUE with *pfworked set to TRUE.  If the
   link fails because it must go across a device, it should return
   TRUE with *pfworked set to FALSE.  If the link fails for some other
   reason, it should print an error message and return FALSE.  On a
   system which does not support links to files, this should just
   return TRUE with *pfworked set to FALSE.  */
extern boolean fsysdep_link P((const char *zfrom, const char *zto,
			       boolean *pfworked));

/* Get the port name.  This is used when uucico is started up in slave
   mode to figure out which port was used to call in so that it can
   determine any appropriate protocol parameters.  This may return
   NULL if the port cannot be determined, which will just mean that no
   protocol parameters are applied.  The name returned should be the
   sort of name that would appear in the port file.  This should set
   *pftcp_port to TRUE if it can determine that the port is a TCP
   connection rather than a normal serial port.  */
extern const char *zsysdep_port_name P((boolean *pftcp_port));

/* Make a spool directory for a system.  This will be called each time
   the system might be accessed.  It should return FALSE on error.  */
extern boolean fsysdep_make_spool_dir P((const struct ssysteminfo *qsys));

/* Return whether a file name is in a directory.  The zfile argument
   may itself be a directory; if it is the same as zdir, the function
   should return TRUE.  Note that this deals with file names only; it
   is not checking whether there actually is a file named zfile.  This
   should return TRUE if the file name is in the directory, FALSE
   otherwise.  There is no way to return error.  The qsys argument
   should be used to expand ~ into the public directory.  */
extern boolean fsysdep_in_directory P((const struct ssysteminfo *qsys,
				       const char *zfile,
				       const char *zdir));

/* Return TRUE if a file exists, FALSE otherwise.  There is no way to
   return error.  */
extern boolean fsysdep_file_exists P((const char *zfile));

/* Exit the current program and start a new one.  If the ffork
   argument is TRUE, the new program should be started up and the
   current program should continue (but in all current cases, it will
   immediately exit anyhow); if the ffork argument is FALSE, the new
   program should replace the current program.  The three string
   arguments may be catenated together to form the program to execute;
   I did it this way to make it easy to call execl(2), and because I
   never needed more than two arguments.  The program will always be
   "uucico" or "uuxqt".  The return value will be passed directly to
   usysdep_exit, and should be TRUE on success, FALSE on error.  */
extern boolean fsysdep_run P((boolean ffork, const char *zprogram,
			      const char *zarg1, const char *zarg2));

/* Send a mail message.  This function will be passed an array of
   strings.  All necessary newlines are already included; the strings
   should simply be concatenated together to form the mail message.
   It should return FALSE on error, although the return value is often
   ignored.  */
extern boolean fsysdep_mail P((const char *zto, const char *zsubject,
			       int cstrs, const char **paz));

/* Get the time in seconds since some epoch.  The actual epoch is
   unimportant, so long as the time values are consistent.  */
extern long isysdep_time P((void));

/* Get the time in seconds and microseconds (millionths of a second)
   since some epoch.  If microseconds can not be determined, *pimicros
   can just be set to zero.  */
extern void usysdep_full_time P((long *pisecs, long *pimicros));

/* Get the current time in a struct tm, and also get microseconds.  I
   assume that this structure is defined in <time.h>.  This is
   basically just localtime, except that it also returns the current
   number of microseconds.  It is a system dependent function because
   there is no way in ANSI way to get microseconds, and there's no
   reason to expect that I can pass the seconds returned from
   usysdep_full_time to localtime.  The typedef is a hack to avoid the
   problem of mentioning a structure for the first time in a prototype
   while remaining compatible with standard C.  The type tm_ptr is
   never again referred to.  */
typedef struct tm *tm_ptr;
extern void usysdep_localtime P((tm_ptr q, long *pimicros));

/* Sleep for a number of seconds.  */
extern void usysdep_sleep P((int cseconds));

/* Pause for half a second.  */
extern void usysdep_pause P((void));

/* Lock a remote system.  This should return FALSE if the system is
   already locked (no error should be reported).  */
extern boolean fsysdep_lock_system P((const struct ssysteminfo *qsys));

/* Unlock a remote system.  This should return FALSE on error
   (although the return value is generally ignored).  */
extern boolean fsysdep_unlock_system P((const struct ssysteminfo *qsys));

/* Get the conversation sequence number for a remote system, and
   increment it for next time.  This should return -1 on error.  */
extern long isysdep_get_sequence P((const struct ssysteminfo *qsys));

/* Get the status of a remote system.  This should return FALSE on
   error.  Otherwise it should set *qret to the status.  */
extern boolean fsysdep_get_status P((const struct ssysteminfo *qsys,
				     struct sstatus *qret));

/* Set the status of a remote system.  This should return FALSE on
   error.  The system will be locked before this call is made.  */
extern boolean fsysdep_set_status P((const struct ssysteminfo *qsys,
				     const struct sstatus *qset));

/* Check whether there is work for a remote system.  This should set
   *pbgrade to the highest grade of work waiting to execute.  It
   should return TRUE if there is work, FALSE otherwise; there is no
   way to indicate an error.  */
extern boolean fsysdep_has_work P((const struct ssysteminfo *qsys,
				   char *pbgrade));

/* Initialize the work scan.  This will be called before
   fsysdep_get_work.  The bgrade argument is the minimum grade of
   execution files that should be considered (e.g. a bgrade of 'd'
   will allow all grades from 'A' to 'Z' and 'a' to 'd').  It should
   return FALSE on error.  */
extern boolean fsysdep_get_work_init P((const struct ssysteminfo *qsys,
					int bgrade));

/* Get the next command to be executed for a remote system.  The
   bgrade argument will be the same as for fsysdep_get_work_init;
   probably only one of these functions will use it, namely the
   function for which it is more convenient.  This should return FALSE
   on error.  The structure pointed to by qcmd should be filled in.
   The strings may point into a static buffer; they will be copied out
   if necessary.  If there is no more work, this should set qcmd->bcmd
   to 'H' and return TRUE.  This should set qcmd->pseq to something
   which can be passed to fsysdep_did_work to remove the job from the
   queue when it has been completed.  */
extern boolean fsysdep_get_work P((const struct ssysteminfo *qsys,
				   int bgrade, struct scmd *qcmd));

/* Remove a job from the work queue.  This must also remove the
   temporary file used for a send command, if there is one.  It should
   return FALSE on error.  */
extern boolean fsysdep_did_work P((pointer pseq));

/* Cleanup anything left over by fsysdep_get_work_init and
   fsysdep_get_work.  This may be called even though
   fsysdep_get_work_init has not been.  */
extern void usysdep_get_work_free P((const struct ssysteminfo *qsys));

/* Get the real file name for a file.  The file may or may not exist
   on the system.  If the zname argument is not NULL, then the zfile
   argument may be a directory; if it is, zname should be used for the
   name of the file within the directory.  The zname argument may not
   be simple (it may be in the format of another OS) so care should be
   taken with it.  On Unix, if the zfile argument begins with ~user/
   it goes in that users home directory, and if it begins with ~/
   (~uucp/) it goes in the public directory (note that each system may
   have its own public directory); similar conventions may be
   desirable on other systems.  The return value may point to a common
   static buffer.  This should return NULL on error.  */
extern const char *zsysdep_real_file_name P((const struct ssysteminfo *,
					     const char *zfile,
					     const char *zname));

/* Get a file name from the spool directory.  This should return
   NULL on error.  The return value may point to a common static
   buffer.  */
extern const char *zsysdep_spool_file_name P((const struct ssysteminfo *,
					      const char *zfile));

/* Make necessary directories.  This should create all non-existent
   directories for a file.  If the fpublic argument is TRUE, anybody
   should be permitted to create and remove files in the directory;
   otherwise anybody can list the directory, but only the UUCP system
   can create and remove files.  It should return FALSE on error.  */
extern boolean fsysdep_make_dirs P((const char *zfile, boolean fpublic));

/* Create a stdio file, setting appropriate protection.  If the
   fpublic argument is TRUE, the file is made publically accessible;
   otherwise it is treated as a private data file.  If the fappend
   argument is TRUE, the file is opened in append mode; otherwise any
   previously existing file of the same name is removed, and the file
   is kept private to the UUCP system.  If the fmkdirs argument is
   TRUE, then any necessary directories should also be created.  On a
   system in which file protections are unimportant (and the necessary
   directories exist), this may be implemented as

   fopen (zfile, fappend ? "a" : "w");

   */
extern FILE *esysdep_fopen P((const char *zfile, boolean fpublic,
			      boolean fappend, boolean fmkdirs));

/* Open a file to send to another system; the qsys argument is the
   system the file is being sent to.  This should set *pimode to the
   mode that should be sent over (this should be a UNIX style file
   mode number).  This should set *pcbytes to the number of bytes
   contained in the file.  It should return EFILECLOSED on error.  */
extern openfile_t esysdep_open_send P((const struct ssysteminfo *qsys,
				       const char *zname,
				       unsigned int *pimode,
				       long *pcbytes));

/* Open a file to receive from another system.  Receiving a file is
   done in two steps.  First esysdep_open_receive is called.  This
   should open a temporary file and return the file name in *pztemp.
   It may ignore qsys (the system the file is coming from) and zto
   (the file to be created) although they are passed in case they are
   useful.  The file mode is not available at this point.  The *pztemp
   return value may point to a common static buffer.  The amount of
   free space should be returned in *pcbytes; ideally it should be the
   lesser of the amount of free space on the file system of the
   temporary file and the amount of free space on the file system of
   the final destination.  If the amount of free space is not
   available, *pcbytes should be set to -1.  The function should
   return EFILECLOSED on error.

   After the file is written, fsysdep_move_file will be called to move
   the file to its final destination, and to set the correct file
   mode.  */
extern openfile_t esysdep_open_receive P((const struct ssysteminfo *qsys,
					  const char *zto,
					  const char **pztemp,
					  long *pcbytes));

/* Move a file.  This is used to move a received file to its final
   location.  It is also used by uuxqt to move files in and out of the
   execute directory.  The zto argument is the file to create.  The
   zorig argument is the name of the file to move.  The imode argument
   is the Unix file mode to use for the final file; if it is zero, it
   should be ignored and the file should be kept private to the UUCP
   system.  This should return FALSE on error; the zorig file should
   be removed even if an error occurs.  */
extern boolean fsysdep_move_file P((const char *zorig, const char *zto,
				    unsigned int imode));

/* Truncate a file which we are receiving into.  This may be done by
   closing the original file, removing it and reopening it.  This
   should return FALSE on error.  */
extern openfile_t esysdep_truncate P((openfile_t e, const char *zname));

/* Start expanding a wildcarded file name.  This should return FALSE
   on error; otherwise subsequent calls to zsysdep_wildcard should
   return file names.  The argument may have leading ~ characters.  */
extern boolean fsysdep_wildcard_start P((const struct ssysteminfo *qsys,
					 const char *zfile));

/* Get the next wildcard name.  This should return NULL when there are
   no more names to return.  The return value may point to a common
   static buffer.  The argument should be the same as that to
   fsysdep_wildcard_start.  There is no way to return error.  */
extern const char *zsysdep_wildcard P((const struct ssysteminfo *qsys,
				       const char *zfile));

/* Finish getting wildcard names.  This may be called before or after
   zsysdep_wildcard has returned NULL.  It should return FALSE on
   error.  */
extern boolean fsysdep_wildcard_end P((void));

/* Prepare to execute a bunch of file transfer requests.  This should
   make an entry in the spool directory so that the next time uucico
   is started up it will transfer these files.  The bgrade argument
   specifies the grade of the commands.  The commands themselves are
   in the pascmds array, which has ccmds entries.  The function should
   return FALSE on error.  */
 extern boolean fsysdep_spool_commands P((const struct ssysteminfo *qsys,
					  int bgrade, int ccmds,
					  const struct scmd *pascmds));

/* Get a file name to use for a data file to be copied to another
   system.  A file which will become an execute file will use a grade
   of 'X' (actually this is just convention, but it affects where the
   file will be placed in the spool directory on Unix).  The ztname,
   zdname and zxname arguments will all either be NULL or point to an
   array of CFILE_NAME_LEN characters in length.  The ztname array
   should be set to a temporary file name that could be passed to
   zsysdep_spool_file_name to retrieve the return value of this
   function; this will be appropriate for the temporary name in a send
   request.  The zdname array should be set to a data file name that
   is appropriate for the spool directory of the other system; this
   will be appropriate for the name of the destination file in a send
   request of a data file for an execution of some sort.  The zxname
   array should be set to an execute file name that is appropriate for
   the other system.  This should return NULL on error.  The return
   value may point to a common static buffer.  */

#define CFILE_NAME_LEN (15)

extern const char *zsysdep_data_file_name P((const struct ssysteminfo *qsys,
					     int bgrade, char *ztname,
					     char *zdname, char *zxname));

/* Get a name for a local execute file.  This is used by uux for a
   local command with remote files.  It should return NULL on error.
   The return value may point to a common static buffer.  */
extern const char *zsysdep_xqt_file_name P((void));

/* Beginning getting execute files.  To get a list of execute files,
   first fsysdep_get_xqt_init is called, then zsysdep_get_xqt is
   called several times until it returns NULL, then finally
   usysdep_get_xqt_free is called.  */
extern boolean fsysdep_get_xqt_init P((void));

/* Get the next execute file.  This should return NULL when finished
   (with *pferr set to FALSE).  On an error this should return NULL
   with *pferr set to TRUE.  This should set *pzsystem to the name of
   the system for which the execute file was created.  */
extern const char *zsysdep_get_xqt P((const char **pzsystem,
				      boolean *pferr));

/* Clean up after getting execute files.  */
extern void usysdep_get_xqt_free P((void));

/* Get the absolute pathname of a command to execute.  This is given
   the legal list of commands (which may be the special case "ALL")
   and the path.  It must return an absolute pathname to the command.
   If it gets an error it should set *pferr to TRUE and return NULL;
   if the command is not found it should set *pferr to FALSE and
   return NULL.  Otherwise, the return value may point to a common
   static buffer.  */
extern const char *zsysdep_find_command P((const char *zcmd,
					   const char *zcmds,
					   const char *zpath,
					   boolean *pferr));

#if ! ALLOW_FILENAME_ARGUMENTS
/* Check an argument to an execution command to make sure that it
   doesn't refer to a file name that may not be accessed.  This should
   check the argument to see if it is a filename.  If it is, it should
   either reject it out of hand or it should call fin_directory_list
   on the file with both qsys->zremote_receive and qsys->zremote_send.
   If the file is rejected, it should log an error and return FALSE.
   Otherwise it should return TRUE.  */
extern boolean fsysdep_xqt_check_file P((const struct ssysteminfo *qsys,
					 const char *zfile));
#endif /* ! ALLOW_FILENAME_ARGUMENTS */

/* Run an execute file.  The arguments are:

   qsys -- system for which execute file was created
   zuser -- user who requested execution
   zcmd -- command to execute (from zsysdep_find_command)
   pazargs -- list of arguments to command
   zfullcmd -- command and arguments stuck together in one string
   zinput -- file name for standard input (may be NULL)
   zoutput -- file name for standard output (may be NULL)
   fshell -- if TRUE, use /bin/sh to execute file
   pzerror -- set to name of standard error file

   If fshell is TRUE, the command should be executed with /bin/sh
   (obviously, this can only really be done on Unix systems).  This
   should return FALSE if an error occurrs.  */
extern boolean fsysdep_execute P((const struct ssysteminfo *qsys,
				  const char *zuser,
				  const char *zcmd,
				  const char **pazargs,
				  const char *zfullcmd,
				  const char *zinput,
				  const char *zoutput,
				  boolean fshell,
				  const char **pzerror));

/* Lock a particular uuxqt command (e.g. rmail).  This should return
   FALSE if the command is already locked.  This is used to make sure
   only one uuxqt process is handling a particular command.  There is
   no way to return error.  */
extern boolean fsysdep_lock_uuxqt P((const char *zcmd));

/* Unlock a particular uuxqt command.  This should return FALSE on
   error.  */
extern boolean fsysdep_unlock_uuxqt P((const char *zcmd));

/* See whether a particular uuxqt command is locked.  This should
   return TRUE if the command is locked (because fsysdep_lock_uuxqt
   was called), FALSE otherwise.  There is no way to return error.  */
extern boolean fsysdep_uuxqt_locked P((const char *zcmd));

/* Lock an execute file in order to execute it.  This should return
   FALSE if the execute file is already locked.  There is no way to
   return error.  */
extern boolean fsysdep_lock_uuxqt_file P((const char *zfile));

/* Unlock an execute file.  This should return FALSE on error.  */
extern boolean fsysdep_unlock_uuxqt_file P((const char *zfile));

/* Lock the execution directory.  This should return FALSE if the
   directory is already locked.  There is no way to return error.  */
extern boolean fsysdep_lock_uuxqt_dir P((void));

/* Remove all files in the execution directory, and unlock it.  This
   should return FALSE on error.  */
extern boolean fsysdep_unlock_uuxqt_dir P((void));

/* Add the working directory to a file name.  If the file already has
   a directory, it should not be changed.  The return value may point
   to a common static buffer.  If the flocal argument is TRUE, then
   this is a local file (so, for example, a leading ~ should be
   expanded as appropriate); otherwise the file is on a remote system.
   This should return NULL on error.  */
extern const char *zsysdep_add_cwd P((const char *zfile,
				      boolean flocal));

/* Get the base name of a file.  The file will be a local file name,
   and this function should return the base file name, ideally in a
   form which will make sense on most systems; it will be used if the
   destination of a uucp is a directory.  */
extern const char *zsysdep_base_name P((const char *zfile));

/* Return a filename within a directory.  The zdir argument may name a
   file, in which case it should be returned.  If it names a
   directory, this function should get the filename from the zfile
   argument and return that filename within the directory.  */
extern const char *zsysdep_in_dir P((const char *zdir, const char *zfile));

/* Get the mode of a file.  This should return a Unix style file mode.
   It should return 0 on error.  */
extern unsigned int isysdep_file_mode P((const char *zfile));

/* See whether the user has access to a file.  This is called by uucp
   and uux to prevent copying of a file which uucp can read but the
   user cannot.  If access is denied, this should log an error message
   and return FALSE.  */
extern boolean fsysdep_access P((const char *zfile));

/* See whether the daemon has access to a file.  This is called by
   uucp and uux when a file is queued up for transfer without being
   copied into the spool directory.  It is merely an early error
   check, as the daemon would of course discover the error itself when
   it tried the transfer.  If access would be denied, this should log
   an error message and return FALSE.  */
extern boolean fsysdep_daemon_access P((const char *zfile));

#endif
