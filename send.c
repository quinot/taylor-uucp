/* send.c
   Routines to send a file.

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

#include "uucp.h"

#if USE_RCS_ID
const char send_rcsid[] = "$Id$";
#endif

#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "prot.h"
#include "trans.h"

/* We keep this information in the pinfo field of the stransfer
   structure.  */
struct ssendinfo
{
  /* Local user to send mail to (may be NULL).  */
  char *zmail;
  /* Full file name.  */
  char *zfile;
  /* Number of bytes in file.  */
  long cbytes;
  /* TRUE if this was a local request.  */
  boolean flocal;
  /* TRUE if this is a spool directory file.  */
  boolean fspool;
  /* TRUE if the file has been completely sent.  */
  boolean fsent;
  /* Execution file for sending an unsupported E request.  */
  char *zexec;
};

/* Local functions.  */

static void usfree_send P((struct stransfer *qtrans));
static boolean flocal_send_fail P((struct stransfer *qtrans,
				   struct scmd *qcmd,
				   const struct uuconf_system *qsys,
				   const char *zwhy));
static boolean flocal_send_request P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon));
static boolean flocal_send_await_reply P((struct stransfer *qtrans,
					  struct sdaemon *qdaemon,
					  const char *zdata, size_t cdata));
static boolean flocal_send_cancelled P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean flocal_send_open_file P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_fail P((enum tfailure twhy, int iremote));
static boolean fremote_rec_fail_send P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_reply P((struct stransfer *qtrans,
				    struct sdaemon *qdaemon));
static boolean fsend_file_end P((struct stransfer *qtrans,
				 struct sdaemon *qdaemon));
static boolean fsend_await_confirm P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon,
				      const char *zdata, size_t cdata));
static boolean fsend_exec_file_init P((struct stransfer *qtrans,
				       struct sdaemon *qdaemon));
static void usadd_exec_line P((char **pz, size_t *pcalc, size_t *pclen,
			       int bcmd, const char *z1, const char *z2));
static boolean fsend_exec_file P((struct stransfer *qtrans,
				  struct sdaemon *qdaemon));

/* Free up a send stransfer structure.  */

static void
usfree_send (qtrans)
     struct stransfer *qtrans;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  if (qinfo != NULL)
    {
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      ubuffree (qinfo->zexec);
      xfree (qtrans->pinfo);
    }

  utransfree (qtrans);
}      

/* Set up a local request to send a file.  This may be called before
   we have even tried to call the remote system.

   If we are using a traditional protocol, which doesn't support
   channel numbers and doesn't permit the file to be sent until an
   acknowledgement has been received, the sequence of function calls
   looks like this:

   flocal_send_file_init --> uqueue_local
   flocal_send_request (sends S request) --> uqueue_receive
   flocal_send_await_reply (waits for SY) --> uqueue_send
   flocal_send_open_file (opens file, calls pffile) --> uqueue_send
   send file
   fsend_file_end (calls pffile) --> uqueue_receive
   fsend_await_confirm (waits for CY)

   If flocal_send_await_reply gets an SN, it deletes the request.  If
   the SY reply contains a file position at which to start sending,
   flocal_send_await_reply sets qinfo->ipos.

   This gets more complex if the protocol supports channels.  In that
   case, we want to start sending the file data immediately, to avoid
   the round trip delay between flocal_send_request and
   flocal_send_await_reply.  To do this, flocal_send_request calls
   uqueue_send rather than uqueue_receive.  The main execution
   sequence looks like this:

   flocal_send_file_init --> uqueue_local
   flocal_send_request (sends S request) --> uqueue_send
   flocal_send_open_file (opens file, calls pffile) --> uqueue_send
   send file
   fsend_file_end (calls pffile) --> uqueue_receive
   sometime: flocal_send_await_reply (waits for SY)
   fsend_await_confirm (waits for CY)

   In this case flocal_send_await_reply must be run before
   fsend_await_confirm; it may be run anytime after
   flocal_send_request.

   If flocal_send_await_reply is called before the entire file has
   been sent: if it gets an SN, it calls flocal_send_cancelled to send
   an empty data block to inform the remote system that the file
   transfer has stopped.  If it gets a file position request, it must
   adjust the file position accordingly.

   If flocal_send_await_reply is called after the entire file has been
   sent: if it gets an SN, it can simply delete the request.  It can
   ignore any file position request.

   If the request is not deleted, flocal_send_await_reply must arrange
   for the next string to be passed to fsend_await_confirm.
   Presumably fsend_await_confirm will only be called after the entire
   file has been sent.

   Just to make things even more complex, these same routines support
   sending execution requests, since that is much like sending a file.
   For an execution request, the bcmd character will be E rather than
   S.  If an execution request is being sent to a system which does
   not support them, it must be sent as two S requests instead.  The
   second one will be the execution file, but no actual file is
   created; instead the zexec and znext fields in the ssendinfo
   structure are used.  So if the bcmd character is E, then if the
   zexec field is NULL, the data file is being sent, otherwise the
   fake execution file is being sent.  */

