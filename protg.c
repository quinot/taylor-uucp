/* protg.c
   The 'g' protocol.

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
   Revision 1.1  1991/09/10  19:40:31  ian
   Initial revision

   */

#include "uucp.h"

#if USE_RCS_ID
char protg_rcsid[] = "$Id$";
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "prot.h"
#include "port.h"
#include "system.h"

/* Each 'g' protocol packet begins with six bytes.  They are:

   <DLE><k><c0><c1><C><x>

   <DLE> is the ASCII DLE character (^P or '\020').
   if 1 <= <k> <= 8, the packet is followed by 2 ** (k + 4) bytes of data;
   if <k> == 9, these six bytes are a complete control packet;
   other value of <k> are illegal.
   <c0> is the low byte of a checksum.
   <c1> is the high byte of a checksum.
   <C> is a control byte (see below).
   <x> is <k> ^ <c0> ^ <c1> ^ <C>.

   The control byte <C> is divided into three bitfields:

   t t x x x y y y

   The two bit field tt is the packet type.
   The three bit field xxx is the control type for a control packet, or
   the sequence number for a data packet.
   The three bit field yyy is a value for a control packet, or the
   sequence number of the last packet received for a data packet.

   For all successfully recieved packets, the control byte is stored
   into iGpacket_control.  */

/* Names for the bytes in the frame header.  */

#define IFRAME_DLE (0)
#define IFRAME_K (1)
#define IFRAME_CHECKLOW (2)
#define IFRAME_CHECKHIGH (3)
#define IFRAME_CONTROL (4)
#define IFRAME_XOR (5)

/* Length of the frame header.  */
#define CFRAMELEN (6)

/* Macros to break apart the control bytes.  */

#define CONTROL_TT(b) ((int)(((b) >> 6) & 03))
#define CONTROL_XXX(b) ((int)(((b) >> 3) & 07))
#define CONTROL_YYY(b) ((int)((b) & 07))

/* DLE value.  */
#define DLE ('\020')

/* <k> field value for a control message.  */
#define KCONTROL (9)

/* Get the next sequence number given a sequence number.  */
#define INEXTSEQ(i) ((i + 1) & 07)

/* Compute i1 - i2 modulo 8.  */
#define CSEQDIFF(i1, i2) (((i1) + 8 - (i2)) & 07)

/* Packet types.  These are from the tt field.
   CONTROL -- control packet
   ALTCHAN -- alternate channel; not used by UUCP
   DATA -- full data segment
   SHORTDATA -- less than full data segment (all the bytes specified by
   the packet length <k> are always transferred).  Let <u> be the number
   of bytes in the data segment not to be used.  If <u> <= 0x7f, the first
   byte of the data segment is <u> and the data follows.  If <u> > 0x7f,
   the first byte of the data segment is 0x80 | (<u> & 0x7f), the second
   byte of the data segment is <u> >> 7, and the data follows.  The
   maximum possible data segment size is 2**12, so this handles all
   possible cases.  */

#define CONTROL (0)
#define ALTCHAN (1)
#define DATA (2)
#define SHORTDATA (3)

/* Control types.  These are from the xxx field if the type (tt field)
   is CONTROL.

   CLOSE -- close the connection
   RJ -- reject; packet yyy last to be received correctly
   SRJ -- selective reject; reject only packet yyy (not used by UUCP)
   RR -- receiver ready; packet yyy received correctly
   INITC -- third step of initialization; yyy holds window size
   INITB -- second step of initialization; yyy holds maximum <k> value - 1
   INITA -- first step of initialization; yyy holds window size.

   The yyy value for RR is the same as the yyy value for an ordinary
   data packet.  */

#define CLOSE (1)
#define RJ (2)
#define SRJ (3)
#define RR (4)
#define INITC (5)
#define INITB (6)
#define INITA (7)

/* Maximum amount of data in a single packet.  This is set by the <k>
   field in the header; the amount of data in a packet is
   2 ** (<k> + 4).  <k> ranges from 1 to 8.  */
    
#define CMAXDATAINDEX (8)

#define CMAXDATA (1 << (CMAXDATAINDEX + 4))

/* Maximum window size.  */

#define CMAXWINDOW (7)

/* Local variables.  */

/* Next sequence number to send.  */
static int iGsendseq;

/* Last sequence number that has been acked.  */
static int iGremote_ack;

/* Last sequence number to be retransmitted.  */
static int iGretransmit_seq;

/* Last sequence number we have received.  */
static int iGrecseq;

/* Last sequence number we have acked.  */
static int iGlocal_ack;

/* Number of bytes sent for current file.  */
static long cGsent_bytes;

/* Number of bytes received for current file.  */
static long cGreceived_bytes;

/* Whether we've reported an error for a received file.  */
static boolean fGreceived_error;

/* Local window size.  */
int iGlocal_winsize = 3;

/* Local packet size.  Used only during handshake.  */
int iGlocal_packsize = 64;

/* Remote window size (set during handshake).  */
static int iGremote_winsize;

/* Remote segment size (set during handshake).  This is one less than
   the value in the header.  */
static int iGremote_segsize;

/* Remote packet size (set during handshake).  */
static int iGremote_packsize;

/* Recieved control byte.  */
static int iGpacket_control;

/* Number of times to retry the initial handshake.  */
static int cGstartup_retries = 8;

/* Number of times to retry sending an initial control packet.  */
static int cGexchange_init_retries = 4;

/* Timeout (seconds) for receiving an initial control packet.  */
static int cGexchange_init_timeout = 10;

/* Timeout (seconds) for receiving a data packet.  */
static int cGtimeout = 10;

/* Maximum number of timeouts when receiving a data packet.  */
static int cGretries = 6;

/* Amount of garbage data we are prepared to see before giving up.  */
static int cGgarbage_data = 10000;

/* Length of receive buffer.  */
#define CRECBUFLEN (3 * CMAXDATA)

/* Buffer to hold received data.  */
static char abGrecbuf[CRECBUFLEN];

/* Index of start of data in abGrecbuf.  */
static int iGrecstart;

/* Index of end of data (first byte not included in data) in abGrecbuf.  */
static int iGrecend;

/* Whether a CLOSE packet is OK here; this is used to avoid giving a
   warning for systems that hang up in a hurry.  */
static boolean fGclose_ok;

/* Protocol parameter commands.  */

struct scmdtab asGproto_params[] =
{
  { "window", CMDTABTYPE_INT, (pointer) &iGlocal_winsize, NULL },
  { "packet-size", CMDTABTYPE_INT, (pointer) &iGlocal_packsize, NULL },
  { "startup-retries", CMDTABTYPE_INT, (pointer) &cGstartup_retries, NULL },
  { "init-timeout", CMDTABTYPE_INT, (pointer) &cGexchange_init_timeout,
      NULL },
  { "init-retries", CMDTABTYPE_INT, (pointer) &cGexchange_init_retries,
      NULL },
  { "timeout", CMDTABTYPE_INT, (pointer) &cGtimeout, NULL },
  { "retries", CMDTABTYPE_INT, (pointer) &cGretries, NULL },
  { "garbage", CMDTABTYPE_INT, (pointer) &cGgarbage_data, NULL },
  { NULL, 0, NULL, NULL }
};

/* Local functions.  */

static boolean fgexchange_init P((boolean fmaster, int ictl, int ival,
				 int *piset));
static boolean fgmain_loop P((void));
static boolean fgsendfile_confirm P((void));
static boolean fgrecfile_confirm P((void));     
static boolean fgsend_cmd P((const char *zcmd));
static boolean fgsend_control P((int ictl, int ival));
static const char *zgget_cmd P((void));
static void ugadd_cmd P((const char *zdata, int clen, boolean flast));
static boolean fggot_data P((const char *zfirst, int cfirst,
			     const char *zsecond, int csecond,
			     boolean *pferr));
static boolean fgsend_packet P((char *zpack, int clen));
static void ugadjust_ack P((int iseq));
static boolean fgwait_for_packet P((boolean freturncontrol, int ctimeout,
				    int cretries));
static boolean fgsend_acks P((void));
static boolean fggot_ack P((int iack));
static boolean fgprocess_data P((boolean fdoacks, boolean freturncontrol,
				 boolean *pferr, int *pcneed,
				 boolean *pffound));
static boolean fginit_sendbuffers P((void));
static char *zgget_send_packet P((void));
static boolean fgoutput P((const char *zpack, int clen));
static int cgframelen P((int cpacklen));
static int igchecksum P((const char *zdata, int clen));
static int igchecksum2 P((const char *zfirst, int cfirst,
			  const char *zsecond, int csecond));

/* Start the protocol.  This requires a three way handshake.  Both sides
   must send and receive an INITA packet, an INITB packet, and an INITC
   packet.  The INITA and INITC packets contain the window size, and the
   INITB packet contains the packet size.  */

