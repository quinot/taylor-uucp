/* prot.h
   Protocol header file.

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

/* The sprotocol structure holds information and functions for a specific
   protocol (e.g. the 'g' protocol).  */

struct sprotocol
{
  /* The name of the protocol (e.g. 'g').  */
  char bname;
  /* Whether the protocol is full-duplex or not; a full-duplex protocol
     can transfer files in both directions at once.  */
  boolean ffullduplex;
  /* Reliability requirements, an or of RELIABLE_xxx defines from
     port.h.  */
  int ireliable;
  /* Protocol parameter commands.  */
  struct uuconf_cmdtab *qcmds;
  /* A routine to start the protocol; the argument is whether the caller
     is the master or the slave.  Returns TRUE if the protocol has been
     succesfully started, FALSE otherwise.  */
  boolean (*pfstart) P((struct sconnection *qconn, boolean fmaster));
  /* Shutdown the protocol.  */
  boolean (*pfshutdown) P((struct sconnection *qconn));
  /* Send a command to the other side.  */
  boolean (*pfsendcmd) P((struct sconnection *qconn, const char *z));
  /* Get buffer to space to fill with data.  This should set *pcdata
     to the amount of data desired.  */
  char *(*pzgetspace) P((struct sconnection *qconn, size_t *pcdata));
  /* Send data to the other side.  The first argument must be a return
     value of pzgetspace.  */
  boolean (*pfsenddata) P((struct sconnection *qconn, char *z,
			   size_t c));
  /* Process received data in abPrecbuf, calling fgot_data as
     necessary.  If fgot_data sets *pfexit, it should get passed back
     out.  */
  boolean (*pfprocess) P((struct sconnection *qconn,
			  boolean *pfexit));
  /* Wait for data to come in and call fgot_data with it until
     fgot_data sets *pfexit.  */
  boolean (*pfwait) P((struct sconnection *qconn));
  /* Handle any file level actions that need to be taken.  If fstart
     is TRUE, a file transfer is beginning.  If fstart is FALSE a file
     transfer is ending, and *pfredo should be set to TRUE if the file
     transfer needs to be redone.  If fstart and fsend are both TRUE,
     cbytes holds the size of the file or -1 if it is unknown.  */
  boolean (*pffile) P((struct sconnection *qconn, boolean fstart,
		       boolean fsend, boolean *pfredo, long cbytes));
};

/* Send a file.
   fmaster -- whether this job is the master
   e -- open file (send if fmaster, receive if ! fmaster)
   qcmd -- command to execute
   zmail -- user to notify on source system.
   ztosys -- system the file is being sent to
   fnew -- whether the system is running our code or not

   If this is called with fmaster TRUE, it is responsible for
   informing the slave that it wants to send a file; the slave will
   eventually call freceive_file.  The file should then be
   transferred.  When the transfer is complete, then if fmaster is
   TRUE and zmail is not NULL, mail should be sent to the user zmail;
   if fmaster is FALSE and qcmd->znotify is not NULL, mail should be
   sent to the user qcmd->znotify.  After the file transfer is
   complete, the work queue entry should be removed by calling
   fsysdep_did_work (pseq).  Most of the latter stuff is handled by
   fsent_file.  */
extern boolean fsend_file P((boolean fmaster, openfile_t e,
			     const struct scmd *qcmd,
			     struct sconnection *qconn,
			     const char *zmail,
			     const char *ztosys, boolean fnew));

/* Receive a file.
   fmaster -- whether this job is the master
   e -- open file (receive if fmaster, send if ! fmaster)
   qcmd -- command to execute
   zmail -- user to notify on destination system.
   zfromsys -- system the file is from
   fspool -- whether this is a spool directory file
   fnew -- whether the system is running our code or not

   The field qcmd->znotify will not be meaningful.

   If this is called with fmaster TRUE, it is responsible for
   informing the slave that it wants to receive a file; the slave will
   eventually call pfsend.  The file should then be transferred.  When
   the transfer is complete, if fmaster is TRUE and zmail is not NULL,
   mail should be sent to the user zmail.  After the file transfer is
   complete, the work queue entry should be removed by calling
   fsysdep_did_work (pseq).  Most of the latter work is done by
   freceived_file.  */