boolean
flocal_send_file_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  const struct uuconf_system *qsys;
  boolean fspool;
  char *zfile;
  long cbytes;
  struct ssendinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (qdaemon->fcaller
      ? ! qsys->uuconf_fcall_transfer
      : ! qsys->uuconf_fcalled_transfer)
    {
      /* uux or uucp should have already made sure that the transfer
	 is possible, but it might have changed since then.  */
      if (! qsys->uuconf_fcall_transfer
	  && ! qsys->uuconf_fcalled_transfer)
	return flocal_send_fail ((struct stransfer *) NULL, qcmd, qsys,
				 "not permitted to transfer files");

      /* We can't do the request now, but it may get done later.  */
      return TRUE;
    }

  /* The 'C' option means that the file has been copied to the spool
     directory.  */
  if (strchr (qcmd->zoptions, 'C') == NULL
      && ! fspool_file (qcmd->zfrom))
    {
      fspool = FALSE;
      if (! fin_directory_list (qcmd->zfrom,
				qsys->uuconf_pzlocal_send,
				qsys->uuconf_zpubdir, TRUE,
				TRUE, qcmd->zuser))
	return flocal_send_fail ((struct stransfer *) NULL, qcmd, qsys,
				 "not permitted to send");

      if (! fsysdep_file_exists (qcmd->zfrom))
	return flocal_send_fail ((struct stransfer *) NULL, qcmd, qsys,
				 "does not exist");

      zfile = zbufcpy (qcmd->zfrom);
    }
  else
    {
      fspool = TRUE;
      zfile = zsysdep_spool_file_name (qsys, qcmd->ztemp, TRUE);
      if (zfile == NULL)
	return FALSE;

      /* If the file does not exist, we obviously can't send it.  This
	 can happen legitimately if it has already been sent.  */
      if (! fsysdep_file_exists (zfile))
	{
	  (void) fsysdep_did_work (qcmd->pseq);
	  return TRUE;
	}
    }

  /* Make sure we meet any local size restrictions.  The connection
     may not have been opened at this point, so we can't check remote
     size restrictions.  */
  cbytes = csysdep_size (zfile);
  if (cbytes != -1
      && qdaemon->clocal_size != -1
      && qdaemon->clocal_size < cbytes)
    {
      if (qdaemon->cmax_ever == -2)
	{
	  long c1, c2;

	  c1 = cmax_size_ever (qsys->uuconf_qcall_local_size);
	  c2 = cmax_size_ever (qsys->uuconf_qcalled_local_size);
	  if (c1 > c2)
	    qdaemon->cmax_ever = c1;
	  else
	    qdaemon->cmax_ever = c2;
	}
		      
      if (qdaemon->cmax_ever != -1
	  && qdaemon->cmax_ever < qcmd->cbytes)
	return flocal_send_fail ((struct stransfer *) NULL, qcmd, qsys,
				 "too large to send");

      return TRUE;
    }

  /* We are now prepared to send the command to the remote system.  We
     queue up a transfer request to send the command when we are
     ready.  */
  qinfo = (struct ssendinfo *) xmalloc (sizeof (struct ssendinfo));
  if (strchr (qcmd->zoptions, 'm') == NULL)
    qinfo->zmail = NULL;
  else
    qinfo->zmail = zbufcpy (qcmd->zuser);
  qinfo->zfile = zfile;
  qinfo->cbytes = cbytes;
  qinfo->flocal = TRUE;
  qinfo->fspool = fspool;
  qinfo->fsent = FALSE;
  qinfo->zexec = NULL;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = flocal_send_request;
  qtrans->pinfo = (pointer) qinfo;

  uqueue_local (qtrans);

  return TRUE;
}

