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

#include "prot.h"
#include "system.h"
#include "trans.h"

/* Local functions.  */

static void utqueue P((struct stransfer **, struct stransfer *));
static void utdequeue P((struct stransfer *));
static void utconnalc P((struct sdaemon *qdaemon, struct stransfer *qtrans));
__inline__ static struct stransfer *qtconn P((int iconn));
__inline__ void utconnfree P((struct stransfer *qtrans));
static boolean ftadd_cmd P((struct sdaemon *qdaemon, const char *z,
			    size_t cdata, boolean flast));
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

/* This number is incremented each time a structure is added to
   qTremote.  This change is detected in ftransfer_loop, where it
   determines whether to break out of the inner loop.  */
static int iTremote;

/* Queue of transfer structures that have been started and want to
   send information.  */
static struct stransfer *qTsend;

/* Number of entries on qTsend.  */
static int cTsend;

/* Queue of transfer structures that have been started and are waiting
   to receive information.  */
static struct stransfer *qTreceive;

/* Number of entries on qTreceive.  */
static int cTreceive;

/* Queue of free transfer structures.  */
static struct stransfer *qTavail;

/* Process time seconds.  */
long iTsecs;

/* Process time microseconds.  */
long iTmicros;

/* Queue up a transfer structure before *pq.  This puts it at the end
   of the list headed by *pq.  */

static void
utqueue (pq, q)
     struct stransfer **pq;
     struct stransfer *q;
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
      if (q->pqqueue == &qTsend)
	--cTsend;
      if (q->pqqueue == &qTreceive)
	--cTreceive;
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
  utqueue (&qTlocal, qtrans);
}

/* Queue up a transfer structure requested by the remote system.  */

void
uqueue_remote (qtrans)
     struct stransfer *qtrans;
{
  utdequeue (qtrans);
  utqueue (&qTremote, qtrans);
  ++iTremote;
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
  utqueue (&qTsend, qtrans);
  ++cTsend;
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
  utqueue (&qTreceive, qtrans);
  ++cTreceive;
}

/* Get a new local connection number.  */

static struct stransfer *aqTconn[IMAX_CONN + 1];

static void
utconnalc (qdaemon, qtrans)
     struct sdaemon *qdaemon;
     struct stransfer *qtrans;
{
  static int iconn;

  if (qdaemon->fhalfduplex)
    {
      qtrans->ilocal = 1;
      return;
    }

  do
    {
      ++iconn;
      if (iconn > qdaemon->qproto->cconns)
	iconn = 1;
    }
  while (aqTconn[iconn] != NULL);

  qtrans->ilocal = iconn;
  aqTconn[iconn] = qtrans;
}

/* Return the transfer for a connection number.  */

__inline__
static struct stransfer *
qtconn (ic)
     int ic;
{
  return aqTconn[ic];
}

/* Clear the connection number for a transfer.  */

