/* trans.c
   Routines to handle file transfers.

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

#include "uucp.h"

#if USE_RCS_ID
const char trans_rcsid[] = "$Id$";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "prot.h"
#include "system.h"
#include "trans.h"

/* Local functions.  */

static void utqueue P((struct stransfer **, struct stransfer *,
		       boolean fhead));
static void utdequeue P((struct stransfer *));
static void utchanalc P((struct sdaemon *qdaemon, struct stransfer *qtrans));
__inline__ static struct stransfer *qtchan P((int ichan));
__inline__ static void utchanfree P((struct stransfer *qtrans));
static boolean ftadd_cmd P((struct sdaemon *qdaemon, const char *z,
			    size_t cdata, int iremote, boolean flast));
static boolean fremote_hangup_reply P((struct stransfer *qtrans,
				       struct sdaemon *qdaemon));

/* Queue of transfer structures that are ready to start which have
   been requested by the local system.  These are only permitted to
   start when the local system is the master.  */
static struct stransfer *qTlocal;

/* Queue of transfer structures that are ready to start which have
   been requested by the remote system.  These are responses to
   commands received from the remote system, and should be started as
   soon as possible.  */
static struct stransfer *qTremote;

/* Queue of transfer structures that have been started and want to
   send information.  This should be static, but the 'a' protocol
   looks at it, at least for now.  */
struct stransfer *qTsend;

/* Queue of transfer structures that have been started and are waiting
   to receive information.  */
static struct stransfer *qTreceive;

/* Queue of free transfer structures.  */
static struct stransfer *qTavail;

/* Array of transfer structures indexed by local channel number.  This
   is maintained for local jobs.  */
static struct stransfer *aqTchan[IMAX_CHAN + 1];

/* Number of local channel numbers currently allocated.  */
static int cTchans;

/* Next channel number to allocate.  */
static int iTchan;

/* Array of transfer structures indexed by remote channel number.
   This is maintained for remote jobs.  */
static struct stransfer *aqTremote[IMAX_CHAN + 1];

/* The stored time of the last received data.  */
static long iTsecs;
static long iTmicros;

/* The size of the command we have read so far in ftadd_cmd.  */
static size_t cTcmdlen;

/* The structure we use when waiting for an acknowledgement of a
   confirmed received file in fsent_receive_ack, and a list of those
   structures.  */

struct sreceive_ack
{
  struct sreceive_ack *qnext;
  char *zto;
  char *ztemp;
  boolean fmarked;
};

static struct sreceive_ack *qTreceive_ack;

/* Queue up a transfer structure before *pq.  This puts it at the head
   or the fail of the list headed by *pq.  */

static void
utqueue (pq, q, fhead)
     struct stransfer **pq;
     struct stransfer *q;
     boolean fhead;
{
  if (*pq == NULL)
    {
      *pq = q;
      q->qprev = q->qnext = q;
    }
  else
    {
      q->qnext = *pq;
      q->qprev = (*pq)->qprev;
      q->qprev->qnext = q;
      q->qnext->qprev = q;
      if (fhead)
	*pq = q;
    }
  q->pqqueue = pq;
}

/* Dequeue a transfer structure.  */

static void
utdequeue (q)
     struct stransfer *q;
{
  if (q->pqqueue != NULL)
    {
      if (*(q->pqqueue) == q)
	{
	  if (q->qnext == q)
	    *(q->pqqueue) = NULL;
	  else
	    *(q->pqqueue) = q->qnext;
	}
      q->pqqueue = NULL;
    }
  if (q->qprev != NULL)
    q->qprev->qnext = q->qnext;
  if (q->qnext != NULL)
    q->qnext->qprev = q->qprev;
  q->qprev = NULL;
  q->qnext = NULL;
}

/* Queue up a transfer structure requested by the local system.  */

void
uqueue_local (qtrans)
     struct stransfer *qtrans;
{
  utdequeue (qtrans);
  utqueue (&qTlocal, qtrans, FALSE);
}