/* Clean up after a failing local send request.  If zwhy is not NULL,
   this reports an error to the log file and to the user.  */

static boolean
flocal_send_fail (qtrans, qcmd, qsys, zwhy)
     struct stransfer *qtrans;
     struct scmd *qcmd;
     const struct uuconf_system *qsys;
     const char *zwhy;
{
  if (zwhy != NULL)
    {
      char *zfree;

      if (qcmd->bcmd != 'E')
	zfree = NULL;
      else
	{
	  zfree = zbufalc (sizeof "Execution of \"\": "
			   + strlen (qcmd->zcmd)
			   + strlen (zwhy));
	  sprintf (zfree, "Execution of \"%s\": %s", qcmd->zcmd, zwhy);
	  zwhy = zfree;
	}

      ulog (LOG_ERROR, "%s: %s", qcmd->zfrom, zwhy);
      (void) fmail_transfer (FALSE, qcmd->zuser, (const char *) NULL,
			     zwhy, qcmd->zfrom, (const char *) NULL,
			     qcmd->zto, qsys->uuconf_zname,
			     zsysdep_save_temp_file (qcmd->pseq));

      ubuffree (zfree);
    }

  (void) fsysdep_did_work (qcmd->pseq);

  if (qtrans != NULL)
    usfree_send (qtrans);

  return TRUE;
}

/* This is called when we are ready to send the request to the remote
   system.  We form the request and send it over.  If the protocol
   does not support multiple channels, we start waiting for the
   response; otherwise we can start sending the file immediately.  */

static boolean
flocal_send_request (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zsend;
  const char *znotify;
  boolean fret;

  /* Make sure the file meets any remote size restrictions.  */
  if (qdaemon->cmax_receive != -1
      && qdaemon->cmax_receive < qinfo->cbytes)
    return flocal_send_fail (qtrans, &qtrans->s, qdaemon->qsys,
			     "too large for receiver");

  /* If this an execution request and the other side supports
     execution requests, we send an E command.  Otherwise we send an S
     command.  The case of an execution request when we are sending
     the fake execution file is handled just like an S request at this
     point.  */
  if (qtrans->s.bcmd == 'E'
      && (qdaemon->ifeatures & FEATURE_EXEC) != 0)
    {
      /* Send the string
	 E zfrom zto zuser zoptions ztemp imode znotify size zcmd
	 to the remote system.  We put a '-' in front of the (possibly
	 empty) options and a '0' in front of the mode.  */
      znotify = qtrans->s.znotify;
      if (znotify == NULL || *znotify == '\0')
	znotify = "\"\"";
      zsend = zbufalc (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
		       + strlen (qtrans->s.zuser)
		       + strlen (qtrans->s.zoptions)
		       + strlen (qtrans->s.ztemp)
		       + strlen (znotify) + strlen (qtrans->s.zcmd)
		       + 50);
      sprintf (zsend, "E %s %s %s -%s %s 0%o %s %ld %s", qtrans->s.zfrom,
	       qtrans->s.zto, qtrans->s.zuser, qtrans->s.zoptions,
	       qtrans->s.ztemp, qtrans->s.imode, znotify, qinfo->cbytes,
	       qtrans->s.zcmd);
    }
  else
    {
      const char *zoptions;

      /* Send the string
	 S zfrom zto zuser zoptions ztemp imode znotify
	 to the remote system.  We put a '-' in front of the (possibly
	 empty) options and a '0' in front of the mode.  If size
	 negotiation is supported, we also send the size; in this case
	 if znotify is empty we must send it as "".  If this is really
	 an execution request, we have to simplify the options string
	 to remove the various execution options which may confuse the
	 remote system.  */
      if (qtrans->s.bcmd != 'E')
	zoptions = qtrans->s.zoptions;
      else if (strchr (qtrans->s.zoptions, 'C') != NULL)
	zoptions = "C";
      else
	zoptions = "c";

      znotify = qtrans->s.znotify;
      if (znotify == NULL)
	znotify = "";
      zsend = zbufalc (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
		       + strlen (qtrans->s.zuser) + strlen (zoptions)
		       + strlen (qtrans->s.ztemp) + strlen (znotify) + 50);
      if ((qdaemon->ifeatures & FEATURE_SIZES) == 0)
	sprintf (zsend, "S %s %s %s -%s %s 0%o %s", qtrans->s.zfrom,
		 qtrans->s.zto, qtrans->s.zuser, zoptions,
		 qtrans->s.ztemp, qtrans->s.imode, znotify);
      else
	{
	  if (*znotify == '\0')
	    znotify = "\"\"";
	  sprintf (zsend, "S %s %s %s -%s %s 0%o %s %ld", qtrans->s.zfrom,
		   qtrans->s.zto, qtrans->s.zuser, zoptions,
		   qtrans->s.ztemp, qtrans->s.imode, znotify,
		   qinfo->cbytes);
	}
    }

  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, zsend, qtrans->ilocal,
					qtrans->iremote);
  ubuffree (zsend);
  if (! fret)
    {
      usfree_send (qtrans);
      return FALSE;
    }

  /* If we are using a protocol which can make multiple channels, then
     we can open and send the file whenever we are ready.  This is
     because we will be able to distinguish the response by the
     channel it is directed to.  This assumes that every protocol
     which supports multiple channels also supports sending the file
     position in mid-stream, since otherwise we would not be able to
     restart files.  */
  qtrans->fcmd = TRUE;
  qtrans->psendfn = flocal_send_open_file;
  qtrans->precfn = flocal_send_await_reply;

  if (qdaemon->qproto->cchans > 1)
    uqueue_send (qtrans);
  else
    uqueue_receive (qtrans);

  return TRUE;
}

