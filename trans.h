/* trans.h
   Header file for file and command transfer routines.

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
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

/* The maximum possible number of connections.  */
#define IMAX_CONN (256)

/* This structure is used to hold information concerning the
   communication link established with the remote system.  */

struct sdaemon
{
  /* Global uuconf pointer.  */
  pointer puuconf;
  /* Remote system information.  */
  const struct uuconf_system *qsys;
  /* Local name being used.  */
  const char *zlocalname;
  /* Connection structure.  */
  struct sconnection *qconn;
  /* Protocol being used.  */
  const struct sprotocol *qproto;
  /* The largest file size permitted for a local request.  */
  long clocal_size;
  /* The largest file size permitted for a remote request.  */
  long cremote_size;
  /* The largest file size that may ever be transferred.  */
  long cmax_ever;
  /* The remote system ulimit.  */
  long cmax_receive;
  /* TRUE if the remote side is Taylor UUCP.  */
  boolean fnew;
  /* TRUE if we are hanging up.  */
  boolean fhangup;
  /* TRUE if the local system is currently the master.  */
  boolean fmaster;
  /* TRUE if the local system placed the call.  */
  boolean fcaller;
  /* TRUE if the connection is half-duplex.  */
  boolean fhalfduplex;
  /* If fcaller is FALSE, the lowest grade which may be transferred
     during this call.  */
  char bgrade;
};

/* This structure is used to hold a file or command transfer which is
   in progress.  */

struct stransfer
{
  /* Next file transfer in queue.  */
  struct stransfer *qnext;
  /* Previous file transfer in queue.  */
  struct stransfer *qprev;
  /* Points to the queue this structure is on.  */
  struct stransfer **pqqueue;
  /* The function to call to send some data.  */
  boolean (*psendfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon));
  /* The function to call when data is received.  */
  boolean (*precfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon,
		       const char *zdata, size_t cdata));
  /* Type specific information.   */
  pointer pinfo;
  /* TRUE if we are sending the file e (this is used to avoid a call
     to psendfn).  */
  boolean fsendfile;
  /* TRUE if we are receiving the file e (this is used to avoid a call
     to precfn).  */
  boolean frecfile;
  /* The file to read or write.  */
  openfile_t e;
  /* The position we are at in the file.  */
  long ipos;
  /* TRUE if we are waiting for a command string.  */
  boolean fcmd;
  /* The command string we have so far.  */
  char *zcmd;
  /* The length of the command string we have so far.  */
  size_t ccmd;
  /* Local destination number.  */
  int ilocal;
  /* Remote destination number.  */
  int iremote;
  /* The command.  */
  struct scmd s;
  /* The process time; imicros can be negative.  */
  long isecs;
  long imicros;
  /* Number of bytes sent or received.  */
  long cbytes;
};

/* The main loop which talks to the remote system, passing transfer
   requests and file back and forth.  */
extern boolean floop P((struct sdaemon *qdaemon));

/* Allocate a new transfer structure.  */
extern struct stransfer *qtransalc P((struct scmd *qcmd));

/* Free a transfer structure.  */
extern void utransfree P((struct stransfer *qtrans));

/* Queue up local requests.  If pfany is not NULL, this sets *pfany to
   TRUE if there are, in fact, any local requests which can be done at
   this point.  */
extern boolean fqueue P((struct sdaemon *qdaemon, boolean *pfany));

/* Queue a new transfer request made by the local system.  */
extern void uqueue_local P((struct stransfer *qtrans));

/* Queue a new transfer request made by the remote system.  */
extern void uqueue_remote P((struct stransfer *qtrans));

/* Queue a transfer request which wants to send something.  */
extern void uqueue_send P((struct stransfer *qtrans));

/* Queue a transfer request which wants to receiving something.  */
extern void uqueue_receive P((struct stransfer *qtrans));

/* Prepare to send a file by local or remote request.  */
extern boolean flocal_send_file_init P((struct sdaemon *qdaemon,
					struct scmd *qcmd));
extern boolean fremote_send_file_init P((struct sdaemon *qdaemon,
					 struct scmd *qcmd));

/* Prepare to receive a file by local or remote request.  */
extern boolean flocal_rec_file_init P((struct sdaemon *qdaemon,
				       struct scmd *qcmd));
extern boolean fremote_rec_file_init P((struct sdaemon *qdaemon,
					struct scmd *qcmd));

/* Prepare to request work by local or remote request.  */
extern boolean flocal_xcmd_init P((struct sdaemon *qdaemon,
				   struct scmd *qcmd));
extern boolean fremote_xcmd_init P((struct sdaemon *qdaemon,
				    struct scmd *qcmd));

/* Handle data received by a protocol.  This is called by the protocol
   specific routines as data comes in.  The data is passed as two
   buffers because that is convenient for packet based protocols.  The
   ilocal argument is the local connection number, and the iremote
   argument is the remote connection number.  Either may be -1, if the
   protocol does not have connections.  The ipos argument is the
   position in the file, if the protocol knows it; for most protocols,
   this will be -1.  This will set *pfexit to TRUE if there is
   something for the main loop to do.  A file is complete is when a
   zero length buffer is passed (cfirst == 0).  A command is complete
   when data containing a null byte is passed.  This will return FALSE
   on error.  */
extern boolean fgot_data P((struct sdaemon *qdaemon,
			    const char *zfirst, size_t cfirst,
			    const char *zsecond, size_t csecond,
			    int ilocal, int iremote,
			    long ipos, boolean *pfexit));