extern boolean freceive_file P((boolean fmaster, openfile_t e,
				const struct scmd *qcmd,
				struct sconnection *qconn,
				const char *zmail,
				const char *zfromsys, boolean fspool,
				boolean fnew));

/* Request a transfer.  This is only called by the master.
   qcmd -- command (only pseq, zfrom, zto, zuser, zoptions valid)

   This function should tell the slave that the master wants to
   execute a transfer.  The slave may queue up work to do.  The return
   value is FALSE if some error occurred.  This always does its work
   immediately, so it does not use qcmd->pseq.  It sets *pfnever to
   TRUE if the request was denied.  */
extern boolean fxcmd P((const struct scmd *qcmd,
			struct sconnection *qconn,
			boolean *pfnever));

/* Confirm a transfer.  This is only called by the slave.  This is
   called after a transfer request has been received to confirm that
   it was successful.  If it was not successful, pffail will be
   called with a first argument of 'X'.  */
extern boolean fxcmd_confirm P((struct sconnection *qconn));

/* Fail.  This is called by the slave if it is unable to execute some
   request by the master.  The argument bcmd is the request which
   failed ('S' or 'R').  The argument twhy indicates the reason.  The
   return value is FALSE if some error occurred.  */
extern boolean ftransfer_fail P((int bcmd, enum tfailure twhy,
				 struct sconnection *qconn));

/* Get a command from the master.  The strings in the command argument
   are set to point into a static buffer.  If fmaster is TRUE, this
   should not wait if there is no command pending; if fmaster is FALSE
   it should wait until a command is received.  The field qcmd->pseq
   will be set to NULL.  */
extern boolean fgetcmd P((boolean fmaster, struct scmd *qcmd,
			  struct sconnection *qconn));

/* Get a command string from the other system, where the nature of a
   command string is defined by the protocol.  The return value is
   fragile, and must be saved if any other protocol related calls are
   made.  */
extern const char *zgetcmd P((struct sconnection *qconn));

/* Hangup.  This is only called by the master, and indicates that the
   master is ready to relinquish control; after calling it, the master
   becomes the slave.  If the original slave has no work to do, it
   confirms the hangup (the new slave will wind up getting a 'Y'
   command from fgetcmd).  If the the original slave has work to do,
   it becomes the master (it also denies the hangup, but this is not
   seen outside the protocol code).  The return value of fhangup is
   FALSE if some error occurred.  */
extern boolean fhangup_request P((struct sconnection *qconn));

/* Hangup reply.  This is only called by the slave if the master has
   sent a hangup request.  If fconfirm is TRUE, the slave is
   confirming the hangup, in which case the protocol should be shut
   down.  If fconfirm is FALSE, the slave will become the master.  The
   return value is FALSE if some error occurred.  */
extern boolean fhangup_reply P((boolean fconfirm,
				struct sconnection *qconn));

/* Handle data received by a protocol.  This is called by the protocol
   specific routines as data comes in.  The protocol specific routines
   may know that the data is destined for a command or a file, in
   which case they should pass fcmd and ffile appropriately.
   Otherwise they may both be passed as FALSE, in which case if a file
   recieve is in progress the data will be sent to the file, otherwise
   to a command.  This will set *pfexit to TRUE if the file or command
   is finished.  A file is finished when a zero length buffer is
   passed.  A command is finished when a string containing a null byte
   is passed.  This will return FALSE on error.  */
extern boolean fgot_data P((const char *zdata, size_t cdata,
			    boolean fcmd, boolean ffile,
			    boolean *pfexit,
			    struct sconnection *qconn));

/* Send data to the other system.  If the fread argument is TRUE, this
   will also receive data into the receive buffer abPrecbuf; fread is
   passed as TRUE if the protocol expects data to be coming back, to
   make sure the input buffer does not fill up.  Returns FALSE on
   error.  */
extern boolean fsend_data P((struct sconnection *qconn,
			     const char *zsend, size_t csend,
			     boolean fdoread));

/* Receive data from the other system when there is no data to send.
   The cneed argument is the amount of data desired and the ctimeout
   argument is the timeout in seconds.  This will set *pcrec to the
   amount of data received.  It will return FALSE on error.  If a
   timeout occurs, it will return TRUE with *pcrec set to zero.  */