boolean
fgstart (fmaster)
     boolean fmaster;
{
  int iseg;
  int i;

  iGsendseq = 1;
  iGremote_ack = 0;
  iGretransmit_seq = -1;
  iGrecseq = 0;
  iGlocal_ack = 0;
  
  /* We must determine the segment size based on the packet size
     which may have been modified by a protocol parameter command.
     A segment size of 2^n is passed as n - 5.  */

  i = iGlocal_packsize;
  iseg = -1;
  while (i > 0)
    {
      ++iseg;
      i >>= 1;
    }
  iseg -= 5;
  if (iseg < 0 || iseg > 7)
    {
      ulog (LOG_ERROR, "Illegal packet size %d for 'g' protocol",
	    iGlocal_packsize);
      iseg = 1;
    }
  
  for (i = 0; i < cGstartup_retries; i++)
    {
      if (! fgexchange_init (fmaster, INITA, iGlocal_winsize,
			    &iGremote_winsize))
	continue;
      if (! fgexchange_init (fmaster, INITB, iseg,
			    &iGremote_segsize))
	continue;
      if (! fgexchange_init (fmaster, INITC, iGlocal_winsize,
			    &iGremote_winsize))
	continue;

      /* We have succesfully connected.  Determine the remote packet
	 size.  */

      iGremote_packsize = 1 << (iGremote_segsize + 5);

      if (! fginit_sendbuffers ())
	return FALSE;

#if DEBUG > 2
      if (iDebug > 2)
	ulog (LOG_DEBUG, "fgstart: Protocol started; segsize %d, winsize %d",
	      iGremote_segsize, iGremote_winsize);
#endif

      return TRUE;
    }

#if DEBUG > 2
  if (iDebug > 2)
    ulog (LOG_DEBUG, "fgstart: Protocol startup failed");
#endif

  return FALSE;
}

/* Exchange initialization messages with the other system.

   A problem:

   We send INITA; it gets received
   We receive INITA
   We send INITB; it gets garbled
   We receive INITB

   We have seen and sent INITB, so we start to send INITC.  The other
   side as sent INITB but not seen it, so it times out and resends
   INITB.  We will continue sending INITC and the other side will
   continue sending INITB until both sides give up and start again
   with INITA.

   It might seem as though if we are sending INITC and receive INITB,
   we should resend our INITB, but this could cause infinite echoing
   of INITB on a long-latency line.  Rather than risk that, I have
   implemented a fast drop-back procedure.  If we are sending INITB and
   receive INITC, the other side has gotten ahead of us.  We immediately
   fail and begin again with INITA.  For the other side, if we are
   sending INITC and see INITA, we also immediately fail back to INITA.

   Unfortunately, this doesn't work for the other case, in which we
   are sending INITB but the other side has not yet seen INITA.  As
   far as I can see, if this happens we just have to wait until we
   time out and resend INITA.  */

/*ARGSUSED*/
static boolean
fgexchange_init (fmaster, ictl, ival, piset)
     boolean fmaster;
     int ictl;
     int ival;
     int *piset;
{
  int i;

  /* The three-way handshake should be independent of who initializes
     it, so we ignore the fmaster argument.  If some other UUCP turns
     out to care about this, we can change it.  */

  for (i = 0; i < cGexchange_init_retries; i++)
    {
      long itime;
      int ctimeout;

      if (! fgsend_control (ictl, ival))
	return FALSE;
      
      itime = isysdep_time ();
      ctimeout = cGexchange_init_timeout;

      do
	{
	  long inewtime;

	  /* We pass 0 as the retry count to fgwait_for_packet because
	     we want to handle retries here and because if it retried
	     it would send a packet, which would be bad.  */

	  if (! fgwait_for_packet (TRUE, ctimeout, 0))
	    break;

	  if (CONTROL_TT (iGpacket_control) == CONTROL)
	    {
	      if (CONTROL_XXX (iGpacket_control) == ictl)
		{
		  *piset = CONTROL_YYY (iGpacket_control);
		  return TRUE;
		}

	      /* If the other side is farther along than we are,
		 we have lost a packet.  Fail immediately back to
		 INITA (but don't fail if we are already doing INITA,
		 since that would count against cStart_retries more
		 than it should).  */
	      if (CONTROL_XXX (iGpacket_control) < ictl && ictl != INITA)
		return FALSE;

	      /* If we are sending INITC and we receive an INITA, the other
		 side has failed back (we know this because we have
		 seen an INITB from them).  Fail back ourselves to
		 start the whole handshake over again.  */
	      if (CONTROL_XXX (iGpacket_control) == INITA && ictl == INITC)
		return FALSE;
	    }

	  inewtime = isysdep_time ();
	  ctimeout -= inewtime - itime;
	}
      while (ctimeout > 0);
    }

  return FALSE;
}

/* Send a file.  If we are the master, we must set up a command to
   transfer the file (the command will be read by tggetcmd) and wait
   for a confirmation that we can begin sending the file.  If we are
   the slave, the master is waiting in fgreceive and we must confirm
   that we will send the file.  Either way, we begin transferring
   data.

   The 'g' protocol is not full-duplex, but with a few minor changes
   it could be.  This code is written as though it were, and perhaps I
   will implement a slightly modified protocol some day.  This means
   that we accept packets as we send them; if a packet does come in,
   we get out of this routine and let the receiving routine handle
   everything.

   When the file transfer is complete we must send mail to the local
   used and remove the work queue entry.

   This function returns FALSE if there is a communication failure.
   It returns TRUE otherwise, even if the file transfer failed.  */

boolean
fgsend (fmaster, e, qcmd, zmail, ztosys, fnew)
     boolean fmaster;
     openfile_t e;
     const struct scmd *qcmd;
     const char *zmail;
     const char *ztosys;
     boolean fnew;
{
  if (fmaster)
    {
      int clen;
      char *zsend;
      const char *zrec;
      
      /* Send the string
	 S zfrom zto zuser zoptions ztemp imode znotify
	 to the remote system.  We put a '-' in front of the (possibly
	 empty) options and a '0' in front of the mode.  The remote
	 system will ignore ztemp, but it is supposed to be sent anyhow.
	 If fnew is TRUE, we also send the size; in this case if ztemp
	 is empty we must send it as "".  */
      clen = (strlen (qcmd->zfrom) + strlen (qcmd->zto)
	      + strlen (qcmd->zuser) + strlen (qcmd->zoptions)
	      + strlen (qcmd->ztemp) + strlen (qcmd->znotify)
	      + 50);
      zsend = (char *) alloca (clen);
      if (! fnew)
	sprintf (zsend, "S %s %s %s -%s %s 0%o %s", qcmd->zfrom, qcmd->zto,
		 qcmd->zuser, qcmd->zoptions, qcmd->ztemp, qcmd->imode,
		 qcmd->znotify);
      else
	{
	  const char *znotify;

	  if (qcmd->znotify[0] != '\0')
	    znotify = qcmd->znotify;
	  else
	    znotify = "\"\"";
	  sprintf (zsend, "S %s %s %s -%s %s 0%o %s %ld", qcmd->zfrom,
		   qcmd->zto, qcmd->zuser, qcmd->zoptions, qcmd->ztemp,
		   qcmd->imode, znotify, qcmd->cbytes);
	}

      if (! fgsend_cmd (zsend))
	{
	  (void) ffileclose (e);
	  return FALSE;
	}

      /* Now we must await a reply.  This prevents the 'g' protocol
	 from begin full-duplex, since if there were a file transfer
	 going on in the opposite direction it would be impossible to
	 distinguish the command reply from the file contents.  We
	 could make it work by using the alternate channel, which is
	 not generally supported.  */

      zrec = zgget_cmd ();
      if (zrec == NULL)
	{
	  (void) ffileclose (e);
	  return FALSE;
	}

      if (zrec[0] != 'S'
	  || (zrec[1] != 'Y' && zrec[1] != 'N'))
	{
	  ulog (LOG_ERROR, "Bad response to send request");
	  (void) ffileclose (e);
	  return FALSE;
	}

      if (zrec[1] == 'N')
	{
	  const char *zerr;

	  if (zrec[2] == '2')
	    zerr = "permission denied";
	  else if (zrec[2] == '4')
	    {
	      /* This means the remote system cannot create
		 work files; log the error and try again later.  */
	      ulog (LOG_ERROR,
		    "Can't send %s: remote cannot create work files",
		    qcmd->zfrom);
	      (void) ffileclose (e);
	      return TRUE;
	    }
	  else if (zrec[2] == '6')
	    {
	      /* The remote system says the file is too large.  It
		 would be better if we could determine whether it will
		 always be too large.  */
	      ulog (LOG_ERROR, "%s is too big to send now",
		    qcmd->zfrom);
	      (void) ffileclose (e);
	      return TRUE;
	    }
	  else
	    zerr = "unknown reason";

	  ulog (LOG_ERROR, "Can't send %s: %s", qcmd->zfrom, zerr);
	  (void) fsysdep_did_work (qcmd->pseq);
	  (void) ffileclose (e);
	  return TRUE;
	}
    }
  else
    {
      char absend[20];


      /* We are the slave; confirm that we will send the file.  We
	 send the file mode in the confirmation string.  */

      sprintf (absend, "RY 0%o", qcmd->imode);

      if (! fgsend_cmd (absend))
	{
	  (void) ffileclose (e);
	  return FALSE;
	}
    }

  /* Record the file we are sending, and enter the main loop.  */

  if (! fstore_sendfile (e, qcmd->pseq, qcmd->zfrom, qcmd->zto, ztosys,
			 qcmd->zuser, zmail))
    return FALSE;

  cGsent_bytes = 0;

  return fgmain_loop ();
}