/* This is called when a reply is received for the send request.  As
   described at length above, if the protocol supports multiple
   channels we may be in the middle of sending the file, or we may
   even finished sending the file.  */

static boolean
flocal_send_await_reply (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char bcmd;

  if (qtrans->s.bcmd == 'E'
      && (qdaemon->ifeatures & FEATURE_EXEC) != 0)
    bcmd = 'E';
  else
    bcmd = 'S';
  if (zdata[0] != bcmd
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      ulog (LOG_ERROR, "%s: Bad response to %c request: \"%s\"",
	    qtrans->s.zfrom, bcmd, zdata);
      usfree_send (qtrans);
      return FALSE;
    }

  if (zdata[1] == 'N')
    {
      const char *zerr;
      boolean fnever;

      fnever = TRUE;
      if (zdata[2] == '2')
	zerr = "permission denied";
      else if (zdata[2] == '4')
	{
	  zerr = "remote cannot create work files";
	  fnever = FALSE;
	}
      else if (zdata[2] == '6')
	{
	  zerr = "too large for receiver now";
	  fnever = FALSE;
	}
      else if (zdata[2] == '7')
	{
	  /* The file is too large to ever send.  */
	  zerr = "too large for receiver";
	}
      else if (zdata[2] == '8')
	{
	  /* The file was already received by the remote system.  This
	     is not an error, it just means that the ack from the
	     remote was lost in the previous conversation, and there
	     is no need to resend the file.  */
	  zerr = NULL;
	}
      else
	zerr = "unknown reason";

      if (! fnever)
	{
	  if (qtrans->s.bcmd == 'E')
	    ulog (LOG_ERROR, "Execution of \"%s\": %s", qtrans->s.zcmd,
		  zerr);
	  else
	    ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);
	}
      else
	{
	  if (! flocal_send_fail ((struct stransfer *) NULL, &qtrans->s,
				  qdaemon->qsys, zerr))
	    return FALSE;
	}

      /* If the protocol does not support multiple channels, we can
	 simply remove the transaction.  Otherwise we must make sure
	 the remote side knows that we have finished sending the file
	 data.  If we have already sent the entire file, there will be
	 no confusion.  */
      if (qdaemon->qproto->cchans == 1 || qinfo->fsent)
	usfree_send (qtrans);
      else
	{
	  qtrans->psendfn = flocal_send_cancelled;
	  qtrans->precfn = NULL;
	  qtrans->fsendfile = FALSE;
	  uqueue_send (qtrans);
	}

      return TRUE;
    }

  /* A number following the SY or EY is the file position to start
     sending from.  If we are already sending the file, we must set
     the position accordingly.  */
  if (zdata[2] != '\0')
    {
      long cskip;

      cskip = strtol (zdata + 2, (char **) NULL, 0);
      if (cskip > 0 && qtrans->ipos < cskip)
	{
	  if (qtrans->fsendfile && ! qinfo->fsent)
	    {
	      if (! ffileseek (qtrans->e, cskip))
		{
		  ulog (LOG_ERROR, "seek: %s", strerror (errno));
		  usfree_send (qtrans);
		  return FALSE;
		}
	    }
	  qtrans->ipos = cskip;
	}
    }

  /* Now queue up to send the file or to wait for the confirmation.
     We already set psendfn at the end of flocal_send_request.  If the
     protocol supports multiple channels, we have already called
     uqueue_send; calling it again would move the request in the
     queue, which would make the log file a bit confusing.  */
  qtrans->precfn = fsend_await_confirm;
  if (qinfo->fsent)
    uqueue_receive (qtrans);
  else if (qdaemon->qproto->cchans <= 1)
    uqueue_send (qtrans);

  return TRUE;
}

