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
   Revision 1.18  1992/01/16  23:46:40  ian
   Zero out unused bytes in short packets

   Revision 1.17  1992/01/16  18:16:14  ian
   Corrected misspelling in debugging message

   Revision 1.16  1992/01/07  17:11:15  ian
   Discount out of order packets in the error count

   Revision 1.15  1992/01/04  22:15:41  ian
   Mark Mallett: wait for INITx from master before sending our INITx

   Revision 1.14  1992/01/02  05:01:28  ian
   Count rejections separately from resent packets

   Revision 1.13  1991/12/31  21:17:32  ian
   Franc,ois Pinard: forgot to initialize cGdelayed_packets

   Revision 1.12  1991/12/30  04:07:13  ian
   Don't send RR packets when a failure occurs

   Revision 1.11  1991/12/28  03:49:23  ian
   Added HAVE_MEMFNS and HAVE_BFNS; changed uses of memset to bzero

   Revision 1.10  1991/12/27  06:01:55  ian
   Check for configurable maximum number of errors

   Revision 1.9  1991/12/20  03:02:01  ian
   Oleg Tabarovsky: added statistical messages to 'g' and 'f' protocols

   Revision 1.8  1991/12/17  22:13:14  ian
   David Nugent: zero out garbage before sending data

   Revision 1.7  1991/12/11  15:02:45  ian
   Tweaked initialization sequence slightly for better packet loss response

   Revision 1.6  1991/11/15  21:33:36  ian
   Remove unreached line

   Revision 1.5  1991/11/15  21:00:59  ian
   Efficiency hacks for 'f' and 't' protocols

   Revision 1.4  1991/11/11  00:39:45  ian
   Open port in seven bit mode, added fport_set to change to eight bit

   Revision 1.3  1991/11/09  18:53:07  ian
   Reworked protocol interface

   Revision 1.2  1991/11/08  04:07:04  ian
   Brian Campbell: made cGsent_bytes and cGreceived_bytes long, not int

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
#include "system.h"
#include "port.h"

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

/* Get the length of a packet given a pointer to the header.  */
#define CPACKLEN(z) (1 << ((z)[IFRAME_K] + 4))

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

/* Maximum number of errors we are prepared to see before giving up.  */
static int cGmax_errors = 100;

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
  { "errors", CMDTABTYPE_INT, (pointer) &cGmax_errors, NULL },
  { NULL, 0, NULL, NULL }
};

/* Statistics.  */

/* Number of packets we have sent.  */
static long cGsent_packets;

/* Number of packets we have resent (these are not included in
   cGsent_packets).  */
static long cGresent_packets;

/* Number of packets we have delayed sending (these should not be
   counted in cGresent_packets).  */
static long cGdelayed_packets;

/* Number of packets we have received.  */
static long cGrec_packets;

/* Number of packets rejected because the header was bad.  */
static long cGbad_hdr;

/* Number of packets rejected because the checksum was bad.  */
static long cGbad_checksum;

/* Number of packets received out of order.  */
static long cGbad_order;

/* Number of packets rejected by receiver (number of RJ packets
   received).  */
static long cGremote_rejects;

/* Local functions.  */

static boolean fgexchange_init P((boolean fmaster, int ictl, int ival,
				 int *piset));
static boolean fgsend_control P((int ictl, int ival));
static void ugadjust_ack P((int iseq));
static boolean fgwait_for_packet P((boolean freturncontrol, int ctimeout,
				    int cretries));
static boolean fgsend_acks P((void));
static boolean fggot_ack P((int iack));
static boolean fgprocess_data P((boolean fdoacks, boolean freturncontrol,
				 boolean *pfexit, int *pcneed,
				 boolean *pffound));