/* Confirm that a file has been sent.  Return FALSE for a
   communication error.  We expect the receiving system to send back
   CY; if an error occurred while moving the received file into its
   final location, the receiving system will send back CN5.  */

static boolean
fgsendfile_confirm ()
{
  const char *zrec;

  zrec = zgget_cmd ();
  if (zrec == NULL)
    return FALSE;

  if (zrec[0] != 'C'
      || (zrec[1] != 'Y' && zrec[1] != 'N'))
    {
      ulog (LOG_ERROR, "Bad confirmation for sent file");
      (void) fsent_file (FALSE, cGsent_bytes);
    }
  else if (zrec[1] == 'N')
    {
      if (zrec[2] == '5')
	ulog (LOG_ERROR, "File could not be stored in final location");
      else
	ulog (LOG_ERROR, "File send failed for unknown reason");
      (void) fsent_file (FALSE, cGsent_bytes);
    }
  else
    (void) fsent_file (TRUE, cGsent_bytes);

  return TRUE;
}

/* Receive a file.  If we are the master, we must set up a file
   request and wait for the other side to confirm it.  If we are the
   slave, we must confirm a request made by the other side.  We then
   start receiving the file.

   This function must return FALSE if there is a communication error
   and TRUE otherwise.  We return TRUE even if the file transfer
   fails.  */

boolean
fgreceive (fmaster, e, qcmd, zmail, zfromsys, fnew)
     boolean fmaster;
     openfile_t e;
     const struct scmd *qcmd;
     const char *zmail;
     const char *zfromsys;
     boolean fnew;
{
  unsigned int imode;

  if (fmaster)
    {
      int clen;
      char *zsend;
      const char *zrec;

      /* We send the string
	 R from to user options
	 We put a dash in front of options.  If we are talking to a
	 counterpart, we also send the maximum size file we are
	 prepared to accept, as returned by esysdep_open_receive.  */
      
      clen = (strlen (qcmd->zfrom) + strlen (qcmd->zto)
	      + strlen (qcmd->zuser) + strlen (qcmd->zoptions) + 30);
      zsend = (char *) alloca (clen);

      if (! fnew)
	sprintf (zsend, "R %s %s %s -%s", qcmd->zfrom, qcmd->zto,
		 qcmd->zuser, qcmd->zoptions);
      else
	sprintf (zsend, "R %s %s %s -%s %ld", qcmd->zfrom, qcmd->zto,
		 qcmd->zuser, qcmd->zoptions, qcmd->cbytes);

      if (! fgsend_cmd (zsend))
	{
	  (void) ffileclose (e);
	  return FALSE;
	}

      /* Wait for a reply.  */

      zrec = zgget_cmd ();
      if (zrec == NULL)
	{
	  (void) ffileclose (e);
	  return FALSE;
	}

      if (zrec[0] != 'R'
	  || (zrec[1] != 'Y' && zrec[1] != 'N'))
	{
	  ulog (LOG_ERROR, "Bad response to receive request");
	  (void) ffileclose (e);
	  return FALSE;
	}

      if (zrec[1] == 'N')
	{
	  const char *zerr;

	  if (zrec[2] == '2')
	    zerr = "no such file";
	  else if (zrec[2] == '6')
	    {
	      /* We sent over the maximum file size we were prepared
		 to receive, and the remote system is telling us that
		 the file is larger than that.  Try again later.  It
		 would be better if we could know whether there will
		 ever be enough room.  */
	      ulog (LOG_ERROR, "%s is too big to receive",
		    qcmd->zfrom);
	      (void) ffileclose (e);
	      return TRUE;
	    }
	  else
	    zerr = "unknown reason";
	  ulog (LOG_ERROR, "Can't receive %s: %s", qcmd->zfrom, zerr);
	  (void) fsysdep_did_work (qcmd->pseq);
	  (void) ffileclose (e);
	  return TRUE;
	}
      
      /* The mode should have been sent as "RY 0%o".  If it wasn't,
	 we use 0666.  */
      imode = (unsigned int) strtol (zrec + 2, (char **) NULL, 8);
      if (imode == 0)
	imode = 0666;
    }
  else
    {
      /* Tell the other system to go ahead and send.  */

      if (! fgsend_cmd ("SY"))
	{
	  (void) ffileclose (e);
	  return FALSE;
	}
      imode = qcmd->imode;
    }

  if (! fstore_recfile (e, qcmd->pseq, qcmd->zfrom, qcmd->zto, zfromsys,
			qcmd->zuser, imode, zmail, qcmd->ztemp))
    return FALSE;

  cGreceived_bytes = 0;
  fGreceived_error = FALSE;

  return fgmain_loop ();
}

/* Confirm that a file was received correctly.  */

static boolean
fgrecfile_confirm ()
{
  boolean fok;

  if (freceived_file (TRUE, cGreceived_bytes))
    fok = fgsend_cmd ("CY");
  else
    fok = fgsend_cmd ("CN5");
  if (! fok)
    return FALSE;

  /* Wait until the packet is received and acknowledged.  */

  while (INEXTSEQ (iGremote_ack) != iGsendseq)
    {
      if (! fgwait_for_packet (TRUE, cGtimeout, cGretries))
	return FALSE;
    }

  return TRUE;
}

/* Send a transfer request.  This is only called by the master.  It
   ignored the pseq entry in the scmd structure.  */

boolean
fgxcmd (qcmd)
     const struct scmd *qcmd;
{
  int clen;
  char *zsend;
  const char *zrec;

  /* We send the string
     X from to user options
     We put a dash in front of options.  */
      
  clen = (strlen (qcmd->zfrom) + strlen (qcmd->zto)
	  + strlen (qcmd->zuser) + strlen (qcmd->zoptions) + 7);
  zsend = (char *) alloca (clen);

  sprintf (zsend, "X %s %s %s -%s", qcmd->zfrom, qcmd->zto,
	   qcmd->zuser, qcmd->zoptions);

  if (! fgsend_cmd (zsend))
    return FALSE;

  /* Wait for a reply.  */

  zrec = zgget_cmd ();
  if (zrec == NULL)
    return FALSE;

  if (zrec[0] != 'X'
      || (zrec[1] != 'Y' && zrec[1] != 'N'))
    {
      ulog (LOG_ERROR, "Bad response to wildcard request");
      return FALSE;
    }

  if (zrec[1] == 'N')
    {
      ulog (LOG_ERROR, "Work request denied");
      return TRUE;
    }
  
  return TRUE;
}

/* Confirm a transfer request.  */

boolean
fgxcmd_confirm ()
{
  return fgsend_cmd ("XY");
}

/* Get and parse a command from the other system.  Handle hangups
   specially.  */

boolean
fggetcmd (fmaster, qcmd)
     boolean fmaster;
     struct scmd *qcmd;
{
  static char *z;
  static int c;

  while (TRUE)
    {
      const char *zcmd;
      int clen;

      zcmd = zgget_cmd ();
      if (zcmd == NULL)
	return FALSE;

      clen = strlen (zcmd);
      if (clen + 1 > c)
	{
	  c = clen + 1;
	  z = (char *) xrealloc ((pointer) z, c);
	}
      strcpy (z, zcmd);

      if (! fparse_cmd (z, qcmd))
	continue;

      /* Handle hangup commands specially.  If it's just 'H', return
	 it.  If it's 'N', the other side is denying a hangup request
	 which we can just ignore (since the top level code assumes
	 that hangup requests are denied).  If it's 'Y', the other
	 side is confirming a hangup request.  In this case we confirm
	 with an "HY", wait for yet another "HY" from the other side,
	 and then finally shut down the protocol (I don't know why it
	 works this way, but it does).  We then return a 'Y' command
	 to the top level code.  */

      if (qcmd->bcmd == 'N')
	{
#if DEBUG > 0
	  if (fmaster)
	    ulog (LOG_ERROR, "Got hangup reply as master");
#endif
	  continue;
	}

      if (qcmd->bcmd == 'Y')
	{
#if DEBUG > 0
	  if (fmaster)
	    ulog (LOG_ERROR, "Got hangup reply as master");
#endif
	  /* Don't check errors rigorously here, since the other side
	     might jump the gun and hang up.  */

	  if (! fgsend_cmd ("HY"))
	    return TRUE;
	  fGclose_ok = TRUE;
	  zcmd = zgget_cmd ();
	  fGclose_ok = FALSE;
	  if (zcmd == NULL)
	    return TRUE;
	  if (strcmp (zcmd, "HY") != 0)
	    ulog (LOG_ERROR, "Got \"%s\" when expecting \"HY\"",
		  zcmd);
	  (void) fgsend_control (CLOSE, 0);
	  return TRUE;
	}

      return TRUE;
    }