/* Open the file, if any, and prepare to send it.  */

static boolean
flocal_send_open_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  const char *zuser;

  /* If this is not a fake execution file, open it.  */
  if (qinfo->zexec == NULL)
    {
      /* If there is an ! in the user name, this is a remote request
	 queued up by fremote_xcmd_init.  */
      zuser = qtrans->s.zuser;
      if (strchr (zuser, '!') != NULL)
	zuser = NULL;

      qtrans->e = esysdep_open_send (qdaemon->qsys, qinfo->zfile,
				     ! qinfo->fspool, zuser);
      if (! ffileisopen (qtrans->e))
	{
	  (void) fmail_transfer (FALSE, qtrans->s.zuser,
				 (const char *) NULL,
				 "cannot open file",
				 qtrans->s.zfrom, (const char *) NULL,
				 qtrans->s.zto,
				 qdaemon->qsys->uuconf_zname,
				 zsysdep_save_temp_file (qtrans->s.pseq));
	  (void) fsysdep_did_work (qtrans->s.pseq);
	  usfree_send (qtrans);

	  /* Unfortunately, there is no way to cancel a file send
	     after we've already put it in progress.  So we have to
	     return FALSE to drop the connection.  */
	  return FALSE;
	}
    }

  /* If flocal_send_await_reply has received a reply with a file
     position, it will have set qtrans->ipos to the position at which
     to start.  */
  if (qtrans->ipos > 0)
    {
      if (qinfo->zexec != NULL)
	{
	  if (qtrans->ipos > qtrans->cbytes)
	    qtrans->ipos = qtrans->cbytes;
	}
      else
	{
	  if (! ffileseek (qtrans->e, qtrans->ipos))
	    {
	      ulog (LOG_ERROR, "seek: %s", strerror (errno));
	      usfree_send (qtrans);
	      return FALSE;
	    }
	}
    }

  /* We don't bother to log sending the execution file.  */
  if (qinfo->zexec == NULL)
    {
      const char *zsend;

      if (qtrans->s.bcmd == 'E')
	zsend = qtrans->s.zcmd;
      else
	zsend = qtrans->s.zfrom;
      qtrans->zlog = zbufalc (sizeof "Sending " + strlen (zsend));
      sprintf (qtrans->zlog, "Sending %s", zsend);
    }

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes, &fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  if (qinfo->zexec != NULL)
    qtrans->psendfn = fsend_exec_file;
  else
    {
      qtrans->fsendfile = TRUE;
      qtrans->psendfn = fsend_file_end;
    }

  uqueue_send (qtrans);

  return TRUE;
}

/* Cancel a file send by sending an empty buffer.  This is only called
   for a protocol which supports multiple channels.  It is needed
   so that both systems agree as to when a channel is no longer
   needed.  */

static boolean
flocal_send_cancelled (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  char *zdata;
  size_t cdata;
  boolean fret;
  
  zdata = (*qdaemon->qproto->pzgetspace) (qdaemon, &cdata);
  if (zdata == NULL)
    {
      usfree_send (qtrans);
      return FALSE;
    }

  fret = (*qdaemon->qproto->pfsenddata) (qdaemon, zdata, (size_t) 0,
					 qtrans->ilocal, qtrans->iremote,
					 qtrans->ipos);
  usfree_send (qtrans);
  return fret;
}

/* A remote request to receive a file (meaning that we have to send a
   file).  The sequence of functions calls is as follows:

   fremote_rec_file_init (open file) --> uqueue_remote
   fremote_rec_reply (send RY, call pffile) --> uqueue_send
   send file
   fsend_file_end (calls pffile) --> uqueue_receive
   fsend_await_confirm (waits for CY)
   */

