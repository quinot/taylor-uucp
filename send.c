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
};

/* Local functions.  */

static boolean flocal_send_fail P((struct stransfer *qtrans,
				   struct scmd *qcmd,
				   const struct uuconf_system *qsys,
				   const char *zwhy));
static boolean flocal_send_request P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon));
static boolean flocal_send_await_reply P((struct stransfer *qtrans,
					  struct sdaemon *qdaemon,
					  const char *zdata, size_t cdata));
static boolean flocal_send_open_file P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_fail P((enum tfailure twhy));
static boolean fremote_rec_fail_send P((struct stransfer *qtrans,
					struct sdaemon *qdaemon));
static boolean fremote_rec_reply P((struct stransfer *qtrans,
				    struct sdaemon *qdaemon));
static boolean fsend_file_end P((struct stransfer *qtrans,
				 struct sdaemon *qdaemon));
static boolean fsend_await_confirm P((struct stransfer *qtrans,
				      struct sdaemon *qdaemon,
				      const char *zdata, size_t cdata));

/* Set up a local request to send a file.  This should return FALSE
   only if there is some sort of fatal error.  This may be called
   before we have even tried to call the remote system.  */

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
      zfile = zsysdep_spool_file_name (qsys, qcmd->ztemp);
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

  /* We are now prepared to send the 'S' command to the remote system.
     We queue up a transfer request to send the command when we are
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

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = flocal_send_request;
  qtrans->pinfo = (pointer) qinfo;

  uqueue_local (qtrans);

  return TRUE;
}

/* Report an error for a local send request.  */

static boolean
flocal_send_fail (qtrans, qcmd, qsys, zwhy)
     struct stransfer *qtrans;
     struct scmd *qcmd;
     const struct uuconf_system *qsys;
     const char *zwhy;
{
  if (zwhy != NULL)
    ulog (LOG_ERROR, "%s: %s", qcmd->zfrom, zwhy);
  (void) fmail_transfer (FALSE, qcmd->zuser, (const char *) NULL,
			 zwhy, qcmd->zfrom, (const char *) NULL,
			 qcmd->zto, qsys->uuconf_zname,
			 zsysdep_save_temp_file (qcmd->pseq));
  (void) fsysdep_did_work (qcmd->pseq);
  if (qtrans != NULL)
    {
      struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
    }
  return TRUE;
}

/* This is called when we are ready to send the request to the remote
   system.  We form the request and send it over, and then start
   waiting for the response.  */

static boolean
flocal_send_request (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  size_t clen;
  char *zsend;
  boolean fret;

  /* Make sure the file meets any remote size restrictions.  */
  if (qdaemon->cmax_receive != -1
      && qdaemon->cmax_receive < qinfo->cbytes)
    return flocal_send_fail (qtrans, &qtrans->s, qdaemon->qsys,
			     "too large for receiver");

  /* Send the string
     S zfrom zto zuser zoptions ztemp imode znotify
     to the remote system.  We put a '-' in front of the (possibly
     empty) options and a '0' in front of the mode.  The remote
     system will ignore ztemp, but it is supposed to be sent anyhow.
     If fnew is TRUE, we also send the size; in this case if ztemp
     is empty we must send it as "".  */
  clen = (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
	  + strlen (qtrans->s.zuser) + strlen (qtrans->s.zoptions)
	  + strlen (qtrans->s.ztemp) + strlen (qtrans->s.znotify)
	  + 50);
  zsend = zbufalc (clen);
  if (! qdaemon->fnew)
    sprintf (zsend, "S %s %s %s -%s %s 0%o %s", qtrans->s.zfrom,
	     qtrans->s.zto, qtrans->s.zuser, qtrans->s.zoptions,
	     qtrans->s.ztemp, qtrans->s.imode, qtrans->s.znotify);
  else
    {
      const char *znotify;

      if (qtrans->s.znotify[0] != '\0')
	znotify = qtrans->s.znotify;
      else
	znotify = "\"\"";
      sprintf (zsend, "S %s %s %s -%s %s 0%o %s %ld", qtrans->s.zfrom,
	       qtrans->s.zto, qtrans->s.zuser, qtrans->s.zoptions,
	       qtrans->s.ztemp, qtrans->s.imode, znotify, qinfo->cbytes);
    }

  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, zsend);
  ubuffree (zsend);
  if (! fret)
    {
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return FALSE;
    }

  qtrans->fcmd = TRUE;
  qtrans->psendfn = flocal_send_open_file;
  qtrans->precfn = flocal_send_await_reply;

  /* If we are using a protocol which can make multiple connections,
     then we can open and send the file whenever we are ready.  This
     is because we will be able to distinguish the response by the
     connection is directed to.  This assumes that every protocol
     which supports multiple connections also supports sending the
     file position, since otherwise we would not be able to restart
     files.  */
  if (qdaemon->qproto->cconns > 1)
    uqueue_send (qtrans);
  else
    uqueue_receive (qtrans);

  return TRUE;
}