  /*NOTREACHED*/
}

/* Make a hangup request.  */

boolean
fghangup ()
{
  return fgsend_cmd ("H");
}

/* Reply to a hangup request.  This is only called by the slave.  If
   fconfirm is TRUE, we are closing down the protocol.  We send an HY
   message.  The master responds with an HY message.  We send another
   HY message, and then shut down the protocol.  */

boolean
fghangup_reply (fconfirm)
     boolean fconfirm;
{
  if (! fconfirm)
    return fgsend_cmd ("HN");
  else
    {
      const char *z;

      if (! fgsend_cmd ("HY"))
	return FALSE;

      z = zgget_cmd ();
      if (z == NULL)
	return FALSE;
      if (strcmp (z, "HY") != 0)
	ulog (LOG_ERROR, "Got \"%s\" when expecting \"HY\"", z);
      else
	{
	  if (! fgsend_cmd ("HY"))
	    return FALSE;
	}

      return fgsend_control (CLOSE, 0);
    }
}

/* Shut down the protocol on error.  */

boolean
fgshutdown ()
{
  (void) fgsend_control (CLOSE, 0);
  return fgsend_control (CLOSE, 0);
}

/* Signal a file transfer failure to the other side.  This is only called
   by the slave.  */

boolean
fgfail (bcmd, twhy)
     int bcmd;
     enum tfailure twhy;
{
  const char *z;

  switch (bcmd)
    {
    case 'S':
      switch (twhy)
	{
	case FAILURE_PERM:
	  z = "SN2";
	  break;
	case FAILURE_OPEN:
	  z = "SN4";
	  break;
	case FAILURE_SIZE:
	  z = "SN6";
	  break;
	default:
	  z = "SN";
	  break;
	}
      break;
    case 'R':
      switch (twhy)
	{
	case FAILURE_PERM:
	case FAILURE_OPEN:
	  z = "RN2";
	  break;
	case FAILURE_SIZE:
	  z = "RN6";
	  break;
	default:
	  z = "RN";
	  break;
	}
      break;
    case 'X':
      z = "XN";
      break;
    default:
#if DEBUG > 0
      ulog (LOG_ERROR, "fgfail: Can't happen");
#endif
      return FALSE;
    }
  
  if (! fgsend_cmd (z))
    return FALSE;

  /* Wait until the packet is received and acknowledged.  */

  while (INEXTSEQ (iGremote_ack) != iGsendseq)
    {
      if (! fgwait_for_packet (TRUE, cGtimeout, cGretries))
	return FALSE;
    }

  return TRUE;
}

/* Check whether we are ready for another command.  We are ready if we
   are not in the process of sending or receiving a file.  */

boolean
fgready ()
{
  return ! ffileisopen (eSendfile) && ! ffileisopen (eRecfile);
}

/* The main loop of the 'g' protocol sends data from the current send
   file and receives data into the current receive file (actually,
   since 'g' is half-duplex, there can't be both a send and a receive
   file, but we pretend that it could happen).

   If a packet is received when there is no receive file, we exit out
   of the main loop to permit the packet to be handled at a higher
   level.  If we finish sending a file, we exit out to permit a higher
   level to decide what to send next.  */

static boolean
fgmain_loop ()
{
#if DEBUG > 7
  if (iDebug > 7)
    ulog (LOG_DEBUG, "fgmain_loop: Main 'g' protocol loop");
#endif

  if (ffileisopen (eSendfile))
    {
      int iend;

      iend = iGrecend;

      while (TRUE)
	{
	  int clen;
	  boolean ferr;

	  /* We keep sending out packets until we have something
	     in the receive buffer.  */
	  while (iend == iGrecend)
	    {
	      char *zpacket, *zdata;
	      int cframe;

	      /* Get a packet and fill it with data.  */

	      zpacket = zgget_send_packet ();
	      zdata = zpacket + CFRAMELEN;

	      clen = cfileread (eSendfile, zdata, iGremote_packsize);
	      if (ffilereaderror (eSendfile, clen))
		{
		  /* The protocol gives us no way to report a file
		     sending error, so we just drop the connection.
		     What else can we do?  */
		  ulog (LOG_ERROR, "read: %s", strerror (errno));
		  usendfile_error ();
		  return FALSE;
		}

	      /* We may have to move the data, alas.  How can we avoid
		 this?  Moreover, if we are going to have to move the
		 data, we really should do a checksum calculation at
		 the same time.  */
	      cframe = cgframelen (clen);
	      if (cframe != CFRAMELEN && clen != 0)
		xmemmove (zpacket + cframe, zdata, clen);

	      if (! fgsend_packet (zpacket, clen))
		return FALSE;

	      cGsent_bytes += clen;

	      /* If we have reached the end of the file, wait for
		 confirmation and return out to get the next file.  */
	      if (clen == 0)
		return fgsendfile_confirm ();
	    }

	  /* Process the data in the receive buffer.  The
	     fgprocess_data function will return TRUE when it either
	     finished receiving a file or gets a complete command
	     packet.  Either way we let the higher level functions
	     decide what to do next.  Neither possibility can actually
	     happen in the normal half-duplex 'g' protocol.  */
	  if (fgprocess_data (FALSE, FALSE, &ferr, (int *) NULL,
			      (boolean *) NULL))
	    return TRUE;
	  if (ferr)
	    return FALSE;

	  iend = iGrecend;
	}
    }

#if DEBUG > 0
  /* If there is no file to send, there really should be a file to
     receive.  */

  if (! ffileisopen(eRecfile))
    ulog (LOG_FATAL, "fgmain_loop: No send or receive file");
#endif

  /* We have no file to send.  Wait for packets to come in.  The
     fgwait_for_packet function will only return when fgprocess_data
     returns TRUE, which will happen when the file has been completely
     received.  */

  return fgwait_for_packet (FALSE, cGtimeout, cGretries);
}

/* Send a command string.  This is actually just an interface to
   fgsend_packet, but if this were a full-duplex protocol it would
   have to be different.  We send packets containing the string until
   the entire string has been sent.  Each packet is full.  We make
   sure the last byte of the last packet is '\0', since that is what
   Ultrix UUCP seems to require.  */

static boolean
fgsend_cmd (z)
     const char *z;
{
  int clen;
  boolean fagain;

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgsend_cmd: Sending command \"%s\"", z);
#endif

  clen = strlen (z);

  do
    {
      char *zpacket;

      zpacket = zgget_send_packet ();

      if (clen < iGremote_packsize)
	{
	  strcpy (zpacket + CFRAMELEN, z);
	  zpacket[CFRAMELEN + iGremote_packsize - 1] = '\0';
	  fagain = FALSE;
	}
      else
	{
	  memcpy (zpacket + CFRAMELEN, z, iGremote_packsize);
	  z += iGremote_packsize;
	  clen -= iGremote_packsize;
	  fagain = TRUE;
	}

      if (! fgsend_packet (zpacket, iGremote_packsize))
	return FALSE;
    }
  while (fagain);

  return TRUE;
}

/* This function is called by the packet receive function (via
   fggot_data) when a command string is received.  We must queue up
   received commands since we don't know when we'll be able to get to
   them (for example, the acknowledgements for the last few packets of
   a sent file may contain the string indicating whether the file was
   received correctly).  */

struct scmdqueue
{
  struct scmdqueue *qnext;
  int csize;
  int clen;
  char *z;
};

static struct scmdqueue *qGcmd_queue;
static struct scmdqueue *qGcmd_free;

static void
ugadd_cmd (z, clen, flast)
     const char *z;
     int clen;
     boolean flast;
{
  struct scmdqueue *q;

  q = qGcmd_free;
  if (q == NULL)
    {
      q = (struct scmdqueue *) xmalloc (sizeof (struct scmdqueue));
      q->qnext = NULL;
      q->csize = 0;
      q->clen = 0;
      q->z = NULL;
      qGcmd_free = q;
    }

  if (q->clen + clen + 1 > q->csize)
    {
      q->csize = q->clen + clen + 1;
      q->z = (char *) xrealloc ((pointer) q->z, q->csize);
    }

  memcpy (q->z + q->clen, z, clen);
  q->clen += clen;
  q->z[q->clen] = '\0';

  /* If the last string in this command, add it to the queue of
     finished commands.  */

  if (flast)
    {
      struct scmdqueue **pq;

      for (pq = &qGcmd_queue; *pq != NULL; pq = &(*pq)->qnext)
	;
      *pq = q;
      qGcmd_free = q->qnext;
      q->qnext = NULL;
    }
}