boolean
fremote_rec_file_init (qdaemon, qcmd, iremote)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
     int iremote;
{
  const struct uuconf_system *qsys;
  char *zfile;
  long cbytes;
  unsigned int imode;
  openfile_t e;
  struct ssendinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (! qsys->uuconf_fsend_request)
    {
      ulog (LOG_ERROR, "%s: not permitted to send files to remote",
	    qcmd->zfrom);
      return fremote_rec_fail (FAILURE_PERM, iremote);
    }

  if (fspool_file (qcmd->zfrom))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", qcmd->zfrom);
      return fremote_rec_fail (FAILURE_PERM, iremote);
    }

  zfile = zsysdep_local_file (qcmd->zfrom, qsys->uuconf_zpubdir);
  if (zfile != NULL)
    {
      char *zbased;

      zbased = zsysdep_add_base (zfile, qcmd->zto);
      ubuffree (zfile);
      zfile = zbased;
    }
  if (zfile == NULL)
    return fremote_rec_fail (FAILURE_PERM, iremote);

  if (! fin_directory_list (zfile, qsys->uuconf_pzremote_send,
			    qsys->uuconf_zpubdir, TRUE, TRUE,
			    (const char *) NULL))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", zfile);
      ubuffree (zfile);
      return fremote_rec_fail (FAILURE_PERM, iremote);
    }

  /* If the file is larger than the amount of space the other side
     reported, we can't send it.  */
  cbytes = csysdep_size (zfile);
  if (cbytes != -1
      && ((qcmd->cbytes != -1 && qcmd->cbytes < cbytes)
	  || (qdaemon->cremote_size != -1
	      && qdaemon->cremote_size < cbytes)
	  || (qdaemon->cmax_receive != -1
	      && qdaemon->cmax_receive < cbytes)))
    {
      ulog (LOG_ERROR, "%s: too large to send", zfile);
      ubuffree (zfile);
      return fremote_rec_fail (FAILURE_SIZE, iremote);
    }

  imode = isysdep_file_mode (zfile);

  e = esysdep_open_send (qsys, zfile, TRUE, (const char *) NULL);
  if (! ffileisopen (e))
    {
      ubuffree (zfile);
      return fremote_rec_fail (FAILURE_OPEN, iremote);
    }

  qinfo = (struct ssendinfo *) xmalloc (sizeof (struct ssendinfo));
  qinfo->zmail = NULL;
  qinfo->zfile = zfile;
  qinfo->cbytes = cbytes;
  qinfo->flocal = FALSE;
  qinfo->fspool = FALSE;
  qinfo->fsent = FALSE;
  qinfo->zexec = NULL;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = fremote_rec_reply;
  qtrans->iremote = iremote;
  qtrans->pinfo = (pointer) qinfo;
  qtrans->e = e;
  qtrans->s.imode = imode;

  uqueue_remote (qtrans);

  return TRUE;
}

/* Reply to a receive request from the remote system, and prepare to
   start sending the file.  */

static boolean
fremote_rec_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char absend[20];

  sprintf (absend, "RY 0%o", qtrans->s.imode);
  if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, absend, qtrans->ilocal,
				       qtrans->iremote))
    {
      (void) ffileclose (qtrans->e);
      usfree_send (qtrans);
      return FALSE;
    }

  qtrans->zlog = zbufalc (sizeof "Sending " + strlen (qtrans->s.zfrom));
  sprintf (qtrans->zlog, "Sending %s", qtrans->s.zfrom);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes, &fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qtrans->fsendfile = TRUE;
  qtrans->psendfn = fsend_file_end;
  qtrans->precfn = fsend_await_confirm;

  uqueue_send (qtrans);

  return TRUE;
}

/* If we can't send a file as requested by the remote system, queue up
   a failure reply which will be sent when possible.  */

static boolean
fremote_rec_fail (twhy, iremote)
     enum tfailure twhy;
     int iremote;
{
  enum tfailure *ptinfo;
  struct stransfer *qtrans;

  ptinfo = (enum tfailure *) xmalloc (sizeof (enum tfailure));
  *ptinfo = twhy;

  qtrans = qtransalc ((struct scmd *) NULL);
  qtrans->psendfn = fremote_rec_fail_send;
  qtrans->iremote = iremote;
  qtrans->pinfo = (pointer) ptinfo;

  uqueue_remote (qtrans);

  return TRUE;
}