/* Queue up a transfer structure requested by the remote system.  The
   stransfer structure should have the iremote field set.  We need to
   record it, so that any subsequent data associated with this
   channel can be routed to the right place.  */

void
uqueue_remote (qtrans)
     struct stransfer *qtrans;
{
  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "uqueue_remote: Channel %d",
		  qtrans->iremote);
  if (qtrans->iremote > 0)
    aqTremote[qtrans->iremote] = qtrans;
  utdequeue (qtrans);
  utqueue (&qTremote, qtrans, FALSE);
}

/* Queue up a transfer with something to send.  */

void
uqueue_send (qtrans)
     struct stransfer *qtrans;
{
#if DEBUG > 0
  if (qtrans->psendfn == NULL)
    ulog (LOG_FATAL, "uqueue_send: Bad call");
#endif
  utdequeue (qtrans);
  utqueue (&qTsend, qtrans, FALSE);
}

/* Queue up a transfer with something to receive.  */

void
uqueue_receive (qtrans)
     struct stransfer *qtrans;
{
#if DEBUG > 0
  if (qtrans->precfn == NULL)
    ulog (LOG_FATAL, "uqueue_receive: Bad call");
#endif
  utdequeue (qtrans);
  utqueue (&qTreceive, qtrans, FALSE);
}

/* Get a new local channel number.  */

static void
utchanalc (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  do
    {
      ++iTchan;
      if (iTchan > qdaemon->qproto->cchans)
	iTchan = 1;
    }
  while (aqTchan[iTchan] != NULL);

  qtrans->ilocal = iTchan;
  aqTchan[iTchan] = qtrans;
  ++cTchans;
}

/* Return the transfer for a channel number.  */

__inline__
static struct stransfer *
qtchan (ic)
     int ic;
{
  return aqTchan[ic];
}

/* Clear the channel number for a transfer.  */

__inline__
static void
utchanfree (qt)
     struct stransfer *qt;
{
  if (qt->ilocal != 0)
    {
      aqTchan[qt->ilocal] = NULL;
      qt->ilocal = 0;
      --cTchans;
    }
}

/* Allocate a new transfer structure.  */

struct stransfer *
qtransalc (qcmd)
     struct scmd *qcmd;
{
  register struct stransfer *q;

  q = qTavail;
  if (q != NULL)
    utdequeue (q);
  else
    {
      q = (struct stransfer *) xmalloc (sizeof (struct stransfer));
      q->calcs = 1;
    }
  q->qnext = NULL;
  q->qprev = NULL;
  q->pqqueue = NULL;
  q->psendfn = NULL;
  q->precfn = NULL;
  q->pinfo = NULL;
  q->fsendfile = FALSE;
  q->frecfile = FALSE;
  q->e = EFILECLOSED;
  q->ipos = 0;
  q->fcmd = FALSE;
  q->zcmd = NULL;
  q->ccmd = 0;
  q->ilocal = 0;
  q->iremote = 0;
  if (qcmd != NULL)
    {
      q->s = *qcmd;
      q->s.zfrom = zbufcpy (qcmd->zfrom);
      q->s.zto = zbufcpy (qcmd->zto);
      q->s.zuser = zbufcpy (qcmd->zuser);
      q->s.zoptions = zbufcpy (qcmd->zoptions);
      q->s.ztemp = zbufcpy (qcmd->ztemp);
      q->s.znotify = zbufcpy (qcmd->znotify);
      q->s.zcmd = zbufcpy (qcmd->zcmd);
    }
  else
    {
      q->s.zfrom = NULL;
      q->s.zto = NULL;
      q->s.zuser = NULL;
      q->s.zoptions = NULL;
      q->s.ztemp = NULL;
      q->s.znotify = NULL;
      q->s.zcmd = NULL;
    }
  q->isecs = 0;
  q->imicros = 0;
  q->cbytes = 0;

  return q;
}

/* Free a transfer structure.  This does not free any pinfo
   information that may have been allocated.  */