/* Get a command string.  We just have to wait until the receive
   packet function gives us something in qGcmd_queue.  The return
   value of this may be treated as a static buffer; it will last
   at least until the next packet is received.  */

static const char *
zgget_cmd ()
{
  struct scmdqueue *q;

  while (qGcmd_queue == NULL)
    {
#if DEBUG > 4
      if (iDebug > 4)
	ulog (LOG_DEBUG, "zgget_cmd: Waiting for packet");
#endif

      /* If we are receiving a file, then this may return once when
	 the file has been completely filled without setting
	 qGcmd_queue.  The second time it returns qGcmd_queue will
	 certainly be set.  */
      if (! fgwait_for_packet (FALSE, cGtimeout, cGretries))
	return NULL;
    }

  q = qGcmd_queue;
  qGcmd_queue = q->qnext;

  q->clen = 0;

  /* We must not replace qGcmd_free, because it may already be
     receiving a new command string.  */
  if (qGcmd_free == NULL)
    {
      q->qnext = NULL;
      qGcmd_free = q;
    }
  else
    {
      q->qnext = qGcmd_free->qnext;
      qGcmd_free->qnext = q;
    }

  return q->z;
}

/* This function is called when a data packet is received.  If there
   is a file to receive data into, we write the data directly to it.
   Otherwise this must be a command of some sort, and we add it to a
   queue of commands.  This function returns TRUE if it can accept
   more data, FALSE if it would rather not; this return value is just
   a heuristic that fgprocess_data uses to decide whether to try to
   process more information or just return out.  If the function
   returns FALSE and *pferr is TRUE, it got an error.  */

static boolean
fggot_data (zfirst, cfirst, zsecond, csecond, pferr)
     const char *zfirst;
     int cfirst;
     const char *zsecond;
     int csecond;
     boolean *pferr;
{
  *pferr = FALSE;

  if (ffileisopen (eRecfile))
    {
      if (cfirst == 0)
	{
	  if (! fgrecfile_confirm ())
	    {
	      *pferr = TRUE;
	      return FALSE;
	    }
	  return FALSE;
	}
      else
	{
	  int cwrote;

	  do
	    {
	      /* Cast zfirst to avoid warnings because of erroneous
		 prototypes on Ultrix.  */
	      cwrote = cfilewrite (eRecfile, (char *) zfirst, cfirst);
	      if (cwrote != cfirst && ! fGreceived_error)
		{
		  if (cwrote < 0)
		    ulog (LOG_ERROR, "write: %s", strerror (errno));
		  else
		    ulog (LOG_ERROR, "write of %d wrote only %d",
			  cfirst, cwrote);
		  urecfile_error ();
		  fGreceived_error = TRUE;
		}

	      cGreceived_bytes += cfirst;

	      zfirst = zsecond;
	      cfirst = csecond;
	      csecond = 0;
	    }
	  while (cfirst != 0);

	  return TRUE;
	}
    }

  /* We want to add this data to the current command string.  If there
     is not null character in the data, this string will be continued
     by the next packet.  Otherwise this must be the last string in
     the command, and we don't care about what comes after the null
     byte.  */

  do
    {
      const char *z;

      z = (const char *) memchr ((constpointer) zfirst, '\0', cfirst);
      if (z == NULL)
	ugadd_cmd (zfirst, cfirst, FALSE);
      else
	{
	  ugadd_cmd (zfirst, z - zfirst, TRUE);
	  return FALSE;
	}

      zfirst = zsecond;
      cfirst = csecond;
      csecond = 0;
    }
  while (cfirst != 0);

  return TRUE;
}

/* We keep an array of buffers to retransmit as necessary.  Rather
   than waste static space on large buffer sizes, we allocate the
   buffers once we know how large the other system expects them to be.
   The sequence numbers used in the 'g' protocol are only three bits
   long, so we allocate eight buffers and maintain a correspondence
   between buffer index and sequence number.  This always wastes some
   buffer space, but it's easy to implement.  */

#define CSENDBUFFERS (CMAXWINDOW + 1)

static char *azGsendbuffers[CSENDBUFFERS];

static boolean
fginit_sendbuffers ()
{
  int i;

  /* Free up any remaining old buffers.  */

  for (i = 0; i < CSENDBUFFERS; i++)
    {
      xfree ((pointer) azGsendbuffers[i]);
      azGsendbuffers[i] = (char *) malloc (CFRAMELEN + iGremote_packsize);
      if (azGsendbuffers[i] == NULL)
	return FALSE;
    }
  return TRUE;
}

/* Allocate a packet to send out.  The return value of this function
   must be filled in and passed to fgsend_packet, or discarded.  This
   will ensure that the buffers and iGsendseq stay in synch.  */

static char *
zgget_send_packet ()
{
  return azGsendbuffers[iGsendseq];
}

/* Send out a data packet.  This computes the checksum, sets up the
   header, and sends the packet out.  The argument should point to one
   of the send buffers allocated above, and the data should be in the
   buffer starting at z + cgframelen (clen).  The packet will be of
   size iGremote_packsize.  */