__inline__
static void
utconnfree (qt)
     struct stransfer *qt;
{
  if (qt->ilocal != -1)
    {
      aqTconn[qt->ilocal] = NULL;
      qt->ilocal = -1;
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
    q = (struct stransfer *) xmalloc (sizeof (struct stransfer));
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
  q->ilocal = -1;
  q->iremote = -1;
  if (qcmd != NULL)
    {
      q->s = *qcmd;
      q->s.zfrom = zbufcpy (qcmd->zfrom);
      q->s.zto = zbufcpy (qcmd->zto);
      q->s.zuser = zbufcpy (qcmd->zuser);
      q->s.zoptions = zbufcpy (qcmd->zoptions);
      q->s.ztemp = zbufcpy (qcmd->ztemp);
      q->s.znotify = zbufcpy (qcmd->znotify);
    }
  else
    {
      q->s.zfrom = NULL;
      q->s.zto = NULL;
      q->s.zuser = NULL;
      q->s.zoptions = NULL;
      q->s.ztemp = NULL;
      q->s.znotify = NULL;
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
  
  utconnfree (q);    

#if DEBUG > 0
  q->zcmd = NULL;
  q->s.zfrom = NULL;
  q->s.zto = NULL;
  q->s.zuser = NULL;
  q->s.zoptions = NULL;
  q->s.ztemp = NULL;
  q->s.znotify = NULL;
  q->psendfn = NULL;
  q->precfn = NULL;
#endif

  utdequeue (q);
  utqueue (&qTavail, q);
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
  boolean fret;

  iTsecs = isysdep_process_time (&iTmicros);

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
	      if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, "H"))
		{
		  fret = FALSE;
		  break;
		}
	      qdaemon->fmaster = FALSE;
	    }
	}

      while (cTsend + cTreceive < qdaemon->qproto->cconns)
	{
	  /* We have room for an additional connection.  */
	  if (qTremote != NULL)
	    q = qTremote;
	  else if (qTlocal != NULL)
	    q = qTlocal;
	  else
	    break;

	  uqueue_send (q);
	  utconnalc (qdaemon, q);
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
	  long inextsecs, inextmicros;

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
	      int iremote;

	      /* We can read the file in a tight loop until qTremote
		 changes or until we have transferred the entire file.
		 We can disregard any changes to qTlocal since we
		 already have something to send anyhow.  */
	      iremote = iTremote;
	      do
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
							cdata, ipos))
		    {
		      fret = FALSE;
		      break;
		    }

		  /* It is possible that this transfer has just been
		     discarded.  */
		  if (q != qTsend)
		    break;

		  if (cdata == 0)
		    {
		      q->fsendfile = FALSE;
		      if (! (*q->psendfn) (q, qdaemon))
			fret = FALSE;
		      break;
		    }
		}
	      while (iremote == iTremote);

	      if (! fret)
		break;
	    }

	  inextsecs = isysdep_process_time (&inextmicros);
	  if (q == qTsend)
	    {
	      q->isecs += inextsecs - iTsecs;
	      q->imicros += inextmicros - iTmicros;
	    }
	  iTsecs = inextsecs;
	  iTmicros = inextmicros;
	}
    }

  ulog_user ((const char *) NULL);

  (void) (*qdaemon->qproto->pfshutdown) (qdaemon);

  usysdep_get_work_free (qdaemon->qsys);

  if (! fret)
    ustats_failed (qdaemon->qsys);

  return fret;
}

/* This is called by the protocol routines when they have received
   some data.  If pfexit is not NULL, it should be set to TRUE if the
   protocol receive loop should exit back to the main floop routine,
   above.  */

boolean 
fgot_data (qdaemon, zfirst, cfirst, zsecond, csecond, ilocal, iremote, ipos,
	   pfexit)
     struct sdaemon *qdaemon;
     const char *zfirst;
     size_t cfirst;
     const char *zsecond;
     size_t csecond;
     int ilocal;
     int iremote;
     long ipos;
     boolean *pfexit;
{
  struct stransfer *q;
  int cwrote;
  boolean fret;
  long inextsecs, inextmicros;

  if (pfexit != NULL)
    *pfexit = FALSE;

  if (ilocal == -1)
    {
      /* The protocol doesn't know where to route this data.  If
	 there's anything waiting for data, then it gets it.  	
	 Otherwise treat it as a new command.  */
      if (qTreceive != NULL)
	ilocal = qTreceive->ilocal;
      else
	ilocal = 0;
    }

  if (ilocal == 0)
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
			  TRUE);
      else
	{
	  fret = ftadd_cmd (qdaemon, zfirst, cfirst, FALSE);
	  if (fret && csecond > 0)
	    {
	      znull = (const char *) memchr (zsecond, '\0', csecond);
	      if (znull != NULL)
		fret = ftadd_cmd (qdaemon, zsecond,
				  (size_t) (znull - zsecond), TRUE);
	      else
		fret = ftadd_cmd (qdaemon, zsecond, csecond, FALSE);
	    }
	}

      if (pfexit != NULL)
	*pfexit = (qdaemon->fhangup
		   || qdaemon->fmaster
		   || qTsend != NULL
		   || qTremote != NULL);

      /* The time spent to gather a new command does not get charged
	 to any one command.  */
      iTsecs = isysdep_process_time (&iTmicros);

      return fret;
    }

  /* Get the transfer structure this data is intended for.  If there
     is no such transfer structure, we discard the data.  This will
     happen if we reject a file transfer request after some of the
     file has already been transferred.  */
  q = qtconn (ilocal);
  if (q == NULL || q->precfn == NULL)
    {
      DEBUG_MESSAGE1 (DEBUG_UUCP_PROTO, "fgot_data: Discarding %lu bytes",
		      (unsigned long) (cfirst + csecond));
      /* We don't charge this time to anybody.  */
      iTsecs = isysdep_process_time (&iTmicros);
      if (pfexit != NULL)
	*pfexit = (qdaemon->fhangup
		   || qdaemon->fmaster
		   || qTsend != NULL
		   || qTremote != NULL);
      return TRUE;
    }

  ulog_user (q->s.zuser);