void
utransfree (q)
     struct stransfer *q;
{
  ubuffree (q->zcmd);
  ubuffree ((char *) q->s.zfrom);
  ubuffree ((char *) q->s.zto);
  ubuffree ((char *) q->s.zuser);
  ubuffree ((char *) q->s.zoptions);
  ubuffree ((char *) q->s.ztemp);
  ubuffree ((char *) q->s.znotify);
  ubuffree ((char *) q->s.zcmd);
  
  utchanfree (q);    
  if (q->iremote > 0)
    {
      aqTremote[q->iremote] = NULL;
      q->iremote = 0;
    }

#if DEBUG > 0
  q->zcmd = NULL;
  q->s.zfrom = NULL;
  q->s.zto = NULL;
  q->s.zuser = NULL;
  q->s.zoptions = NULL;
  q->s.ztemp = NULL;
  q->s.znotify = NULL;
  q->s.zcmd = NULL;
  q->psendfn = NULL;
  q->precfn = NULL;
#endif

  ++q->calcs;

  utdequeue (q);
  utqueue (&qTavail, q, FALSE);
}

/* Gather local commands and queue them up for later processing.  Also
   recompute time based control values.  */

boolean
fqueue (qdaemon, pfany)
     struct sdaemon *qdaemon;
     boolean *pfany;
{
  const struct uuconf_system *qsys;
  int bgrade;
  struct uuconf_timespan *qlocal_size, *qremote_size;

  if (pfany != NULL)
    *pfany = FALSE;

  qsys = qdaemon->qsys;

  /* If we are not the caller, the grade will be set during the
     initial handshake.  */
  if (! qdaemon->fcaller)
    bgrade = qdaemon->bgrade;
  else
    {
      long ival;

      if (! ftimespan_match (qsys->uuconf_qtimegrade, &ival,
			     (int *) NULL))
	bgrade = '\0';
      else
	bgrade = (char) ival;
    }

  /* Determine the maximum sizes we can send and receive.  */
  if (qdaemon->fcaller)
    {
      qlocal_size = qsys->uuconf_qcall_local_size;
      qremote_size = qsys->uuconf_qcall_remote_size;
    }
  else
    {
      qlocal_size = qsys->uuconf_qcalled_local_size;
      qremote_size = qsys->uuconf_qcalled_remote_size;
    }

  if (! ftimespan_match (qlocal_size, &qdaemon->clocal_size, (int *) NULL))
    qdaemon->clocal_size = (long) -1;
  if (! ftimespan_match (qremote_size, &qdaemon->cremote_size, (int *) NULL))
    qdaemon->cremote_size = (long) -1;

  if (bgrade == '\0')
    return TRUE;

  if (! fsysdep_get_work_init (qsys, bgrade, FALSE))
    return FALSE;

  while (TRUE)
    {
      struct scmd s;

      if (! fsysdep_get_work (qsys, bgrade, FALSE, &s))
	return FALSE;

      if (s.bcmd == 'H')
	{
	  ulog_user ((const char *) NULL);
	  break;
	}

      ulog_user (s.zuser);

      switch (s.bcmd)
	{
	case 'S':
	case 'E':
	  if (! flocal_send_file_init (qdaemon, &s))
	    return FALSE;
	  break;
	case 'R':
	  if (! flocal_rec_file_init (qdaemon, &s))
	    return FALSE;
	  break;
	case 'X':
	  if (! flocal_xcmd_init (qdaemon, &s))
	    return FALSE;
	  break;
#if DEBUG > 0
	default:
	  ulog (LOG_FATAL, "fqueue: Can't happen");
	  break;
#endif
	}
    }	  

  if (pfany != NULL)
    *pfany = qTlocal != NULL;

  return TRUE;
}

/* The main transfer loop.  The uucico daemon spends essentially all
   its time in this function.  */

boolean
floop (qdaemon)
     struct sdaemon *qdaemon;
{
  int i;
  int cchans;
  boolean fret;

  /* If we are using a half-duplex line, act as though we have only a
     single channel; otherwise we might start a send and a receive at
     the same time.  */
  if ((qdaemon->ireliable & UUCONF_RELIABLE_FULLDUPLEX) == 0)
    cchans = 1;
  else
    cchans = qdaemon->qproto->cchans;