static boolean
fgsend_packet (z, c)
     char *z;
     int c;
{
  int itt;
  unsigned short icheck;

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgsend_packet: Sending %d bytes", c);
#endif

  /* Set the initial length bytes.  See the description at the definition
     of SHORTDATA, above.  */

  if (c == iGremote_packsize)
    itt = DATA;
#if DEBUG > 0
  else if (c > iGremote_packsize)
    {
      ulog (LOG_FATAL, "fgsend_packet: Packet size too large");
      /* Avoid an uninitialized warning.  */
      itt = DATA;
    }
#endif
  else
    {
      int cshort;

      itt = SHORTDATA;
      cshort = iGremote_packsize - c;
      if (cshort <= 127)
	z[CFRAMELEN] = (char) cshort;
      else
	{
	  z[CFRAMELEN] = (char) (0x80 | (cshort & 0x7f));
	  z[CFRAMELEN + 1] = (char) (cshort >> 7);
	}
    }

  z[IFRAME_DLE] = DLE;
  z[IFRAME_K] = (char) (iGremote_segsize + 1);

  icheck = (unsigned short) igchecksum (z + CFRAMELEN, iGremote_packsize);

  /* We're just about ready to go.  Wait until there is room in the
     receiver's window for us to send the packet.  We do this now so
     that we send the correct value for the last packet received.
     Note that if iGsendseq == iGremote_ack, this means that the
     sequence numbers are actually 8 apart, since the packet could not
     have been acknowledged before it was sent; this can happen when
     the window size is 7.  */
  while (iGsendseq == iGremote_ack
	 || CSEQDIFF (iGsendseq, iGremote_ack) > iGremote_winsize)
    {
      if (! fgwait_for_packet (TRUE, cGtimeout, cGretries))
	return FALSE;
    }

  /* Ack all packets up to the next one, since the UUCP protocol
     requires that all packets be acked in order.  */
  while (CSEQDIFF (iGrecseq, iGlocal_ack) > 1)
    {
      iGlocal_ack = INEXTSEQ (iGlocal_ack);
      if (! fgsend_control (RR, iGlocal_ack))
	return FALSE;
    }
  iGlocal_ack = iGrecseq;

  z[IFRAME_CONTROL] = (char) ((itt << 6) | (iGsendseq << 3) | iGrecseq);

  iGsendseq = INEXTSEQ (iGsendseq);

  icheck = ((unsigned short)
	    ((0xaaaa - (icheck ^ (z[IFRAME_CONTROL] & 0xff))) & 0xffff));
  z[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  z[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  z[IFRAME_XOR] = (char) (z[IFRAME_K] ^ z[IFRAME_CHECKLOW]
			  ^ z[IFRAME_CHECKHIGH] ^ z[IFRAME_CONTROL]);

  /* If we've retransmitted a packet, but it hasn't been acked yet,
     and this isn't the next packet after the retransmitted one (note
     that iGsendseq has already been incremented at this point) then
     don't send this packet yet.  The other side is probably not ready
     for it yet.  Instead, code in fgprocess_data will send the
     outstanding packets when an ack is received.  */

  if (iGretransmit_seq != -1
      && INEXTSEQ (INEXTSEQ (iGretransmit_seq)) != iGsendseq)
    return TRUE;

  return fgoutput (z, CFRAMELEN + iGremote_packsize);
}

/* Recompute the control byte and checksum of a packet so that it
   includes the correct packet acknowledgement.  This is called
   when a packet is retransmitted to make sure the retransmission
   does not confuse the other side.  */

static void
ugadjust_ack (iseq)
     int iseq;
{
  char *z;
  unsigned short icheck;

  z = azGsendbuffers[iseq];

  /* If the received packet number is the same, there is nothing
     to do.  */
  if (CONTROL_YYY (z[IFRAME_CONTROL]) == iGrecseq)
    return;

  /* Get the old checksum.  */
  icheck = (unsigned short) (((z[IFRAME_CHECKHIGH] & 0xff) << 8)
			     | (z[IFRAME_CHECKLOW] & 0xff));
  icheck = ((unsigned short)
	    (((0xaaaa - icheck) ^ (z[IFRAME_CONTROL] & 0xff)) & 0xffff));

  /* Update the control byte.  */
  z[IFRAME_CONTROL] = (char) ((z[IFRAME_CONTROL] &~ 07) | iGrecseq);

  /* Create the new checksum.  */
  icheck = ((unsigned short)
	    ((0xaaaa - (icheck ^ (z[IFRAME_CONTROL] & 0xff))) & 0xffff));
  z[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  z[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  /* Update the XOR byte.  */
  z[IFRAME_XOR] = (char) (z[IFRAME_K] ^ z[IFRAME_CHECKLOW]
			  ^ z[IFRAME_CHECKHIGH] ^ z[IFRAME_CONTROL]);
}

/* Send a control packet.  These are fairly simple to construct.  It
   seems reasonable to me that we should be able to send a control
   packet at any time, even if the receive window is closed.  In
   particular, we don't want to delay when sending a CLOSE control
   message.  If I'm wrong, it can be changed easily enough.  */

static boolean
fgsend_control (ixxx, iyyy)
     int ixxx;
     int iyyy;
{
  char ab[CFRAMELEN];
  int ictl;
  unsigned short icheck;

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgsend_control: Sending control %d, %d",
	  ixxx, iyyy);
#endif

  ab[IFRAME_DLE] = DLE;
  ab[IFRAME_K] = KCONTROL;

  ictl = (CONTROL << 6) | (ixxx << 3) | iyyy;
  icheck = (unsigned short) (0xaaaa - ictl);
  ab[IFRAME_CHECKLOW] = (char) (icheck & 0xff);
  ab[IFRAME_CHECKHIGH] = (char) (icheck >> 8);

  ab[IFRAME_CONTROL] = (char) ictl;

  ab[IFRAME_XOR] = (char) (ab[IFRAME_K] ^ ab[IFRAME_CHECKLOW]
			   ^ ab[IFRAME_CHECKHIGH] ^ ab[IFRAME_CONTROL]);

  return fgoutput (ab, CFRAMELEN);
}

/* Return the length of the frame header that will be required for data
   of a certain length.  See the information by the definition of
   SHORTDATA, above.  */

static int
cgframelen (c)
     int c;
{
#if DEBUG > 0
  if (c > iGremote_packsize)
    ulog (LOG_FATAL, "cgframelen: Packet size too large");
#endif

  if (c == iGremote_packsize)
    return CFRAMELEN;
  else if (iGremote_packsize - c <= 127)
    return CFRAMELEN + 1;
  else
    return CFRAMELEN + 2;
}

/* We want to output and input at the same time, if supported on this
   machine.  If we have something to send, we send it all while
   accepting a large amount of data.  Once we have sent everything we
   look at whatever we have received.  This implementation is simple,
   but it does some unnecessary memory moves which may make it too
   slow.  If data comes in faster than we can send it, we may run out
   of buffer space.  */

static boolean
fgoutput (zsend, csend)
     const char *zsend;
     int csend;
{
  while (csend > 0)
    {
      char *zrec;
      int crec, csent;

      if (iGrecend < iGrecstart)
	{
	  zrec = abGrecbuf + iGrecend;
	  crec = iGrecstart - iGrecend - 1;
	}
      else if (iGrecend < CRECBUFLEN)
	{
	  zrec = abGrecbuf + iGrecend;
	  crec = CRECBUFLEN - iGrecend;
	}
      else
	{
	  zrec = abGrecbuf;
	  crec = iGrecstart - 1;
	}

      csent = csend;

#if DEBUG > 8
      if (iDebug > 8)
	ulog (LOG_DEBUG, "fgoutput: iGrecstart %d; iGrecend %d; crec %d",
	      iGrecstart, iGrecend, crec);
#endif

      if (! fport_io (zsend, &csent, zrec, &crec))
	return FALSE;

      csend -= csent;
      zsend += csent;

      iGrecend = (iGrecend + crec) % CRECBUFLEN;
    }

  return TRUE;
}

/* Get a packet.  This is called when we have nothing to send, but
   want to wait for a packet to come in.

   freturncontrol -- if TRUE, return after getting a control packet
   ctimeout -- timeout in seconds
   cretries -- number of times to retry timeout.

   This function returns TRUE when a packet comes in, or FALSE if
   cretries timeouts of ctimeout seconds were exceeded.  */

static boolean
fgwait_for_packet (freturncontrol, ctimeout, cretries)
     boolean freturncontrol;
     int ctimeout;
     int cretries;
{
  boolean ferr;
  boolean ffound;
  int cneed;
  int ctimeouts;
  int cgarbage;
  int cshort;

  ferr = FALSE;
  ctimeouts = 0;
  cgarbage = 0;
  cshort = 0;

  while (! fgprocess_data (TRUE, freturncontrol, &ferr, &cneed, &ffound))
    {
      char *zrec;
      int crec;
  
      if (ferr)
	return FALSE;

#if DEBUG > 8
      if (iDebug > 8)
	ulog (LOG_DEBUG, "fgwait_for_packet: Need %d bytes", cneed);
#endif

      if (ffound)
	cgarbage = 0;
      else
	{
	  if (cgarbage > cGgarbage_data)
	    {
	      ulog (LOG_ERROR, "Too much unrecognized data");
	      return FALSE;
	    }
	}

      if (iGrecend < iGrecstart)
	{
	  zrec = abGrecbuf + iGrecend;
	  crec = iGrecstart - iGrecend - 1;
	}
      else if (iGrecend < CRECBUFLEN)
	{
	  zrec = abGrecbuf + iGrecend;
	  crec = CRECBUFLEN - iGrecend;
	}
      else
	{
	  zrec = abGrecbuf;
	  crec = iGrecstart - 1;
	}
  
#if DEBUG > 8
      if (iDebug > 8)
	ulog (LOG_DEBUG,
	      "fgwait_for_packet: iGrecstart %d; iGrecend %d; crec %d",
	      iGrecstart, iGrecend, crec);
#endif

      if (crec < cneed)
	cneed = crec;

      if (! fport_read (zrec, &crec, cneed, ctimeout, TRUE))
	return FALSE;

      cgarbage += crec;

      if (crec != 0)
	{
	  ctimeouts = 0;

	  /* If we don't get enough data twice in a row, we may have
	     dropped some data and still be looking for the end of a
	     large packet.  Incrementing iGrecstart will force
	     fgprocess_data to skip that packet and look through the
	     rest of the data.  In some situations, this will be a
	     mistake.  */
	  if (crec >= cneed)
	    cshort = 0;
	  else
	    {
	      ++cshort;
	      if (cshort > 1)
		{
		  iGrecstart = (iGrecstart + 1) % CRECBUFLEN;
		  cshort = 0;
		}
	    }
	}
      else
	{
	  /* The read timed out.  If we're looking for a control
	     packet, assume we're looking for an ack and send the last
	     unacknowledged packet again.  Otherwise, send an RJ with
	     the last packet we received correctly.  */

	  ++ctimeouts;
	  if (ctimeouts > cretries)
	    {
	      if (cretries > 0)
		ulog (LOG_ERROR, "Timed out waiting for packet");
	      return FALSE;
	    }

	  if (freturncontrol
	      && INEXTSEQ (iGremote_ack) != iGsendseq)
	    {
	      int inext;

	      inext = INEXTSEQ (iGremote_ack);
	      ugadjust_ack (inext);
	      if (! fgoutput (azGsendbuffers[inext],
			      CFRAMELEN + iGremote_packsize))
		return FALSE;
	      iGretransmit_seq = inext;
	    }
	  else
	    {
	      /* Send all pending acks first, to avoid confusing
		 the other side.  */
	      if (iGlocal_ack != iGrecseq)
		{
		  if (! fgsend_acks ())
		    return FALSE;
		}
	      if (! fgsend_control (RJ, iGrecseq))
		return FALSE;
	    }
	}

      iGrecend = (iGrecend + crec) % CRECBUFLEN;
    }

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgwait_for_packet: Got packet %d, %d, %d",
	  CONTROL_TT (iGpacket_control), CONTROL_XXX (iGpacket_control),
	  CONTROL_YYY (iGpacket_control));
#endif

  return TRUE;
}

/* Send acks for all packets we haven't acked yet.  */

static boolean
fgsend_acks ()
{
  while (iGlocal_ack != iGrecseq)
    {
      iGlocal_ack = INEXTSEQ (iGlocal_ack);
      if (! fgsend_control (RR, iGlocal_ack))
	return FALSE;
    }
  return TRUE;
}

/* Handle an ack of a packet.  According to Hanrahan's paper, this
   acknowledges all previous packets.  If this is an ack for a
   retransmitted packet, continue by resending up to two more packets
   following the retransmitted one.  This should recover quickly from
   a line glitch, while avoiding the problem of continual
   retransmission.  */

static boolean
fggot_ack (iack)
     int iack;
{
  int inext;

  iGremote_ack = iack;

  if (iack != iGretransmit_seq)
    return TRUE;

  inext = INEXTSEQ (iGretransmit_seq);
  if (inext == iGsendseq)
    iGretransmit_seq = -1;
  else
    {
      ugadjust_ack (inext);
      if (! fgoutput (azGsendbuffers[inext],
		      CFRAMELEN + iGremote_packsize))
	return FALSE;
      inext = INEXTSEQ (inext);
      if (inext == iGsendseq)
	iGretransmit_seq = -1;
      else
	{
	  ugadjust_ack (inext);
	  if (! fgoutput (azGsendbuffers[inext],
			  CFRAMELEN + iGremote_packsize))
	    return FALSE;
	  iGretransmit_seq = inext;
	}
    }

  return TRUE;
}

/* Process the receive buffer into a data packet, if possible.  All
   control packets are handled here.  This function returns TRUE if it
   got a packet, FALSE otherwise.  When a data packet is received,
   fgprocess_data calls fggot_data; if that returns TRUE the function
   will continue trying to process data (unless the freturncontrol
   argument is TRUE, in which case fgprocess_data will return
   immediately).  If fggot_data returns FALSE (meaning that no more
   data is needed), fgprocess_data will return TRUE (meaning that a
   packet was received).  If some error occurs, *pferr will be set to
   TRUE and the function will return FALSE.  If there is not enough
   data to form a complete packet, then if the pcneed argument is not
   NULL *pcneed will be set to the number of bytes needed to form a
   complete packet, and fgprocess_data will return FALSE.  If this
   function found a packet, and pffound is not NULL, it will set
   *pffound to TRUE; this can be used to tell valid control packets
   from an endless stream of garbage.  */

static boolean
fgprocess_data (fdoacks, freturncontrol, pferr, pcneed, pffound)
     boolean fdoacks;
     boolean freturncontrol;
     boolean *pferr;
     int *pcneed;
     boolean *pffound;
{
  *pferr = FALSE;

#if DEBUG > 7
  if (iDebug > 7)
    ulog (LOG_DEBUG, "fgprocess_data: iGrecstart %d; iGrecend %d",
	  iGrecstart, iGrecend);
#endif

  for (; iGrecstart != iGrecend; iGrecstart = (iGrecstart + 1) % CRECBUFLEN)
    {
      char ab[CFRAMELEN];
      int i, iget, cwant;
      unsigned short ihdrcheck, idatcheck;
      const char *zfirst, *zsecond;
      int cfirst, csecond;

      /* Look for the DLE which must start a packet.  */

      if (abGrecbuf[iGrecstart] != DLE)
	continue;

      /* Get the first six bytes into ab.  */

      for (i = 0, iget = iGrecstart;
	   i < CFRAMELEN && iget != iGrecend;
	   i++, iget = (iget + 1) % CRECBUFLEN)
	ab[i] = abGrecbuf[iget];

      /* If there aren't six bytes, there is no packet.  */

      if (i < CFRAMELEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CFRAMELEN - i;
	  return FALSE;
	}

      /* Make sure these six bytes start a packet.  If they don't,
	 loop around to bump iGrecstart and look for another DLE.  */

      if (ab[IFRAME_DLE] != DLE
	  || ab[IFRAME_K] < 1
	  || ab[IFRAME_K] > 9
	  || ab[IFRAME_XOR] != (ab[IFRAME_K] ^ ab[IFRAME_CHECKLOW]
				^ ab[IFRAME_CHECKHIGH] ^ ab[IFRAME_CONTROL])
	  || CONTROL_TT (ab[IFRAME_CONTROL]) == ALTCHAN)
	continue;

      /* The zfirst and cfirst pair point to the first set of data for
	 this packet; the zsecond and csecond point to the second set,
	 in case the packet wraps around the end of the buffer.  */
      zfirst = abGrecbuf + iGrecstart + CFRAMELEN;
      cfirst = 0;
      zsecond = NULL;
      csecond = 0;

      if (ab[IFRAME_K] == KCONTROL)
	{
	  /* This is a control packet.  It should not have any data.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    continue;

	  idatcheck = (unsigned short) (0xaaaa - ab[IFRAME_CONTROL]);
	  cwant = 0;
	}
      else
	{
	  int cinbuf;
	  unsigned short icheck;

	  /* This is a data packet.  It should not be type CONTROL.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) == CONTROL)
	    continue;

	  if (iGrecend >= iGrecstart)
	    cinbuf = iGrecend - iGrecstart;
	  else
	    cinbuf = CRECBUFLEN - (iGrecstart - iGrecend);
	  cinbuf -= CFRAMELEN;

	  /* Make sure we have enough data.  If we don't, wait for
	     more.  */	     

	  cwant = 1 << (ab[IFRAME_K] + 4);
	  if (cinbuf < cwant)
	    {
	      if (pcneed != NULL)
		*pcneed = cwant - cinbuf;
	      return FALSE;
	    }
	  
	  /* Set up the data pointers and compute the checksum.  */

	  if (iGrecend >= iGrecstart)
	    cfirst = cwant;
	  else
	    {
	      cfirst = CRECBUFLEN - (iGrecstart + CFRAMELEN);
	      if (cfirst >= cwant)
		cfirst = cwant;
	      else if (cfirst > 0)
		{
		  zsecond = abGrecbuf;
		  csecond = cwant - cfirst;
		}
	      else
		{
		  /* Here cfirst is non-positive, so subtracting from
		     abGrecbuf will actually skip the appropriate number
		     of bytes at the start of abGrecbuf.  */
		  zfirst = abGrecbuf - cfirst;
		  cfirst = cwant;
		}
	    }

	  if (csecond == 0)
	    icheck = (unsigned short) igchecksum (zfirst, cfirst);
	  else
	    icheck = (unsigned short) igchecksum2 (zfirst, cfirst,
						   zsecond, csecond);

	  idatcheck = ((unsigned short)
		       (((0xaaaa - (icheck ^ (ab[IFRAME_CONTROL] & 0xff)))
			 & 0xffff)));
	}
      
      ihdrcheck = (unsigned short) (((ab[IFRAME_CHECKHIGH] & 0xff) << 8)
				    | (ab[IFRAME_CHECKLOW] & 0xff));

      if (ihdrcheck != idatcheck)
	{
#if DEBUG > 4
	  if (iDebug > 4)
	    ulog (LOG_DEBUG,
		  "fgprocess_data: Checksum failed; expected 0x%x, got 0x%x",
		  ihdrcheck, idatcheck);
#endif

	  /* If the checksum failed for a data packet, then if it was
	     the one we were expecting send an RJ, otherwise
	     acknowledge the last packet received again.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    {
	      boolean facked;
	      
	      if (iGrecseq == iGlocal_ack)
		facked = FALSE;
	      else
		{
		  if (! fgsend_acks ())
		    {
		      *pferr = TRUE;
		      return FALSE;
		    }
		  facked = TRUE;
		}
	      if (CONTROL_XXX (ab[IFRAME_CONTROL]) == INEXTSEQ (iGrecseq))
		{
		  if (! fgsend_control (RJ, iGrecseq))
		    {
		      *pferr = TRUE;
		      return FALSE;
		    }
		}
	      else if (! facked)
		{
		  if (! fgsend_control (RR, iGrecseq))
		    {
		      *pferr = TRUE;
		      return FALSE;
		    }
		}  
	    }

	  /* We can't skip the packet data after this, because if we
	     have lost incoming bytes the next DLE will be somewhere
	     in what we thought was the packet data.  */
	  continue;
	}

      /* We have a packet; remove the processed bytes from the receive
	 buffer.  Note that if we go around the loop again after this
	 assignment, we must decrement iGrecstart first to account for
	 the increment which will be done by the loop control.  */
      iGrecstart = (iGrecstart + cwant + CFRAMELEN) % CRECBUFLEN;

      /* Store the control byte for the use of calling functions.  */
      iGpacket_control = ab[IFRAME_CONTROL] & 0xff;

      /* Tell the caller that we found something.  */
      if (pffound != NULL)
	*pffound = TRUE;

      /* Update the received sequence number from the yyy field of a
	 data packet or an RR control packet.  */

      if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL
	  || CONTROL_XXX (ab[IFRAME_CONTROL]) == RR)
	{
	  if (! fggot_ack (CONTROL_YYY (ab[IFRAME_CONTROL])))
	    {
	      *pferr = TRUE;
	      return FALSE;
	    }
	}

      /* If this isn't a control message, make sure we have received
	 the expected packet sequence number, acknowledge the packet
	 if it's the right one, and process the data.  */

      if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	{
	  if (CONTROL_XXX (ab[IFRAME_CONTROL]) != INEXTSEQ (iGrecseq))
	    {
#if DEBUG > 7
	      if (iDebug > 7)
		ulog (LOG_DEBUG, "fgprocess_data: Got packet %d; expected %d",
		      CONTROL_XXX (ab[IFRAME_CONTROL]),
		      INEXTSEQ (iGrecseq));
#endif

	      /* We got the wrong packet number.  Send an RR to
		 try to get us back in synch.  */

	      if (iGrecseq != iGlocal_ack)
		{
		  if (! fgsend_acks ())
		    {
		      *pferr = TRUE;
		      return FALSE;
		    }
		}
	      else
		{
		  if (! fgsend_control (RR, iGrecseq))
		    {
		      *pferr = TRUE;
		      return FALSE;
		    }
		}

	      /* As noted above, we must decrement iGrecstart because
		 the loop control will increment it.  */

	      --iGrecstart;
	      continue;
	    }

	  /* We got the packet we expected.  */

	  iGrecseq = INEXTSEQ (iGrecseq);

	  /* If we are supposed to do acknowledgements here, send back
	     an RR packet.  */

	  if (fdoacks)
	    {
	      if (! fgsend_acks ())
		{
		  *pferr = TRUE;
		  return FALSE;
		}
	    }

	  /* If this is a short data packet, adjust the data pointers
	     and lengths.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) == SHORTDATA)
	    {
	      int cshort, cmove;

	      if ((zfirst[0] & 0x80) == 0)
		{
		  cshort = zfirst[0] & 0xff;
		  cmove = 1;
		}
	      else
		{
		  int cbyte2;

		  if (cfirst > 1)
		    cbyte2 = zfirst[1] & 0xff;
		  else
		    cbyte2 = zsecond[0] & 0xff;
		  cshort = (zfirst[0] & 0x7f) + (cbyte2 << 7);
		  cmove = 2;
		}

#if DEBUG > 8
	      if (iDebug > 8)
		ulog (LOG_DEBUG,
		      "fgprocess_data: Short by %d (first %d, second %d)",
		      cshort, cfirst, csecond);
#endif

	      /* Adjust the start of the buffer for the bytes used
		 by the count.  */
	      if (cfirst > cmove)
		{
		  zfirst += cmove;
		  cfirst -= cmove;
		}
	      else
		{
		  zfirst = zsecond + (cmove - cfirst);
		  cfirst = csecond - (cmove - cfirst);
		  csecond = 0;
		}

	      /* Adjust the length of the buffer for the bytes we are
		 not supposed to consider.  */
	      cshort -= cmove;
	      if (csecond >= cshort)
		csecond -= cshort;
	      else
		{
		  cfirst -= cshort - csecond;
		  csecond = 0;
		}

	      /* This should not happen, but just in case.  */
	      if (cfirst < 0)
		cfirst = 0;
	    }

	  if (! fggot_data (zfirst, cfirst, zsecond, csecond, pferr))
	    {
	      /* If *pferr is TRUE, fggot_data got an error so we want
		 to return FALSE with *pferr set to TRUE.  Otherwise
		 fggot_data is telling us to get out without looking
		 at any more data.  */
	      if (*pferr)
		return FALSE;
	      else
		return TRUE;
	    }

	  /* If we've been asked to return control packets, get out
	     now.  */
	  if (freturncontrol)
	    return TRUE;

	  /* As noted above, we must decrement iGrecstart because
	     the loop control will increment it.  */

	  --iGrecstart;
	  continue;
	}

      /* Handle control messages here. */

      switch (CONTROL_XXX (ab[IFRAME_CONTROL]))
	{
	case CLOSE:
	  /* The other side has closed the connection.  */
	  if (! fGclose_ok)
	    ulog (LOG_ERROR, "Received unexpected CLOSE packet");
	  (void) fgsend_control (CLOSE, 0);
	  *pferr = TRUE;
	  return FALSE;
	case RJ:
	  /* The other side dropped a packet.  Begin retransmission with
	     the packet following the one acknowledged.  We don't
	     retransmit the packets immediately, but instead wait
	     for the first one to be acked.  This prevents us from
	     sending an entire window several times if we get several
	     RJ packets.  */
	  iGremote_ack = CONTROL_YYY (ab[IFRAME_CONTROL]);
	  iGretransmit_seq = INEXTSEQ (iGremote_ack);
	  if (iGretransmit_seq == iGsendseq)
	    iGretransmit_seq = -1;
	  else
	    {
	      ugadjust_ack (iGretransmit_seq);
	      if (! fgoutput (azGsendbuffers[iGretransmit_seq],
			      CFRAMELEN + iGremote_packsize))
		{
		  *pferr = TRUE;
		  return FALSE;
		}
	    }
	  break;
	case SRJ:
	  /* Selectively reject a particular packet.  This is not used
	     by UUCP, but it's easy to support.  */
	  ugadjust_ack (CONTROL_YYY (ab[IFRAME_CONTROL]));
	  if (! fgoutput (azGsendbuffers[CONTROL_YYY (ab[IFRAME_CONTROL])],
			  CFRAMELEN + iGremote_packsize))
	    {
	      *pferr = TRUE;
	      return FALSE;
	    }
	  break;
	case RR:
	  /* Acknowledge receipt of a packet.  This was already handled
	     above.  */
	  break;
	case INITC:
	case INITB:
	case INITA:
	  /* Ignore attempts to reinitialize.  */
	  break;
	}

      /* If we've been asked to return control packets, get out.  */
      if (freturncontrol)
	return TRUE;

      /* Loop around to look for the next packet, if any.  As noted
	 above, we must decrement iGrecstart since the loop control
	 will increment it.  */

      --iGrecstart;
    }

  /* There is no data left in the receive buffer.  */

  if (pcneed != NULL)
    *pcneed = CFRAMELEN;
  return FALSE;
}

