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

/* We use these typedefs for pointers to structures that may not be
   defined yet.  This avoids problems of defining a structure in an
   ANSI C prototype.  */
typedef struct sdaemon *daemon_ptr;
typedef struct stransfer *transfer_ptr;

/* The sprotocol structure holds information and functions for a specific
   protocol (e.g. the 'g' protocol).  */

struct sprotocol
{
  /* The name of the protocol (e.g. 'g').  */
  char bname;
  /* Reliability requirements, an or of RELIABLE_xxx defines from
     port.h.  */
  int ireliable;
  /* The maximum number of connections this protocol can support.  */
  int cconns;
  /* Protocol parameter commands.  */
  struct uuconf_cmdtab *qcmds;
  /* A routine to start the protocol; the argument is whether the caller
     is the master or the slave.  Returns TRUE if the protocol has been
     succesfully started, FALSE otherwise.  */
  boolean (*pfstart) P((daemon_ptr qdaemon, boolean fmaster));
  /* Shutdown the protocol.  */
  boolean (*pfshutdown) P((daemon_ptr qdaemon));
  /* Send a command to the other side.  */
  boolean (*pfsendcmd) P((daemon_ptr qdaemon, const char *z));
  /* Get buffer to space to fill with data.  This should set *pcdata
     to the amount of data desired.  */
  char *(*pzgetspace) P((daemon_ptr qdaemon, size_t *pcdata));
  /* Send data to the other side.  The argument z must be a return
     value of pzgetspace.  The ipos argument is the file position, and
     is ignored by most protocols.  */
  boolean (*pfsenddata) P((daemon_ptr qdaemon, char *z, size_t c,
			   long ipos));
  /* Wait for data to come in and call fgot_data with it until
     fgot_data sets *pfexit.  */
  boolean (*pfwait) P((daemon_ptr qdaemon));
  /* Handle any file level actions that need to be taken.  If a file
     transfer is starting rather than ending, fstart is TRUE.  If the
     file is being sent rather than received, fsend is TRUE.  If
     fstart and fsend are both TRUE, cbytes holds the size of the
     file.  If *pfhandled is set to TRUE, then the protocol routine
     has taken care of queueing up qtrans for the next action.  */
  boolean (*pffile) P((daemon_ptr qdaemon, transfer_ptr qtrans,
		       boolean fstart, boolean fsend, long cbytes,
		       boolean *pfhandled));
};

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
extern int breceive_char P((struct sconnection *qconn,
			    int ctimeout, boolean freport));

/* The size of the receive buffer.  */
#define CRECBUFLEN (16384)

/* Buffer to hold received data.  */
extern char abPrecbuf[CRECBUFLEN];

/* Index of start of data in abPrecbuf.  */
extern int iPrecstart;

/* Index of end of data (first byte not included in data) in abPrecbuf.  */
extern int iPrecend;

/* Prototypes for 'g' protocol functions.  */

extern struct uuconf_cmdtab asGproto_params[];
extern boolean fgstart P((daemon_ptr qdaemon, boolean fmaster));
extern boolean fgshutdown P((daemon_ptr qdaemon));
extern boolean fgsendcmd P((daemon_ptr qdaemon, const char *z));
extern char *zggetspace P((daemon_ptr qdaemon, size_t *pcdata));
extern boolean fgsenddata P((daemon_ptr qdaemon, char *z, size_t c,
			     long ipos));
extern boolean fgwait P((daemon_ptr qdaemon));

/* Prototypes for 'f' protocol functions.  */

extern struct uuconf_cmdtab asFproto_params[];
extern boolean ffstart P((daemon_ptr qdaemon, boolean fmaster));
extern boolean ffshutdown P((daemon_ptr qdaemon));
extern boolean ffsendcmd P((daemon_ptr qdaemon, const char *z));
extern char *zfgetspace P((daemon_ptr qdaemon, size_t *pcdata));
extern boolean ffsenddata P((daemon_ptr qdaemon, char *z, size_t c,
			     long ipos));
extern boolean ffwait P((daemon_ptr qdaemon));
extern boolean fffile P((daemon_ptr qdaemon, transfer_ptr qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));

/* Prototypes for 't' protocol functions.  */

extern struct uuconf_cmdtab asTproto_params[];
extern boolean ftstart P((daemon_ptr qdaemon, boolean fmaster));
extern boolean ftshutdown P((daemon_ptr qdaemon));
extern boolean ftsendcmd P((daemon_ptr qdaemon, const char *z));
extern char *ztgetspace P((daemon_ptr qdaemon, size_t *pcdata));
extern boolean ftsenddata P((daemon_ptr qdaemon, char *z, size_t c,
			     long ipos));
extern boolean ftwait P((daemon_ptr qdaemon));
extern boolean ftfile P((daemon_ptr qdaemon, transfer_ptr qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));

/* Prototypes for 'e' protocol functions.  */

extern struct uuconf_cmdtab asEproto_params[];
extern boolean festart P((daemon_ptr qdaemon, boolean fmaster));
extern boolean feshutdown P((daemon_ptr qdaemon));
extern boolean fesendcmd P((daemon_ptr qdaemon, const char *z));
extern char *zegetspace P((daemon_ptr qdaemon, size_t *pcdata));
extern boolean fesenddata P((daemon_ptr qdaemon, char *z, size_t c,
			     long ipos));
extern boolean fewait P((daemon_ptr qdaemon));
extern boolean fefile P((daemon_ptr qdaemon, transfer_ptr qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));