  fret = TRUE;

  while (! qdaemon->fhangup)
    {
      register struct stransfer *q;

#if DEBUG > 1
      /* If we're doing any debugging, close the log and debugging
	 files regularly.  This will let people copy them off and
	 remove them while the conversation is in progresss.  */
      if (iDebug != 0)
	{
	  ulog_close ();
	  ustats_close ();
	}
#endif

      if (qdaemon->fmaster
	  && qTremote == NULL
	  && qTlocal == NULL
	  && qTsend == NULL
	  && qTreceive == NULL)
	{
	  /* Try to get some more jobs to do.  If we can't get any,
	     start the hangup procedure.  */
	  if (! fqueue (qdaemon, (boolean *) NULL))
	    {
	      fret = FALSE;
	      break;
	    }
	  if (qTlocal == NULL)
	    {
	      DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "floop: No work for master");
	      if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, "H", 0, 0))
		{
		  fret = FALSE;
		  break;
		}
	      qdaemon->fmaster = FALSE;
	    }
	}

      /* Immediately queue up any remote jobs.  We don't need local
	 channel numbers for them, since we can disambiguate based on
	 the remote channel number.  */
      while (qTremote != NULL)
	{
	  q = qTremote;
	  utdequeue (q);
	  utqueue (&qTsend, q, TRUE);
	}

      /* If we are the master, or if we have multiple channels, try to
	 queue up additional local jobs.  */
      if (qdaemon->fmaster || cchans > 1)
	{
	  while (qTlocal != NULL && cTchans < cchans)
	    {
	      /* We have room for an additional channel.  */
	      q = qTlocal;
	      uqueue_send (q);
	      utchanalc (qdaemon, q);
	    }
	}

      q = qTsend;

      if (q == NULL)
	{
	  ulog_user ((const char *) NULL);
	  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "floop: Waiting for data");
	  if (! (*qdaemon->qproto->pfwait) (qdaemon))
	    {
	      fret = FALSE;
	      break;
	    }
	}
      else
	{
	  long isecs, imicros;
	  int calcs;

	  isecs = isysdep_process_time (&imicros);
	  calcs = q->calcs;

	  ulog_user (q->s.zuser);

	  if (! q->fsendfile)
	    {
	      if (! (*q->psendfn) (q, qdaemon))
		{
		  fret = FALSE;
		  break;
		}
	    }
	  else
	    {
	      if (q->zlog != NULL)
		{
		  ulog (LOG_NORMAL, q->zlog);
		  ubuffree (q->zlog);
		  q->zlog = NULL;
		}

	      /* We can read the file in a tight loop until qTremote
		 changes or until we have transferred the entire file.
		 We can disregard any changes to qTlocal since we
		 already have something to send anyhow.  */
	      while (qTremote == NULL)
		{
		  char *zdata;
		  size_t cdata;
		  long ipos;

		  zdata = (*qdaemon->qproto->pzgetspace) (qdaemon, &cdata);
		  if (zdata == NULL)
		    {
		      fret = FALSE;
		      break;
		    }

		  if (ffileeof (q->e))
		    cdata = 0;
		  else
		    {
		      cdata = cfileread (q->e, zdata, cdata);
		      if (ffilereaderror (q->e, cdata))
			{
			  /* There is no way to report a file reading
			     error, so we just drop the connection.  */
			  ulog (LOG_ERROR, "read: %s", strerror (errno));
			  fret = FALSE;
			  break;
			}
		    }

		  ipos = q->ipos;
		  q->ipos += cdata;
		  q->cbytes += cdata;

		  if (! (*qdaemon->qproto->pfsenddata) (qdaemon, zdata,
							cdata, q->ilocal,
							q->iremote, ipos))
		    {
		      fret = FALSE;
		      break;
		    }

		  /* It is possible that this transfer has just been
		     cancelled.  */
		  if (q != qTsend || ! q->fsendfile)
		    break;

		  if (cdata == 0)
		    {
		      q->fsendfile = FALSE;
		      if (! (*q->psendfn) (q, qdaemon))
			fret = FALSE;
		      break;
		    }
		}

	      if (! fret)
		break;
	    }

	  /* If this is the same transfer, increment the time.  This
	     is only safe because utransfree never actually frees a
	     transfer structure, it just puts it on the available
	     list.  */
	  if (q->calcs == calcs)
	    {
	      long iendsecs, iendmicros;

	      iendsecs = isysdep_process_time (&iendmicros);
	      q->isecs += iendsecs - isecs;
	      q->imicros += iendmicros - imicros;
	    }
	}
    }

  ulog_user ((const char *) NULL);

  (void) (*qdaemon->qproto->pfshutdown) (qdaemon);

  usysdep_get_work_free (qdaemon->qsys);

  if (fret)
    uwindow_acked (qdaemon, TRUE);
  else
    ustats_failed (qdaemon->qsys);

  /* Clear all the variables in case we make another call.  */
  qTlocal = NULL;
  qTremote = NULL;
  qTsend = NULL;
  qTreceive = NULL;
  cTchans = 0;
  iTchan = 0;
  iTsecs = 0;
  iTmicros = 0;
  cTcmdlen = 0;
  qTreceive_ack = NULL;
  for (i = 0; i < IMAX_CHAN + 1; i++)
    {
      aqTchan[i] = NULL;
      aqTremote[i] = NULL;
    }

  return fret;
}