/* Send a failure string for a receive command to the remote system;
   this is called when we are ready to reply to the command.  */

static boolean
fremote_rec_fail_send (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  enum tfailure *ptinfo = (enum tfailure *) qtrans->pinfo;
  const char *z;
  boolean fret;

  switch (*ptinfo)
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
  
  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, z, qtrans->ilocal,
					qtrans->iremote);
  xfree (qtrans->pinfo);
  utransfree (qtrans);
  return fret;
}

/* This is called when the main loop has finished sending a file.  It
   prepares to wait for a response from the remote system.  Note that
   if this is a local request and the protocol supports multiple
   channels, we may not even have received a confirmation of the send
   request.  */

static boolean
fsend_file_end (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, FALSE, TRUE,
					(long) -1, &fhandled))
	{
	  usfree_send (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qinfo->fsent = TRUE;

  /* qtrans->precfn should have been set by a previous function.  */
  qtrans->fcmd = TRUE;
  uqueue_receive (qtrans);

  return TRUE;
}

/* Handle the confirmation string received after sending a file.  */

/*ARGSUSED*/
static boolean
fsend_await_confirm (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  boolean fnever;
  const char *zerr;

  if (qinfo->zexec == NULL)
    (void) ffileclose (qtrans->e);

  fnever = FALSE;
  if (zdata[0] != 'C'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      zerr = "bad confirmation from remote";
      ulog (LOG_ERROR, "%s: %s \"%s\"", qtrans->s.zfrom, zerr, zdata);
    }
  else if (zdata[1] == 'N')
    {
      fnever = TRUE;
      if (zdata[2] == '5')
	{
	  zerr = "file could not be stored in final location";
	  ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);
	}
      else
	{
	  zerr = "file send failed for unknown reason";
	  ulog (LOG_ERROR, "%s: %s \"%s\"", qtrans->s.zfrom, zerr, zdata);
	}
    }
  else
    zerr = NULL;

  ustats (zerr == NULL, qtrans->s.zuser, qdaemon->qsys->uuconf_zname,
	  TRUE, qtrans->cbytes, qtrans->isecs, qtrans->imicros);

  if (zerr == NULL)
    {
      /* If this is an execution request, and the remote system
	 doesn't support execution requests, we have to set up the
	 fake execution file and loop around again.  */
      if (qtrans->s.bcmd == 'E'
	  && (qdaemon->ifeatures & FEATURE_EXEC) == 0
	  && qinfo->zexec == NULL)
	return fsend_exec_file_init (qtrans, qdaemon);

      /* Send mail about the transfer if requested.  */
      if (qinfo->zmail != NULL && *qinfo->zmail != '\0')
	(void) fmail_transfer (TRUE, qtrans->s.zuser, qinfo->zmail,
			       (const char *) NULL,
			       qtrans->s.zfrom, (const char *) NULL,
			       qtrans->s.zto, qdaemon->qsys->uuconf_zname,
			       (const char *) NULL);

      if (qtrans->s.pseq != NULL)
	(void) fsysdep_did_work (qtrans->s.pseq);
    }
  else
    {
      /* If the file send failed, we only try to save the file and
	 send mail if it was requested locally and it will never
	 succeed.  We send mail to qinfo->zmail if set, otherwise to
	 qtrans->s.zuser.  I hope this is reasonable.  */
      if (fnever && qinfo->flocal)
	{
	  (void) fmail_transfer (FALSE, qtrans->s.zuser, qinfo->zmail,
				 zerr, qtrans->s.zfrom, (const char *) NULL,
				 qtrans->s.zto, qdaemon->qsys->uuconf_zname,
				 zsysdep_save_temp_file (qtrans->s.pseq));
	  (void) fsysdep_did_work (qtrans->s.pseq);
	}
    }

  usfree_send (qtrans);

  return TRUE;
}

/* Prepare to send an execution file to a system which does not
   support execution requests.  We build the execution file in memory,
   and then call flocal_send_request as though we were sending a real
   file.  Instead of sending a file, the code in flocal_send_open_file
   will arrange to call fsend_exec_file which will send data out of
   the buffer we have created.  */