/* Compute the 'g' protocol checksum.  This is unfortunately rather
   awkward.  This is the most time consuming code in the entire
   program.  It's also not a great checksum, since it can be fooled
   by some single bit errors.  */

static int
igchecksum (z, c)
     register const char *z;
     register int c;
{
  register unsigned int ichk1, ichk2;

  ichk1 = 0xffff;
  ichk2 = 0;

  do
    {
      register unsigned int b;

      /* Rotate ichk1 left.  */
      if ((ichk1 & 0x8000) == 0)
	ichk1 <<= 1;
      else
	{
	  ichk1 <<= 1;
	  ++ichk1;
	}

      /* Add the next character to ichk1.  */
      b = *z++ & 0xff;
      ichk1 += b;

      /* Add ichk1 xor the character position in the buffer counting from
	 the back to ichk2.  */
      ichk2 += ichk1 ^ c;

      /* If the character was zero, or adding it to ichk1 caused an
	 overflow, xor ichk2 to ichk1.  */
      if (b == 0 || (ichk1 & 0xffff) < b)
	ichk1 ^= ichk2;
    }
  while (--c > 0);

#if DEBUG > 8
  if (iDebug > 8)
    ulog (LOG_DEBUG, "igchecksum: Returning 0x%x", ichk1 & 0xffff);
#endif

  return ichk1 & 0xffff;
}