extern boolean freceive_data P((struct sconnection *qconn, size_t cneed,
				size_t *pcrec, int ctimeout,
				boolean freport));

/* Get one character from the remote system, going through the
   procotol buffering.  The ctimeout argument is the timeout in
   seconds, and the freport argument is TRUE if errors should be
   reported (when closing a connection it is pointless to report
   errors).  This returns a character or -1 on a timeout or -2 on an
   error.  */
extern int breceive_char P((struct sconnection *qconnection,
			    int ctimeout, boolean freport));

/* Protocol in use.  */
extern const struct sprotocol *qProto;

/* The size of the receive buffer.  */
#define CRECBUFLEN (16384)

/* Buffer to hold received data.  */
extern char abPrecbuf[CRECBUFLEN];

/* Index of start of data in abPrecbuf.  */
extern int iPrecstart;

/* Index of end of data (first byte not included in data) in abPrecbuf.  */
extern int iPrecend;

/* Whether an unexpected shutdown is OK now; this is used to avoid
   giving a warning for systems that hang up in a hurry.  */
extern boolean fPerror_ok;

/* Prototypes for 'g' protocol functions.  */

extern struct uuconf_cmdtab asGproto_params[];
extern boolean fgstart P((struct sconnection *qconn,
			  boolean fmaster));
extern boolean fgshutdown P((struct sconnection *qconn));
extern boolean fgsendcmd P((struct sconnection *qconn, const char *z));
extern char *zggetspace P((struct sconnection *qconn, size_t *pcdata));
extern boolean fgsenddata P((struct sconnection *qconn,
			     char *z, size_t c));
extern boolean fgprocess P((struct sconnection *qconn, boolean *pfexit));
extern boolean fgwait P((struct sconnection *qconn));

/* Prototypes for 'f' protocol functions.  */

extern struct uuconf_cmdtab asFproto_params[];
extern boolean ffstart P((struct sconnection *qconn, boolean fmaster));
extern boolean ffshutdown P((struct sconnection *qconn));
extern boolean ffsendcmd P((struct sconnection *qconn, const char *z));
extern char *zfgetspace P((struct sconnection *qconn, size_t *pcdata));
extern boolean ffsenddata P((struct sconnection *qconn, char *z, size_t c));
extern boolean ffprocess P((struct sconnection *qconn, boolean *pfexit));
extern boolean ffwait P((struct sconnection *qconn));
extern boolean fffile P((struct sconnection *qconn, boolean fstart,
			 boolean fsend, boolean *pfredo, long cbytes));

/* Prototypes for 't' protocol functions.  */

extern struct uuconf_cmdtab asTproto_params[];
extern boolean ftstart P((struct sconnection *qconn, boolean fmaster));
extern boolean ftshutdown P((struct sconnection *qconn));
extern boolean ftsendcmd P((struct sconnection *qconn, const char *z));
extern char *ztgetspace P((struct sconnection *qconn, size_t *pcdata));
extern boolean ftsenddata P((struct sconnection *qconn, char *z, size_t c));
extern boolean ftprocess P((struct sconnection *qconn, boolean *pfexit));
extern boolean ftwait P((struct sconnection *qconn));
extern boolean ftfile P((struct sconnection *qconn, boolean fstart,
			 boolean fsend, boolean *pfredo, long cbytes));

/* Prototypes for 'e' protocol functions.  */

extern struct uuconf_cmdtab asEproto_params[];
extern boolean festart P((struct sconnection *qconn, boolean fmaster));
extern boolean feshutdown P((struct sconnection *qconn));
extern boolean fesendcmd P((struct sconnection *qconn, const char *z));
extern char *zegetspace P((struct sconnection *qconn, size_t *pcdata));
extern boolean fesenddata P((struct sconnection *qconn, char *z, size_t c));
extern boolean feprocess P((struct sconnection *qconn, boolean *pfexit));
extern boolean fewait P((struct sconnection *qconn));
extern boolean fefile P((struct sconnection *qconn, boolean fstart,
			 boolean fsend, boolean *pfredo, long cbytes));