/* This is called when a reply is received for the send request.  */

static boolean
flocal_send_await_reply (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  qtrans->precfn = NULL;

  if (zdata[0] != 'S'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      ulog (LOG_ERROR, "%s: Bad response to send request: \"%s\"",
	    qtrans->s.zfrom, zdata);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
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
      else
	zerr = "unknown reason";

      if (fnever)
	return flocal_send_fail (qtrans, &qtrans->s, qdaemon->qsys, zerr);

      ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return TRUE;
    }  

  /* A number following the SY is the file position to start sending
     from.  */
  if (zdata[2] != '\0')
    {
      long cskip;

      cskip = strtol (zdata + 2, (char **) NULL, 0);
      if (cskip > 0)
	{
	  if (ffileisopen (qtrans->e))
	    {
	      if (! ffileseek (qtrans->e, cskip))
		{
		  ulog (LOG_ERROR, "seek: %s", strerror (errno));
		  ubuffree (qinfo->zmail);
		  ubuffree (qinfo->zfile);
		  xfree (qtrans->pinfo);
		  utransfree (qtrans);
		  return FALSE;
		}
	    }
	  qtrans->ipos = cskip;
	}
    }

  /* Now queue up to send.  We set psendfn at the end of
     flocal_send_request above.  */
  uqueue_send (qtrans);
  return TRUE;
}

/* Open the file and prepare to send it.  */

static boolean
flocal_send_open_file (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;
  const char *zuser;
  boolean fgone;

  /* If there is an ! in the user name, this is a remote request
     queued up by fremote_xcmd_init.  */
  zuser = qtrans->s.zuser;
  if (strchr (zuser, '!') != NULL)
    zuser = NULL;

  qtrans->e = esysdep_open_send (qdaemon->qsys, qinfo->zfile,
				 ! qinfo->fspool, zuser, &fgone);
  if (! ffileisopen (qtrans->e))
    {
      /* If the file does not exist, fgone will be set to TRUE.  In
	 this case we might have sent the file the last time we talked
	 to the remote system, because we might have been interrupted
	 in the middle of a command file.  To avoid confusion, we
	 don't send a mail message in this case.  */
      if (! fgone)
	(void) fmail_transfer (FALSE, qtrans->s.zuser,
			       (const char *) NULL,
			       "cannot open file",
			       qtrans->s.zfrom, (const char *) NULL,
			       qtrans->s.zto, qdaemon->qsys->uuconf_zname,
			       zsysdep_save_temp_file (qtrans->s.pseq));
      (void) fsysdep_did_work (qtrans->s.pseq);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);

      /* Unfortunately, there is no way (at the moment) to cancel a
	 file send after we've already put it in progress.  So we have
	 to return FALSE to drop the connection.  */
      return FALSE;
    }

  /* Adjust if we're not meant to start at the beginning.  */
  if (qtrans->ipos > 0)
    {
      if (! ffileseek (qtrans->e, qtrans->ipos))
	{
	  ulog (LOG_ERROR, "seek: %s", strerror (errno));
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}
    }

  ulog (LOG_NORMAL, "Sending %s", qtrans->s.zfrom);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes, &fhandled))
	{
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qtrans->fsendfile = TRUE;
  qtrans->psendfn = fsend_file_end;

  uqueue_send (qtrans);

  return TRUE;
}

/* A remote request to receive a file (meaning that we have to send a
   file).  */