/* This is called by the protocol routines when they have received
   some data.  If pfexit is not NULL, *pfexit should be set to TRUE if
   the protocol receive loop should exit back to the main floop
   routine, above.  It is only important to set *pfexit to TRUE if the
   main loop called the pfwait entry point, so we need never set it to
   TRUE if we just receive data for a file.  This routine never sets
   *pfexit to FALSE.  */

boolean 
fgot_data (qdaemon, zfirst, cfirst, zsecond, csecond, ilocal, iremote, ipos,
	   fallacked, pfexit)
     struct sdaemon *qdaemon;
     const char *zfirst;
     size_t cfirst;
     const char *zsecond;
     size_t csecond;
     int ilocal;
     int iremote;
     long ipos;
     boolean fallacked;
     boolean *pfexit;
{
  long inextsecs, inextmicros;
  struct stransfer *q;
  int cwrote;
  int calcs;
  boolean fret;

  if (iTsecs == 0)
    iTsecs = isysdep_process_time (&iTmicros);

  if (fallacked && qTreceive_ack != NULL)
    uwindow_acked (qdaemon, TRUE);

  /* Now we have to decide which transfer structure gets the data.  If
     ilocal is -1, it means that the protocol does not know where to
     route the data.  In that case we route it to the first transfer
     that is waiting for data, or, if none, as a new command.  If
     ilocal is 0, we either select based on the remote channel number
     or we have a new command.  */
  if (ilocal == -1 && qTreceive != NULL)
    q = qTreceive;
  else if (ilocal == 0 && iremote > 0 && aqTremote[iremote] != NULL)
    q = aqTremote[iremote];
  else if (ilocal <= 0)
    {
      const char *znull;

      ulog_user ((const char *) NULL);

      /* This data is part of a command.  If there is no null
	 character in the data, this string will be continued by the
	 next packet.  Otherwise this must be the last string in the
	 command, and we don't care about what comes after the null
	 byte.  */
      znull = (const char *) memchr (zfirst, '\0', cfirst);
      if (znull != NULL)
	fret = ftadd_cmd (qdaemon, zfirst, (size_t) (znull - zfirst),
			  iremote, TRUE);
      else
	{
	  fret = ftadd_cmd (qdaemon, zfirst, cfirst, iremote, FALSE);
	  if (fret && csecond > 0)
	    {
	      znull = (const char *) memchr (zsecond, '\0', csecond);
	      if (znull != NULL)
		fret = ftadd_cmd (qdaemon, zsecond,
				  (size_t) (znull - zsecond), iremote, TRUE);
	      else
		fret = ftadd_cmd (qdaemon, zsecond, csecond, iremote, FALSE);
	    }
	}

      if (pfexit != NULL && (qdaemon->fhangup || qTremote != NULL))
	*pfexit = TRUE;

      /* The time spent to gather a new command does not get charged
	 to any one command.  */
      iTsecs = isysdep_process_time (&iTmicros);

      return fret;
    }
  else
    {
      /* Get the transfer structure this data is intended for.  */

      q = qtchan (ilocal);
    }

#if DEBUG > 0
  if (q == NULL || q->precfn == NULL)
    {
      ulog (LOG_ERROR, "Protocol error: %lu bytes remote %d local %d",
	    (unsigned long) (cfirst + csecond),
	    iremote, ilocal);
      return FALSE;
    }
#endif

  ulog_user (q->s.zuser);

  calcs = q->calcs;

  fret = TRUE;

  /* If we're receiving a command, then accumulate it up to the null
     byte.  */
  if (q->fcmd)
    {
      const char *znull;

      znull = NULL;
      while (cfirst > 0)
	{
	  size_t cnew;
	  char *znew;

	  znull = (const char *) memchr (zfirst, '\0', cfirst);
	  if (znull != NULL)
	    cnew = znull - zfirst;
	  else
	    cnew = cfirst;
	  znew = zbufalc (q->ccmd + cnew + 1);
	  memcpy (znew, q->zcmd, q->ccmd);
	  memcpy (znew + q->ccmd, zfirst, cnew);
	  znew[q->ccmd + cnew] = '\0';
	  ubuffree (q->zcmd);
	  q->zcmd = znew;
	  q->ccmd += cnew;

	  if (znull != NULL)
	    break;

	  zfirst = zsecond;
	  cfirst = csecond;
	  csecond = 0;
	}

      if (znull != NULL)
	{
	  char *zcmd;
	  size_t ccmd;

	  zcmd = q->zcmd;
	  ccmd = q->ccmd;
	  q->fcmd = FALSE;
	  q->zcmd = NULL;
	  q->ccmd = 0;
	  fret = (*q->precfn) (q, qdaemon, zcmd, ccmd + 1);
	  ubuffree (zcmd);
	}

      if (pfexit != NULL
	  && (qdaemon->fhangup
	      || qdaemon->fmaster
	      || qTsend != NULL))
	*pfexit = TRUE;
    }
  else if (! q->frecfile || cfirst == 0)
    {
      /* We're either not receiving a file or the file transfer is
	 complete.  */
      q->frecfile = FALSE;
      fret = (*q->precfn) (q, qdaemon, zfirst, cfirst);
      if (fret && csecond > 0)
	return fgot_data (qdaemon, zsecond, csecond,
			  (const char *) NULL, (size_t) 0,
			  ilocal, iremote, ipos + (long) cfirst,
			  FALSE, pfexit);
      if (pfexit != NULL
	  && (qdaemon->fhangup
	      || qdaemon->fmaster
	      || qTsend != NULL))
	*pfexit = TRUE;
    }
  else
    {
      if (q->zlog != NULL)
	{
	  ulog (LOG_NORMAL, q->zlog);
	  ubuffree (q->zlog);
	  q->zlog = NULL;
	}

      if (ipos != -1 && ipos != q->ipos)
	{
	  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO,
			  "fgot_data: Seeking to %ld", ipos);
	  if (! ffileseek (q->e, ipos))
	    {
	      ulog (LOG_ERROR, "seek: %s", strerror (errno));
	      fret = FALSE;
	    }
	  q->ipos = ipos;
	}

      if (fret)
	{
	  while (cfirst > 0)
	    {
	      cwrote = cfilewrite (q->e, (char *) zfirst, cfirst);
	      if (cwrote == cfirst)
		{
		  q->cbytes += cfirst;
		  q->ipos += cfirst;
		}
	      else
		{
		  if (cwrote < 0)
		    ulog (LOG_ERROR, "write: %s", strerror (errno));
		  else
		    ulog (LOG_ERROR,
			  "Wrote %d to file when trying to write %lu",
			  cwrote, (unsigned long) cfirst);

		  /* Any write error is almost certainly a temporary
		     condition, or else UUCP would not be functioning
		     at all.  If we continue to accept the file, we
		     will wind up rejecting it at the end (what else
		     could we do?)  and the remote system will throw
		     away the request.  We're better off just dropping
		     the connection, which is what happens when we
		     return FALSE, and trying again later.  */
		  fret = FALSE;
		  break;
		}

	      zfirst = zsecond;
	      cfirst = csecond;
	      csecond = 0;
	    }
	}

      if (pfexit != NULL && qdaemon->fhangup)
	*pfexit = TRUE;
    }

  inextsecs = isysdep_process_time (&inextmicros);

  /* If this is the same transfer structure, add in the current time.
     spent.  We can only get away with this because we know that the
     structure is not actually freed up (it's put on the qTavail
     list).  */
  if (q->calcs == calcs)
    {
      q->isecs += inextsecs - iTsecs;
      q->imicros += inextmicros - iTmicros;
    }

  iTsecs = inextsecs;
  iTmicros = inextmicros;

  return fret;
}