#if DEBUG > 0
  if (iremote != q->iremote
      && q->iremote == -1)
    {
      ulog (LOG_ERROR, "fgot_data: %d != %d", iremote, q->iremote);
      return FALSE;
    }
#endif

  q->iremote = iremote;

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
			  ilocal, iremote, ipos + cfirst, pfexit);
    }
  else
    {
      while (cfirst > 0)
	{
	  if (ipos != -1 && ipos != q->ipos)
	    {
	      if (! ffileseek (q->e, ipos))
		{
		  ulog (LOG_ERROR, "seek: %s", strerror (errno));
		  fret = FALSE;
		  break;
		}
	      q->ipos = ipos;
	    }

	  cwrote = cfilewrite (q->e, (char *) zfirst, cfirst);
	  if (cwrote == cfirst)
	    {
	      q->cbytes += cfirst;
	      q->ipos += cfirst;
	    }
	  else
	    {
	      const char *zerr;

	      if (cwrote < 0)
		zerr = strerror (errno);
	      else
		zerr = "could not write all data";
	      ulog (LOG_ERROR, "write: %s", zerr);

	      /* Any write error is almost certainly a temporary
		 condition, or else UUCP would not be functioning at
		 all.  If we continue to accept the file, we will wind
		 up rejecting it at the end (what else could we do?)
		 and the remote system will throw away the request.
		 We're better off just dropping the connection, which
		 is what happens when we return FALSE, and trying
		 again later.  */
	      fret = FALSE;
	      break;
	    }

	  zfirst = zsecond;
	  cfirst = csecond;
	  csecond = 0;
	}
    }

  if (pfexit != NULL)
    *pfexit = (qdaemon->fmaster
	       || qdaemon->fhangup
	       || qTsend != NULL
	       || qTremote != NULL);

  inextsecs = isysdep_process_time (&inextmicros);
  if (q == qtconn (ilocal))
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
ftadd_cmd (qdaemon, z, clen, flast)
     struct sdaemon *qdaemon;
     const char *z;
     size_t clen;
     boolean flast;
{
  static char *zbuf;
  static size_t cbuf;
  static size_t chave;
  size_t cneed;
  struct scmd s;

  cneed = chave + clen + 1;
  if (cneed > cbuf)
    {
      zbuf = (char *) xrealloc ((pointer) zbuf, cneed);
      cbuf = cneed;
    }

  memcpy (zbuf + chave, z, clen);
  zbuf[chave + clen] = '\0';

  if (! flast)
    {
      chave += clen;
      return TRUE;
    }

  /* Don't save this string for next time.  */
  chave = 0;

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
      return fremote_send_file_init (qdaemon, &s);
    case 'R':
      return fremote_rec_file_init (qdaemon, &s);
    case 'X':
      return fremote_xcmd_init (qdaemon, &s);
    case 'H':
      /* This is a remote request for a hangup.  We close the log
	 files so that they may be moved at this point.  */
      ulog_close ();
      ustats_close ();
      {
	struct stransfer *q;

	q = qtransalc ((struct scmd *) NULL);
	q->psendfn = fremote_hangup_reply;
	uqueue_remote (q);
      }
      return TRUE;
    default:
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
      (void) (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY");
      qdaemon->fhangup = TRUE;
      return TRUE;
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

  if (! fqueue (qdaemon, (boolean *) NULL))
    return FALSE;

  if (qTlocal == NULL)
    {
      DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: No work");
      fret = ((*qdaemon->qproto->pfsendcmd) (qdaemon, "HY")
	      && (*qdaemon->qproto->pfsendcmd) (qdaemon, "HY"));
      qdaemon->fhangup = TRUE;
    }
  else
    {
      DEBUG_MESSAGE0 (DEBUG_UUCP_PROTO, "fremote_hangup_reply: Found work");
      fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, "HN");
      qdaemon->fmaster = TRUE;
    }

  utransfree (qtrans);

  return fret;
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
	  if (q->fsendfile)
	    ustats (FALSE, q->s.zuser, qsys->uuconf_zname, TRUE,
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
	  if (q->frecfile)
	    ustats (FALSE, q->s.zuser, qsys->uuconf_zname, FALSE,
		    q->cbytes, q->isecs, q->imicros);
	  q = q->qnext;
	}
      while (q != qTreceive);
    }
}