boolean
fremote_rec_file_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  const struct uuconf_system *qsys;
  char *zfile;
  long cbytes;
  unsigned int imode;
  openfile_t e;
  struct ssendinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (! qsys->uuconf_fcall_request)
    {
      ulog (LOG_ERROR, "%s: remote system not permitted to request files",
	    qcmd->zfrom);
      return fremote_rec_fail (FAILURE_PERM);
    }

  if (fspool_file (qcmd->zfrom))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", qcmd->zfrom);
      return fremote_rec_fail (FAILURE_PERM);
    }

  zfile = zsysdep_local_file (qcmd->zfrom, qsys->uuconf_zpubdir);
  if (zfile == NULL)
    return fremote_rec_fail (FAILURE_PERM);

  if (! fin_directory_list (zfile, qsys->uuconf_pzremote_send,
			    qsys->uuconf_zpubdir, TRUE, TRUE,
			    (const char *) NULL))
    {
      ulog (LOG_ERROR, "%s: not permitted to send", zfile);
      ubuffree (zfile);
      return fremote_rec_fail (FAILURE_PERM);
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
      return fremote_rec_fail (FAILURE_SIZE);
    }

  imode = isysdep_file_mode (zfile);

  e = esysdep_open_send (qsys, zfile, TRUE, (const char *) NULL,
			 (boolean *) NULL);
  if (! ffileisopen (e))
    {
      ubuffree (zfile);
      return fremote_rec_fail (FAILURE_OPEN);
    }

  qinfo = (struct ssendinfo *) xmalloc (sizeof (struct ssendinfo));
  qinfo->zmail = NULL;
  qinfo->zfile = zfile;
  qinfo->cbytes = cbytes;
  qinfo->flocal = FALSE;
  qinfo->fspool = FALSE;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = fremote_rec_reply;
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
  if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, absend))
    {
      (void) ffileclose (qtrans->e);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return FALSE;
    }

  ulog (LOG_NORMAL, "Sending %s", qtrans->s.zfrom);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, TRUE,
					qinfo->cbytes, &fhandled))
	{
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qtrans->fsendfile = TRUE;
  qtrans->psendfn = fsend_file_end;

  uqueue_send (qtrans);

  return TRUE;
}

/* If we can't send a file as requested by the remote system, queue up
   a failure reply which will be sent when possible.  */

static boolean
fremote_rec_fail (twhy)
     enum tfailure twhy;
{
  enum tfailure *ptinfo;
  struct stransfer *qtrans;

  ptinfo = (enum tfailure *) xmalloc (sizeof (enum tfailure));
  *ptinfo = twhy;

  qtrans = qtransalc ((struct scmd *) NULL);
  qtrans->psendfn = fremote_rec_fail_send;
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
  
  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, z);
  xfree (qtrans->pinfo);
  utransfree (qtrans);
  return fret;
}

/* This is called when the main loop has finished sending a file.  It
   prepares to wait for a response from the remote system.  */

static boolean
fsend_file_end (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct ssendinfo *qinfo = (struct ssendinfo *) qtrans->pinfo;

  /* It is possible that we haven't even had the send request
     confirmed yet, if this is a short file and the protocol supports
     multiple connections.  If so, we just wait for it.  */
  if (qtrans->precfn != NULL)
    {
      uqueue_receive (qtrans);
      return TRUE;
    }

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, FALSE, TRUE,
					(long) -1, &fhandled))
	{
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}

      if (fhandled)
	return TRUE;
    }

  qtrans->fcmd = TRUE;
  qtrans->precfn = fsend_await_confirm;

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

  qtrans->precfn = NULL;

  (void) ffileclose (qtrans->e);

  fnever = FALSE;
  if (zdata[0] != 'C'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      zerr = "bad confirmation from remote";
      ulog (LOG_ERROR, "%s: %s \"%s\"", qinfo->zfile, zerr, zdata);
    }
  else if (zdata[1] == 'N')
    {
      fnever = TRUE;
      if (zdata[2] == '5')
	{
	  zerr = "file could not be stored in final location";
	  ulog (LOG_ERROR, "%s: %s", qinfo->zfile, zerr);
	}
      else
	{
	  zerr = "file send failed for unknown reason";
	  ulog (LOG_ERROR, "%s: %s \"%s\"", qinfo->zfile, zerr, zdata);
	}
    }
  else
    zerr = NULL;

  ustats (zerr == NULL, qtrans->s.zuser, qdaemon->qsys->uuconf_zname,
	  TRUE, qtrans->cbytes, qtrans->isecs, qtrans->imicros);

  if (zerr == NULL)
    {
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

  ubuffree (qinfo->zmail);
  ubuffree (qinfo->zfile);
  xfree (qtrans->pinfo);
  utransfree (qtrans);

  return TRUE;
}