/* Accumulate a string into a command.  If the command is complete,
   start up a new transfer.  */

static boolean
ftadd_cmd (qdaemon, z, clen, iremote, flast)
     struct sdaemon *qdaemon;
     const char *z;
     size_t clen;
     int iremote;
     boolean flast;
{
  static char *zbuf;
  static size_t cbuf;
  size_t cneed;
  struct scmd s;

  cneed = cTcmdlen + clen + 1;
  if (cneed > cbuf)
    {
      zbuf = (char *) xrealloc ((pointer) zbuf, cneed);
      cbuf = cneed;
    }

  memcpy (zbuf + cTcmdlen, z, clen);
  zbuf[cTcmdlen + clen] = '\0';

  if (! flast)
    {
      cTcmdlen += clen;
      return TRUE;
    }

  /* Don't save this string for next time.  */
  cTcmdlen = 0;

  DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO,
		  "ftadd_cmd: Got command \"%s\"", zbuf);

  if (! fparse_cmd (zbuf, &s))
    {
      ulog (LOG_ERROR, "Received garbled command \"%s\"", zbuf);
      return TRUE;
    }

  if (s.bcmd != 'H' && s.bcmd != 'Y' && s.bcmd != 'N')
    ulog_user (s.zuser);
  else
    ulog_user ((const char *) NULL);

  switch (s.bcmd)
    {
    case 'S':
    case 'E':
      return fremote_send_file_init (qdaemon, &s, iremote);
    case 'R':
      return fremote_rec_file_init (qdaemon, &s, iremote);
    case 'X':
      return fremote_xcmd_init (qdaemon, &s, iremote);
    case 'H':
      /* This is a remote request for a hangup.  We close the log
	 files so that they may be moved at this point.  */
      ulog_close ();
      ustats_close ();
      {
	struct stransfer *q;

	q = qtransalc ((struct scmd *) NULL);
	q->psendfn = fremote_hangup_reply;
	q->iremote = iremote;
	uqueue_remote (q);
      }
      return TRUE;
    case 'N':
      /* This means a hangup request is being denied; we just ignore
	 this and wait for further commands.  */
      return TRUE;
    case 'Y':
      /* This is a remote confirmation of a hangup.  We reconfirm.  */
      if (qdaemon->fhangup)
	return TRUE;
#if DEBUG > 0
      if (qdaemon->fmaster)
	ulog (LOG_ERROR, "Got hangup reply as master");
#endif
      /* Don't check errors rigorously here, since the other side
	 might jump the gun and hang up.  The fLog_sighup variable
	 will get set TRUE again when the port is closed.  */
      fLog_sighup = FALSE;
      (void) (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, iremote);
      qdaemon->fhangup = TRUE;
      return TRUE;
#if DEBUG > 0
    default:
      ulog (LOG_FATAL, "ftadd_cmd: Can't happen");
      return FALSE;
#endif
    }
}