static boolean
fsend_exec_file_init (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zxqtfile;
  char abtname[CFILE_NAME_LEN];
  char abxname[CFILE_NAME_LEN];
  char *z;
  size_t calc, clen;

  z = NULL;
  calc = 0;
  clen = 0;

  usadd_exec_line (&z, &calc, &clen, 'U', qtrans->s.zuser,
		   qdaemon->zlocalname);
  usadd_exec_line (&z, &calc, &clen, 'C', qtrans->s.zcmd, "");
  usadd_exec_line (&z, &calc, &clen, 'F', qtrans->s.zto, "");
  usadd_exec_line (&z, &calc, &clen, 'I', qtrans->s.zto, "");
  if (strchr (qtrans->s.zoptions, 'N') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'N', "", "");
  if (strchr (qtrans->s.zoptions, 'Z') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'Z', "", "");
  if (strchr (qtrans->s.zoptions, 'R') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'R', qtrans->s.znotify, "");
  if (strchr (qtrans->s.zoptions, 'e') != NULL)
    usadd_exec_line (&z, &calc, &clen, 'e', "", "");

  qinfo->zexec = z;
  qinfo->cbytes = clen;

  zxqtfile = zsysdep_data_file_name (qdaemon->qsys, qdaemon->zlocalname,
				     'X', abtname, (char *) NULL,
				     abxname);
  if (zxqtfile == NULL)
    {
      usfree_send (qtrans);
      return FALSE;
    }

  ubuffree ((char *) qtrans->s.zfrom);
  qtrans->s.zfrom = zbufcpy (abtname);
  ubuffree ((char *) qtrans->s.zto);
  qtrans->s.zto = zbufcpy (abxname);
  ubuffree ((char *) qtrans->s.zoptions);
  qtrans->s.zoptions = zbufcpy ("C");
  ubuffree ((char *) qtrans->s.ztemp);
  qtrans->s.ztemp = zbufcpy (abtname);

  qtrans->psendfn = flocal_send_request;
  qtrans->precfn = NULL;
  qtrans->ipos = 0;

  uqueue_send (qtrans);

  return TRUE;
}

/* Add a line to the fake execution file.  */

static void
usadd_exec_line (pz, pcalc, pclen, bcmd, z1, z2)
     char **pz;
     size_t *pcalc;
     size_t *pclen;
     int bcmd;
     const char *z1;
     const char *z2;
{
  size_t cadd;

  cadd = 4 + strlen (z1) + strlen (z2);

  if (*pclen + cadd + 1 >= *pcalc)
    {
      char *znew;

      *pcalc += cadd + 100;
      znew = zbufalc (*pcalc);
      if (*pclen > 0)
	{
	  memcpy (znew, *pz, *pclen);
	  ubuffree (*pz);
	}
      *pz = znew;
    }

  /* In some bizarre non-Unix case we might have to worry about the
     newline here.  We don't know how a newline is normally written
     out to a file, but whatever is written to a file is what we will
     normally transfer.  If that is not simply \n then this fake
     execution file will not look like other execution files.  */
  sprintf (*pz + *pclen, "%c %s %s\n", bcmd, z1, z2);

  *pclen += cadd;
}

/* This routine is called to send the contents of the fake execution
   file.  Normally file data is sent by the floop routine in trans.c,
   but since we don't have an actual file we must do it here.  This
   routine sends the complete buffer, followed by a zero length
   packet, and then calls fsend_file_end.  */

static boolean
fsend_exec_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  char *zdata;
  size_t cdata;
  size_t csend;

  zdata = (*qdaemon->qproto->pzgetspace) (qdaemon, &cdata);
  if (zdata == NULL)
    {
      usfree_send (qtrans);
      return FALSE;
    }

  csend = qinfo->cbytes - qtrans->ipos;
  if (csend > cdata)
    csend = cdata;

  memcpy (zdata, qinfo->zexec + qtrans->ipos, csend);

  if (! (*qdaemon->qproto->pfsenddata) (qdaemon, zdata, csend,
					qtrans->ilocal, qtrans->iremote,
					qtrans->ipos))
    {
      usfree_send (qtrans);
      return FALSE;
    }

  qtrans->ipos += csend;

  if (csend == 0)
    return fsend_file_end (qtrans, qdaemon);

  /* Leave the job on the send queue.  */

  return TRUE;
}