static boolean fginit_sendbuffers P((boolean fallocate));
static boolean fgcheck_errors P((void));
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
  boolean fgota, fgotb;

  /* The 'g' protocol requires a full eight bit interface.  */
  if (! fport_set (PORTSETTING_EIGHT))
    return FALSE;

  iGsendseq = 1;
  iGremote_ack = 0;
  iGretransmit_seq = -1;
  iGrecseq = 0;
  iGlocal_ack = 0;
  cGsent_packets = 0;
  cGresent_packets = 0;
  cGdelayed_packets = 0;
  cGrec_packets = 0;
  cGbad_hdr = 0;
  cGbad_checksum = 0;
  cGbad_order = 0;
  cGremote_rejects = 0;
  
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
  
  fgota = FALSE;
  fgotb = FALSE;
  for (i = 0; i < cGstartup_retries; i++)
    {
      if (fgota)
	{
	  if (! fgsend_control (INITA, iGlocal_winsize))
	    return FALSE;
	}
      else
	{
	  if (! fgexchange_init (fmaster, INITA, iGlocal_winsize,
				 &iGremote_winsize))
	    continue;
	}
      fgota = TRUE;

      if (fgotb)
	{
	  if (! fgsend_control (INITB, iseg))
	    return FALSE;
	}
      else
	{
	  if (! fgexchange_init (fmaster, INITB, iseg,
				 &iGremote_segsize))
	    continue;
	}
      fgotb = TRUE;

      if (! fgexchange_init (fmaster, INITC, iGlocal_winsize,
			     &iGremote_winsize))
	continue;

      /* We have succesfully connected.  Determine the remote packet
	 size.  */

      iGremote_packsize = 1 << (iGremote_segsize + 5);

      if (! fginit_sendbuffers (TRUE))
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
     it, but it seems that some versions of uucico assume that the
     master sends first and the slave responds.  This only matters if
     we are the slave and the first packet is garbled.  If we send a
     packet, the other side will assume that we must have seen the
     packet they sent and will never time out and send it again.
     Therefore, if we are the slave we don't send a packet the first
     time through the loop.  This can still fail, but should usually
     work, and, after all, if the initialization packets are received
     correctly there will be no problem no matter what we do.  */

  for (i = 0; i < cGexchange_init_retries; i++)
    {
      long itime;
      int ctimeout;

      if (fmaster || i > 0)
	{
	  if (! fgsend_control (ictl, ival))
	    return FALSE;
	}

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

		  /* If we didn't already send our initialization
		     packet, send it now.  */
		  if (! fmaster && i == 0)
		    {
		      if (! fgsend_control (ictl, ival))
			return FALSE;
		    }

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

/* Shut down the protocol.  */

boolean
fgshutdown ()
{
  (void) fgsend_control (CLOSE, 0);
  (void) fgsend_control (CLOSE, 0);
  (void) fginit_sendbuffers (FALSE);

  /* The count of sent packets may not be accurate, because some of
     them may have not been sent yet if the connection failed in the
     middle (the ones that counted for cGdelayed_packets).  I don't
     think it's worth being precise.  */
  ulog (LOG_NORMAL,
	"Protocol 'g' packets: sent %ld, resent %ld, received %ld",
	cGsent_packets, cGresent_packets - cGdelayed_packets,
	cGrec_packets);
  if (cGbad_hdr != 0
      || cGbad_checksum != 0
      || cGbad_order != 0
      || cGremote_rejects != 0)
    ulog (LOG_NORMAL,
	  "Errors: header %ld, checksum %ld, order %ld, remote rejects %ld",
	  cGbad_hdr, cGbad_checksum, cGbad_order, cGremote_rejects);

  return TRUE;
}

/* Send a command string.  We send packets containing the string until
   the entire string has been sent.  Each packet is full.  */

boolean
fgsendcmd (z)
     const char *z;
{
  int clen;
  boolean fagain;

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgsendcmd: Sending command \"%s\"", z);
#endif

  clen = strlen (z);

  do
    {
      char *zpacket;
      int cdummy;

      zpacket = zggetspace (&cdummy);

      if (clen < iGremote_packsize)
	{
	  int csize;

	  /* If the remote packet size is larger than 64 (the default,
	     which may indicate an older UUCP package), try to fit
	     this command into a smaller packet.  We still always send
	     a complete packet, though.  */

	  if (iGremote_packsize <= 64)
	    csize = iGremote_packsize;
	  else
	    {
	      csize = 32;
	      while (csize < clen)
		csize <<= 1;
	    }

	  strcpy (zpacket, z);
	  bzero (zpacket + clen, csize - clen);
	  fagain = FALSE;

	  if (! fgsenddata (zpacket, csize))
	    return FALSE;
	}
      else
	{
	  memcpy (zpacket, z, iGremote_packsize);
	  z += iGremote_packsize;
	  clen -= iGremote_packsize;
	  fagain = TRUE;

	  if (! fgsenddata (zpacket, iGremote_packsize))
	    return FALSE;
	}
    }
  while (fagain);

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
fginit_sendbuffers (fallocate)
     boolean fallocate;
{
  int i;

  /* Free up any remaining old buffers.  */

  for (i = 0; i < CSENDBUFFERS; i++)
    {
      xfree ((pointer) azGsendbuffers[i]);
      if (fallocate)
	{
	  azGsendbuffers[i] = (char *) malloc (CFRAMELEN + iGremote_packsize);
	  if (azGsendbuffers[i] == NULL)
	    return FALSE;
	  bzero (azGsendbuffers[i], CFRAMELEN + iGremote_packsize);
	}
      else
	azGsendbuffers[i] = NULL;
    }
  return TRUE;
}

/* Allocate a packet to send out.  The return value of this function
   must be filled in and passed to fgsenddata, or discarded.  This
   will ensure that the buffers and iGsendseq stay in synch.  Set
   *pclen to the amount of data to place in the buffer.  */

char *
zggetspace (pclen)
     int *pclen;
{
  *pclen = iGremote_packsize;
  return azGsendbuffers[iGsendseq] + CFRAMELEN;
}

/* Send out a data packet.  This computes the checksum, sets up the
   header, and sends the packet out.  The argument should point to the
   return value of zggetspace.  */

boolean
fgsenddata (zdata, cdata)
     char *zdata;
     int cdata;
{
  char *z;
  int itt, iseg, csize;
  unsigned short icheck;

#if DEBUG > 4
  if (iDebug > 4)
    ulog (LOG_DEBUG, "fgsenddata: Sending %d bytes", cdata);
#endif

  /* Set the initial length bytes.  See the description at the definition
     of SHORTDATA, above.  */

  itt = DATA;
  csize = iGremote_packsize;
  iseg = iGremote_segsize + 1;

#if DEBUG > 0
  if (cdata > iGremote_packsize)
    ulog (LOG_FATAL, "fgsend_packet: Packet size too large");
#endif

  if (cdata < iGremote_packsize)
    {
      /* If the remote packet size is larger than 64, the default, we
	 can assume they can handle a smaller packet as well, which
	 will be more efficient to send.  */
      if (iGremote_packsize > 64)
	{
	  /* The packet size is 1 << (iseg + 4).  */
	  iseg = 1;
	  csize = 32;
	  while (csize < cdata)
	    {
	      csize <<= 1;
	      ++iseg;
	    }
	}

      if (csize != cdata)
	{
	  int cshort;

	  /* We have to move the data within the packet,
	     unfortunately.  It's tough to see any way around this
	     without going to some sort of iovec structure.  It only
	     happens once per file transfer.  It would also be nice if
	     we computed the checksum as we move.  We zero out the
	     unused bytes, since it makes people happy.  */

	  itt = SHORTDATA;
	  cshort = csize - cdata;
	  if (cshort <= 127)
	    {
	      xmemmove (zdata + 1, zdata, cdata);
	      zdata[0] = (char) cshort;
	      bzero (zdata + cdata + 1, cshort - 1);
	    }
	  else
	    {
	      xmemmove (zdata + 2, zdata, cdata);
	      zdata[0] = (char) (0x80 | (cshort & 0x7f));
	      zdata[1] = (char) (cshort >> 7);
	      bzero (zdata + cdata + 2, cshort - 2);
	    }
	}
    }

  z = zdata - CFRAMELEN;

  z[IFRAME_DLE] = DLE;
  z[IFRAME_K] = (char) iseg;

  icheck = (unsigned short) igchecksum (zdata, csize);

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

  ++cGsent_packets;

  if (iGretransmit_seq != -1
      && INEXTSEQ (INEXTSEQ (iGretransmit_seq)) != iGsendseq)
    {
      ++cGdelayed_packets;
      return TRUE;
    }

  return fsend_data (z, CFRAMELEN + csize, TRUE);
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

  return fsend_data (ab, CFRAMELEN, TRUE);
}

/* Process existing data.  Set *pfexit to TRUE if a file or a command
   has been completely received.  Return FALSE on error.  */

boolean
fgprocess (pfexit)
     boolean *pfexit;
{
  /* Don't ack incoming data (since this is called when a file is
     being sent and the acks can be merged onto the outgoing data),
     don't return after receiving a control packet, and don't bother
     to report how much more data is needed or whether any packets
     were found.  */
  return fgprocess_data (FALSE, FALSE, pfexit, (int *) NULL,
			 (boolean *) NULL);
}

/* Wait for data to come in.  This continues processing until a
   complete file or command has been received.  */

boolean
fgwait ()
{
  return fgwait_for_packet (FALSE, cGtimeout, cGretries);
}

/* Get a packet.  This is called when we have nothing to send, but
   want to wait for a packet to come in.  If freturncontrol is TRUE,
   this will return after getting any control packet.  Otherwise, it
   will continue to receive packets until a complete file or a
   complete command has been received.  The timeout and the number of
   retries are specified as arguments.  The function returns FALSE if
   an error occurs or if cretries timeouts of ctimeout seconds were
   exceeded.  */

static boolean
fgwait_for_packet (freturncontrol, ctimeout, cretries)
     boolean freturncontrol;
     int ctimeout;
     int cretries;
{
  int ctimeouts;
  int cgarbage;
  int cshort;

  ctimeouts = 0;
  cgarbage = 0;
  cshort = 0;

  while (TRUE)
    {
      boolean fexit;
      int cneed;
      boolean ffound;
      int crec;
  
      if (! fgprocess_data (TRUE, freturncontrol, &fexit, &cneed, &ffound))
	return FALSE;

      if (fexit)
	return TRUE;

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

      if (! freceive_data (cneed, &crec, ctimeout))
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
		  iPrecstart = (iPrecstart + 1) % CRECBUFLEN;
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
	      ++cGresent_packets;
	      if (! fsend_data (azGsendbuffers[inext],
				CFRAMELEN + CPACKLEN (azGsendbuffers[inext]),
				TRUE))
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
    }
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
      ++cGresent_packets;
      if (! fsend_data (azGsendbuffers[inext],
			CFRAMELEN + CPACKLEN (azGsendbuffers[inext]),
			TRUE))
	return FALSE;
      inext = INEXTSEQ (inext);
      if (inext == iGsendseq)
	iGretransmit_seq = -1;
      else
	{
	  ugadjust_ack (inext);
	  ++cGresent_packets;
	  if (! fsend_data (azGsendbuffers[inext],
			    CFRAMELEN + CPACKLEN (azGsendbuffers[inext]),
			    TRUE))
	    return FALSE;
	  iGretransmit_seq = inext;
	}
    }

  return TRUE;
}

/* See if we've received more than the permitted number of errors.  If
   we receive a bad packet, we can expect a window full (less one) of
   out of order packets to follow, so we discount cGbad_order
   accordingly.  */

static boolean
fgcheck_errors ()
{
  int corder;

  if (cGmax_errors < 0)
    return TRUE;

  corder = (cGbad_order
	    - ((cGbad_hdr + cGbad_checksum) * (iGremote_winsize - 1)));
  if (corder < 0)
    corder = 0;

  if ((cGbad_hdr + cGbad_checksum + corder + cGremote_rejects)
      > cGmax_errors)
    {
      ulog (LOG_ERROR, "Too many 'g' protocol errors");
      return FALSE;
    }

  return TRUE;
}

/* Process the receive buffer into a data packet, if possible.  All
   control packets are handled here.  When a data packet is received,
   fgprocess_data calls fgot_data with the data; if that sets its
   pfexit argument to TRUE fgprocess_data will set *pfexit to TRUE and
   return TRUE.  Otherwise if the freturncontrol argument is TRUE
   fgprocess_data will set *pfexit to FALSE and return TRUE.
   Otherwise fgprocess_data will continue trying to process data.  If
   some error occurs, fgprocess_data will return FALSE.  If there is
   not enough data to form a complete packet, then *pfexit will be set
   to FALSE, *pcneed will be set to the number of bytes needed to form
   a complete packet (unless pcneed is NULL) and fgprocess_data will
   return TRUE.  If this function found a data packet, and pffound is
   not NULL, it will set *pffound to TRUE; this can be used to tell
   valid data from an endless stream of garbage and control packets.
   If fdoacks is TRUE, received packets will be acknowledged;
   otherwise they must be acknowledged later.  */

static boolean
fgprocess_data (fdoacks, freturncontrol, pfexit, pcneed, pffound)
     boolean fdoacks;
     boolean freturncontrol;
     boolean *pfexit;
     int *pcneed;
     boolean *pffound;
{
  *pfexit = FALSE;
  if (pffound != NULL)
    *pffound = FALSE;

  for (; iPrecstart != iPrecend; iPrecstart = (iPrecstart + 1) % CRECBUFLEN)
    {
      char ab[CFRAMELEN];
      int i, iget, cwant;
      unsigned short ihdrcheck, idatcheck;
      const char *zfirst, *zsecond;
      int cfirst, csecond;

      /* Look for the DLE which must start a packet.  */

      if (abPrecbuf[iPrecstart] != DLE)
	continue;

      /* Get the first six bytes into ab.  */

      for (i = 0, iget = iPrecstart;
	   i < CFRAMELEN && iget != iPrecend;
	   i++, iget = (iget + 1) % CRECBUFLEN)
	ab[i] = abPrecbuf[iget];

      /* If there aren't six bytes, there is no packet.  */

      if (i < CFRAMELEN)
	{
	  if (pcneed != NULL)
	    *pcneed = CFRAMELEN - i;
	  return TRUE;
	}

      /* Make sure these six bytes start a packet.  If they don't,
	 loop around to bump iGrecstart and look for another DLE.  */

      if (ab[IFRAME_DLE] != DLE
	  || ab[IFRAME_K] < 1
	  || ab[IFRAME_K] > 9
	  || ab[IFRAME_XOR] != (ab[IFRAME_K] ^ ab[IFRAME_CHECKLOW]
				^ ab[IFRAME_CHECKHIGH] ^ ab[IFRAME_CONTROL])
	  || CONTROL_TT (ab[IFRAME_CONTROL]) == ALTCHAN)
	{
	  ++cGbad_hdr;
	  if (! fgcheck_errors ())
	    return FALSE;
	  continue;
	}

      /* The zfirst and cfirst pair point to the first set of data for
	 this packet; the zsecond and csecond point to the second set,
	 in case the packet wraps around the end of the buffer.  */
      zfirst = abPrecbuf + iPrecstart + CFRAMELEN;
      cfirst = 0;
      zsecond = NULL;
      csecond = 0;

      if (ab[IFRAME_K] == KCONTROL)
	{
	  /* This is a control packet.  It should not have any data.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    {
	      ++cGbad_hdr;
	      if (! fgcheck_errors ())
		return FALSE;
	      continue;
	    }

	  idatcheck = (unsigned short) (0xaaaa - ab[IFRAME_CONTROL]);
	  cwant = 0;
	}
      else
	{
	  int cinbuf;
	  unsigned short icheck;

	  /* This is a data packet.  It should not be type CONTROL.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) == CONTROL)
	    {
	      ++cGbad_hdr;
	      if (! fgcheck_errors ())
		return FALSE;
	      continue;
	    }

	  if (iPrecend >= iPrecstart)
	    cinbuf = iPrecend - iPrecstart;
	  else
	    cinbuf = CRECBUFLEN - (iPrecstart - iPrecend);
	  cinbuf -= CFRAMELEN;

	  /* Make sure we have enough data.  If we don't, wait for
	     more.  */	     

	  cwant = CPACKLEN (ab);
	  if (cinbuf < cwant)
	    {
	      if (pcneed != NULL)
		*pcneed = cwant - cinbuf;
	      return TRUE;
	    }
	  
	  /* Set up the data pointers and compute the checksum.  */

	  if (iPrecend >= iPrecstart)
	    cfirst = cwant;
	  else
	    {
	      cfirst = CRECBUFLEN - (iPrecstart + CFRAMELEN);
	      if (cfirst >= cwant)
		cfirst = cwant;
	      else if (cfirst > 0)
		{
		  zsecond = abPrecbuf;
		  csecond = cwant - cfirst;
		}
	      else
		{
		  /* Here cfirst is non-positive, so subtracting from
		     abPrecbuf will actually skip the appropriate number
		     of bytes at the start of abPrecbuf.  */
		  zfirst = abPrecbuf - cfirst;
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

	  ++cGbad_checksum;
	  if (! fgcheck_errors ())
	    return FALSE;

	  /* If the checksum failed for a data packet, then if it was
	     the one we were expecting send an RJ, otherwise ignore
	     it.  Previously if this code got the wrong packet number
	     it would send an RR, but that may confuse some Telebit
	     modems and it doesn't help in any case since the receiver
	     will probably just ignore the RR as a duplicate (that's
	     basically what this code does).  If we totally missed the
	     packet we will time out and send an RJ in the function
	     fgwait_for_packet above.  */

	  if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	    {
	      /* Make sure we've acked everything up to this point.  */
	      if (iGrecseq != iGlocal_ack)
		{
		  if (! fgsend_acks ())
		    return FALSE;
		}

	      /* If this is the packet we wanted, tell the sender that
		 it failed.  */
	      if (CONTROL_XXX (ab[IFRAME_CONTROL]) == INEXTSEQ (iGrecseq))
		{
		  if (! fgsend_control (RJ, iGrecseq))
		    return FALSE;
		}
	    }

	  /* We can't skip the packet data after this, because if we
	     have lost incoming bytes the next DLE will be somewhere
	     in what we thought was the packet data.  */
	  continue;
	}

      /* We have a packet; remove the processed bytes from the receive
	 buffer.  Note that if we go around the loop again after this
	 assignment, we must decrement iPrecstart first to account for
	 the increment which will be done by the loop control.  */
      iPrecstart = (iPrecstart + cwant + CFRAMELEN) % CRECBUFLEN;

      /* Store the control byte for the use of calling functions.  */
      iGpacket_control = ab[IFRAME_CONTROL] & 0xff;

      /* Update the received sequence number from the yyy field of a
	 data packet or an RR control packet.  */

      if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL
	  || CONTROL_XXX (ab[IFRAME_CONTROL]) == RR)
	{
	  if (! fggot_ack (CONTROL_YYY (ab[IFRAME_CONTROL])))
	    return FALSE;
	}

      /* If this isn't a control message, make sure we have received
	 the expected packet sequence number, acknowledge the packet
	 if it's the right one, and process the data.  */

      if (CONTROL_TT (ab[IFRAME_CONTROL]) != CONTROL)
	{
	  if (CONTROL_XXX (ab[IFRAME_CONTROL]) != INEXTSEQ (iGrecseq))
	    {
	      /* We got the wrong packet number.  */
#if DEBUG > 7
	      if (iDebug > 7)
		ulog (LOG_DEBUG, "fgprocess_data: Got packet %d; expected %d",
		      CONTROL_XXX (ab[IFRAME_CONTROL]),
		      INEXTSEQ (iGrecseq));
#endif

	      ++cGbad_order;
	      if (! fgcheck_errors ())
		return FALSE;

	      /* This code used to send an RR to encourage the other
		 side to get back in synch, but that may confuse some
		 Telebit modems and does little good in any case,
		 since the other side will probably just ignore it
		 anyhow (that's what this code does).  */

	      /* As noted above, we must decrement iPrecstart because
		 the loop control will increment it.  */

	      --iPrecstart;
	      continue;
	    }

	  /* We got the packet we expected.  */

	  ++cGrec_packets;

	  iGrecseq = INEXTSEQ (iGrecseq);

	  /* Tell the caller that we found something.  */
	  if (pffound != NULL)
	    *pffound = TRUE;

	  /* If we are supposed to do acknowledgements here, send back
	     an RR packet.  */

	  if (fdoacks)
	    {
	      if (! fgsend_acks ())
		return FALSE;
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

	  /* If *pfexit gets set by the first batch of data, and there
	     is still more data, it must be the case that we are
	     accumulating a command and encountered a null byte, so we
	     can ignore the second batch of data.  */

	  if (! fgot_data (zfirst, cfirst, FALSE, FALSE, pfexit))
	    return FALSE;
	  if (csecond > 0 && ! *pfexit)
	    {
	      if (! fgot_data (zsecond, csecond, FALSE, FALSE, pfexit))
		return FALSE;
	    }

	  /* If fgot_data told us that we were finished, get out.  */
	  if (*pfexit)
	    return TRUE;

	  /* If we've been asked to return control packets, get out
	     now.  */
	  if (freturncontrol)
	    {
	      *pfexit = TRUE;
	      return TRUE;
	    }

	  /* As noted above, we must decrement iPrecstart because
	     the loop control will increment it.  */

	  --iPrecstart;
	  continue;
	}

      /* Handle control messages here. */

      switch (CONTROL_XXX (ab[IFRAME_CONTROL]))
	{
	case CLOSE:
	  /* The other side has closed the connection.  */
	  if (fPerror_ok)
	    (void) fgshutdown ();
	  else
	    {
	      ulog (LOG_ERROR, "Received unexpected CLOSE packet");
	      (void) fgsend_control (CLOSE, 0);
	    }
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
	      char *zpack;

	      ugadjust_ack (iGretransmit_seq);
	      ++cGresent_packets;
	      ++cGremote_rejects;
	      if (! fgcheck_errors ())
		return FALSE;
	      zpack = azGsendbuffers[iGretransmit_seq];
	      if (! fsend_data (zpack, CFRAMELEN + CPACKLEN (zpack), TRUE))
		return FALSE;
	    }
	  break;
	case SRJ:
	  /* Selectively reject a particular packet.  This is not used
	     by UUCP, but it's easy to support.  */
	  {
	    char *zpack;

	    ugadjust_ack (CONTROL_YYY (ab[IFRAME_CONTROL]));
	    ++cGresent_packets;
	    ++cGremote_rejects;
	    zpack = azGsendbuffers[CONTROL_YYY (ab[IFRAME_CONTROL])];
	    if (! fsend_data (zpack, CFRAMELEN + CPACKLEN (zpack), TRUE))
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
	{
	  *pfexit = TRUE;
	  return TRUE;
	}

      /* Loop around to look for the next packet, if any.  As noted
	 above, we must decrement iPrecstart since the loop control
	 will increment it.  */

      --iPrecstart;
    }

  /* There is no data left in the receive buffer.  */

  if (pcneed != NULL)
    *pcneed = CFRAMELEN;
  return TRUE;
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