/* The remote system is requesting a hang up.  If we have something to
   do, send an HN.  Otherwise send two HY commands (the other side is
   presumed to send an HY command between the first and second, but we
   don't bother to wait for it) and hang up.  */

static boolean
fremote_hangup_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  boolean fret;

  utransfree (qtrans);

  if (qTremote == NULL
      && qTlocal == NULL
      && qTsend == NULL
      && qTreceive == NULL)
    {
      if (! fqueue (qdaemon, (boolean *) NULL))
	return FALSE;

      if (qTlocal == NULL)
	{
	  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: No work");
	  fret = ((*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, 0)
		  && (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY", 0, 0));
	  qdaemon->fhangup = TRUE;
	  return fret;
	}
    }

  DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: Found work");
  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, "HN", 0, 0);
  qdaemon->fmaster = TRUE;
  return fret;
}

/* As described in system.h, we need to keep track of which files have
   been successfully received for which we do not know that the other
   system has received our acknowledgement.  This routine is called to
   keep a list of such files.  */

static struct sreceive_ack *qTfree_receive_ack;

void
usent_receive_ack (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  struct sreceive_ack *q;

  /* We could check the return value here, but if we return FALSE we
     couldn't do anything but drop the connection, which would hardly
     be reasonable.  Instead we trust that the administrator will
     notice and handle any error messages, which are very unlikely to
     occur if everything is set up correctly.  */
  (void) fsysdep_remember_reception (qdaemon->qsys, qtrans->s.zto,
				     qtrans->s.ztemp);

  if (qTfree_receive_ack == NULL)
    q = (struct sreceive_ack *) xmalloc (sizeof (struct sreceive_ack));
  else
    {
      q = qTfree_receive_ack;
      qTfree_receive_ack = q->qnext;
    }

  q->qnext = qTreceive_ack;
  q->zto = zbufcpy (qtrans->s.zto);
  q->ztemp = zbufcpy (qtrans->s.ztemp);
  q->fmarked = FALSE;

  qTreceive_ack = q;
}

