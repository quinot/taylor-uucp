/* prot.h
   Protocol header file.

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
   */

#ifndef PROT_H

#define PROT_H

#ifdef __GNUC__
 #pragma once
#endif

/* The sprotocol structure holds information and functions for a specific
   protocol (e.g. the 'g' protocol).  */

struct sprotocol
{
  /* The name of the protocol (e.g. 'g').  */
  char bname;
  /* Whether the protocol is full-duplex or not; a full-duplex protocol
     can transfer files in both directions at once.  */
  boolean ffullduplex;
  /* Protocol parameter commands.  */
  struct scmdtab *qcmds;
  /* A routine to start the protocol; the argument is whether the caller
     is the master or the slave.  Returns TRUE if the protocol has been
     succesfully started, FALSE otherwise.  */
  boolean (*pfstart) P((boolean fmaster));
  /* Send a file.
     fmaster -- whether this job is the master
     e -- open file (send if fmaster, receive if ! fmaster)
     qcmd -- command to execute
     zmail -- user to notify on source system.
     ztosys -- system the file is being sent to
     fnew -- whether the system is running our code or not

     If this is called with fmaster TRUE, it is responsible for informing
     the slave that it wants to send a file; the slave will eventually call
     pfreceive.  The file should then be transferred.  When the transfer
     is complete, then if fmaster is TRUE and zmail is not NULL, mail should
     be sent to the user zmail; if fmaster is FALSE and qcmd->znotify is not
     NULL, mail should be sent to the user qcmd->znotify.  After the file
     transfer is complete, the work queue entry should be removed by calling
     fsysdep_did_work (pseq).

     For some protocols, this function will not return until the file is
     completely transferred.  For others, it will return quickly and
     transfer the file while considering other commands.  The return value
     is FALSE if some error occurred.  */
  boolean (*pfsend) P((boolean fmaster, openfile_t e,
		       const struct scmd *qcmd, const char *zmail,
		       const char *ztosys, boolean fnew));
  /* Receive a file.
     fmaster -- whether this job is the master
     e -- open file (receive if fmaster, send if ! fmaster)
     qcmd -- command to execute
     zmail -- user to notify on destination system.
     zfromsys -- system the file is from
     fnew -- whether the system is running our code or not

     The field qcmd->znotify will not be meaningful.

     If this is called with fmaster TRUE, it is responsible for informing
     the slave that it wants to receive a file; the slave will eventually
     call pfsend.  The file should then be transferred.  When the transfer
     is complete, if fmaster is TRUE and zmail is not NULL, mail should
     be sent to the user zmail.  After the file transfer is complete,
     the work queue entry should be removed by calling
     fsysdep_did_work (pseq).

     For some protocols, this function will not return until the file is
     completely transferred.  For others, it will return quickly and
     transfer the file while considering other commands.  The return value
     is FALSE if some error occurred.  */
  boolean (*pfreceive) P((boolean fmaster, openfile_t e,
			  const struct scmd *qcmd, const char *zmail,
			  const char *zfromsys, boolean fnew));
  /* Request a transfer.  This is only called by the master.
     qcmd -- command (only pseq, zfrom, zto, zuser, zoptions valid)

     This function should tell the slave that the master wants to
     execute a transfer.  The slave may queue up work to do.  The
     return value is FALSE if some error occurred.  This always does
     its work immediately, so it does not use qcmd->pseq.  */
  boolean (*pfxcmd) P((const struct scmd *qcmd));
  /* Confirm a transfer.  This is only called by the slave.  This is
     called after a transfer request has been received to confirm that
     it was successful.  If it was not successful, pffail will be
     called with a first argument of 'X'.  */
  boolean (*pfxcmd_confirm) P((void));
  /* Get a command from the master.  The strings in the command
     argument are set to point into a static buffer.  If fmaster is
     TRUE, this should not wait if there is no command pending; if
     fmaster is FALSE it should wait until a command is received.
     The field qcmd->pseq will be set to NULL.  */
  boolean (*pfgetcmd) P((boolean fmaster, struct scmd *qcmd));
  /* Check readiness.  Return TRUE if the protocol can do another master
     command.  In general, this will only be called for a full-duplex
     protocol, but it should be defined in any case.  */
  boolean (*pfready) P((void));
  /* Fail.  This is called by the slave if it is unable to execute
     some request by the master.  The argument bcmd is the request
     which failed ('S' or 'R').  The argument twhy indicates the
     reason.  The return value is FALSE if some error occurred.  */
  boolean (*pffail) P((int bcmd, enum tfailure twhy));
  /* Hangup.  This is only called by the master, and indicates that
     the master is ready to relinquish control; after calling it, the
     master becomes the slave.  If the original slave has no work to
     do, it confirms the hangup (the new slave will wind up getting a
     'Y' command from pfgetcmd).  If the the original slave has work
     to do, it becomes the master (it also denies the hangup, but this
     is not seen outside the protocol code).  The return value of
     pfhangup is FALSE if some error occurred.  */
  boolean (*pfhangup) P((void));
  /* Hangup reply.  This is only called by the slave if the master has
     sent a hangup request.  If fconfirm is TRUE, the slave is
     confirming the hangup, in which case the protocol should be shut
     down.  If fconfirm is FALSE, the slave will become the master.
     The return value is FALSE if some error occurred.  */
  boolean (*pfhangup_reply) P((boolean fconfirm));
  /* Shutdown the protocol.  This is only called when an error occurs.  */
  boolean (*pfshutdown) P((void));
};

/* Prototypes for 'g' protocol functions.  */

extern struct scmdtab asGproto_params[];
extern boolean fgstart P((boolean fmaster));
extern boolean fgsend P((boolean fmaster, openfile_t e,
			 const struct scmd *qcmd, const char *zmail,
			 const char *ztosys, boolean fnew));
extern boolean fgreceive P((boolean fmaster, openfile_t e,
			    const struct scmd *qcmd, const char *zmail,
			    const char *zfromsys, boolean fnew));
extern boolean fgxcmd P((const struct scmd *qcmd));
extern boolean fgxcmd_confirm P((void));
extern boolean fggetcmd P((boolean fmaster, struct scmd *qcmd));
extern boolean fgready P((void));
extern boolean fgfail P((int bcmd, enum tfailure twhy));
extern boolean fghangup P((void));
extern boolean fghangup_reply P((boolean fconfirm));
extern boolean fgshutdown P((void));

#endif