/* We use a separate function compute the checksum if the block is
   split around the end of the receive buffer since it occurs much
   less frequently and the checksum is already high up in the
   profiles.  These functions are almost identical, and this one
   actually only has a few more instructions in the inner loop.  */

static int
igchecksum2 (zfirst, cfirst, zsecond, csecond)
     const char *zfirst;
     int cfirst;
     const char *zsecond;
     int csecond;
{
  register unsigned int ichk1, ichk2;
  register const char *z;
  register int c;

  z = zfirst;
  c = cfirst + csecond;

  ichk1 = 0xffff;
  ichk2 = 0;

  do
    {
      register unsigned int b;

      /* Rotate ichk1 left.  */
      if ((ichk1 & 0x8000) == 0)
	ichk1 <<= 1;
      else
	{
	  ichk1 <<= 1;
	  ++ichk1;
	}

      /* Add the next character to ichk1.  */
      b = *z++ & 0xff;
      ichk1 += b;

      /* If the first buffer has been finished, switch to the second.  */
      --cfirst;
      if (cfirst == 0)
	z = zsecond;

      /* Add ichk1 xor the character position in the buffer counting from
	 the back to ichk2.  */
      ichk2 += ichk1 ^ c;

      /* If the character was zero, or adding it to ichk1 caused an
	 overflow, xor ichk2 to ichk1.  */
      if (b == 0 || (ichk1 & 0xffff) < b)
	ichk1 ^= ichk2;
    }
  while (--c > 0);

#if DEBUG > 8
  if (iDebug > 8)
    ulog (LOG_DEBUG, "igchecksum: Returning 0x%x", ichk1 & 0xffff);
#endif

  return ichk1 & 0xffff;
}