/* This routine is called by the protocol code when either all
   outstanding data has been acknowledged or one complete window has
   passed.  It may be called directly by the protocol, or it may be
   called via fgot_data.  If one complete window has passed, then all
   unmarked receives are marked, and we know that all marked ones have
   been acked.  */

void
uwindow_acked (qdaemon, fallacked)
     struct sdaemon *qdaemon;
     boolean fallacked;
{
  register struct sreceive_ack **pq;

  pq = &qTreceive_ack;
  while (*pq != NULL)
    {
      if (fallacked || (*pq)->fmarked)
	{
	  struct sreceive_ack *q;

	  q = *pq;
	  (void) fsysdep_forget_reception (qdaemon->qsys, q->zto,
					   q->ztemp);
	  ubuffree (q->zto);
	  ubuffree (q->ztemp);
	  *pq = q->qnext;
	  q->qnext = qTfree_receive_ack;
	  qTfree_receive_ack = q;
	}
      else
	{
	  (*pq)->fmarked = TRUE;
	  pq = &(*pq)->qnext;
	}
    }
}

/* This routine is called when an error occurred and we are crashing
   out of the connection.  It is only used to report statistics on
   failed transfers to the statistics file.  Note that the number of
   bytes we report as having been sent has little or nothing to do
   with the number of bytes the remote site actually received.  */

void
ustats_failed (qsys)
     const struct uuconf_system *qsys;
{
  register struct stransfer *q;

  if (qTsend != NULL)
    {
      q = qTsend;
      do
	{
	  if (q->fsendfile || q->frecfile)
	    ustats (FALSE, q->s.zuser, qsys->uuconf_zname, q->fsendfile,
		    q->cbytes, q->isecs, q->imicros);
	  q = q->qnext;
	}
      while (q != qTsend);
    }

  if (qTreceive != NULL)
    {
      q = qTreceive;
      do
	{
	  if (q->fsendfile || q->frecfile)
	    ustats (FALSE, q->s.zuser, qsys->uuconf_zname, q->fsendfile,
		    q->cbytes, q->isecs, q->imicros);
	  q = q->qnext;
	}
      while (q != qTreceive);
    }
}
