/* rec.c
   Routines to receive a file.

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
const char rec_rcsid[] = "$Id$";
#endif

#include <errno.h>

#include "system.h"
#include "prot.h"
#include "trans.h"

/* We keep this information in the pinfo field of the stransfer
   structure.  */
struct srecinfo
{
  /* Local user to send mail to (may be NULL).  */
  char *zmail;
  /* Full file name.  */
  char *zfile;
  /* TRUE if this is a spool directory file.  */
  boolean fspool;
  /* TRUE if this was a local request.  */
  boolean flocal;
};

/* Local functions.  */

static boolean flocal_rec_fail P((struct stransfer *qtrans,
				  struct scmd *qcmd,
				  const struct uuconf_system *qsys,
				  const char *zwhy));
static boolean flocal_rec_send_request P((struct stransfer *qtrans,
					  struct sdaemon *qdaemon));
static boolean flocal_rec_await_reply P((struct stransfer *qtrans,
					 struct sdaemon *qdaemon,
					 const char *zdata,
					 size_t cdata));
static boolean fremote_send_reply P((struct stransfer *qtrans,
				     struct sdaemon *qdaemon));
static boolean fremote_send_fail P((enum tfailure twhy));
static boolean fremote_send_fail_send P((struct stransfer *qtrans,
					 struct sdaemon *qdaemon));
static boolean frec_file_end P((struct stransfer *qtrans,
				struct sdaemon *qdaemon,
				const char *zdata, size_t cdata));

/* Set up a request for a file from the remote system.  */

boolean
flocal_rec_file_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  const struct uuconf_system *qsys;
  boolean fspool;
  char *zfile;
  struct srecinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  /* Make sure we are permitted to transfer files.  */
  if (qdaemon->fcaller
      ? ! qsys->uuconf_fcall_transfer
      : ! qsys->uuconf_fcalled_transfer)
    {
      /* This case will have been checked by uucp or uux, but it could
	 have changed.  */
      if (! qsys->uuconf_fcall_transfer
	  && ! qsys->uuconf_fcalled_transfer)
	return flocal_rec_fail ((struct stransfer *) NULL, qcmd, qsys,
				"not permitted to request files");
      return TRUE;
    }

  fspool = fspool_file (qcmd->zto);

  if (fspool)
    {
      /* Normal users are not allowed to receive files in the spool
	 directory, and to make it particularly difficult we require a
	 special option '9'.  This is used only by uux when a file
	 must be requested from one system and then used for a local
	 execution.  */
      if (qcmd->zto[0] != 'D'
	  || strchr (qcmd->zoptions, '9') == NULL)
	return flocal_rec_fail ((struct stransfer *) NULL, qcmd, qsys,
				"not permitted to receive");

      zfile = zsysdep_spool_file_name (qsys, qcmd->zto);
      if (zfile == NULL)
	return FALSE;
    }
  else
    {
      zfile = zsysdep_add_base (qcmd->zto, qcmd->zfrom);
      if (zfile == NULL)
	return FALSE;

      /* Check permissions.  */
      if (! fin_directory_list (zfile, qsys->uuconf_pzlocal_receive,
				qsys->uuconf_zpubdir, TRUE,
				FALSE, qcmd->zuser))
	{
	  ubuffree (zfile);
	  return flocal_rec_fail ((struct stransfer *) NULL, qcmd, qsys,
				  "not permitted to receive");
	}

      /* The 'f' option means that directories should not
	 be created if they do not already exist.  */
      if (strchr (qcmd->zoptions, 'f') == NULL)
	{
	  if (! fsysdep_make_dirs (zfile, TRUE))
	    {
	      ubuffree (zfile);
	      return flocal_rec_fail ((struct stransfer *) NULL, qcmd,
				      qsys, "cannot create directories");
	    }
	}
    }

  qinfo = (struct srecinfo *) xmalloc (sizeof (struct srecinfo));
  if (strchr (qcmd->zoptions, 'm') == NULL)
    qinfo->zmail = NULL;
  else
    qinfo->zmail = zbufcpy (qcmd->zuser);
  qinfo->zfile = zfile;
  qinfo->fspool = fspool;
  qinfo->flocal = TRUE;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = flocal_rec_send_request;
  qtrans->pinfo = (pointer) qinfo;
  qtrans->s.ztemp = NULL;

  uqueue_local (qtrans);

  return TRUE;
}

/* Report an error for a local receive request.  */

static boolean
flocal_rec_fail (qtrans, qcmd, qsys, zwhy)
     struct stransfer *qtrans;
     struct scmd *qcmd;
     const struct uuconf_system *qsys;
     const char *zwhy;
{
  if (zwhy != NULL)
    {
      ulog (LOG_ERROR, "%s: %s", qcmd->zfrom, zwhy);
      (void) fmail_transfer (FALSE, qcmd->zuser, (const char *) NULL, zwhy,
			     qcmd->zfrom, qsys->uuconf_zname,
			     qcmd->zto, (const char *) NULL,
			     (const char *) NULL);
      (void) fsysdep_did_work (qcmd->pseq);
    }
  if (qtrans != NULL)
    {
      struct srecinfo *qinfo = (struct srecinfo *) qtrans->pinfo;

      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
    }
  return TRUE;
}

/* This is called when we are ready to send the actual request to the
   other system.  */

static boolean
flocal_rec_send_request (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct srecinfo *qinfo = (struct srecinfo *) qtrans->pinfo;
  long cbytes;
  size_t clen;
  char *zsend;
  boolean fret;

  qtrans->s.ztemp = zsysdep_receive_temp (qdaemon->qsys, qinfo->zfile,
					  &cbytes);
  if (qtrans->s.ztemp == NULL)
    {
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return FALSE;
    }

  if (cbytes != -1)
    {
      cbytes -= qdaemon->qsys->uuconf_cfree_space;
      if (cbytes < 0)
	cbytes = 0;
    }

  if (qdaemon->clocal_size != -1
      && (cbytes == -1 || qdaemon->clocal_size < cbytes))
    cbytes = qdaemon->clocal_size;

  /* We send the string
     R from to user options

     We put a dash in front of options.  If we are talking to a
     counterpart, we also send the maximum size file we are prepared
     to accept, as returned by esysdep_open_receive.  */
  clen = (strlen (qtrans->s.zfrom) + strlen (qtrans->s.zto)
	  + strlen (qtrans->s.zuser) + strlen (qtrans->s.zoptions) + 30);
  zsend = zbufalc (clen);
  if (! qdaemon->fnew)
    sprintf (zsend, "R %s %s %s -%s", qtrans->s.zfrom, qtrans->s.zto,
	     qtrans->s.zuser, qtrans->s.zoptions);
  else
    sprintf (zsend, "R %s %s %s -%s %ld", qtrans->s.zfrom, qtrans->s.zto,
	     qtrans->s.zuser, qtrans->s.zoptions, cbytes);

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
  qtrans->precfn = flocal_rec_await_reply;

  uqueue_receive (qtrans);

  return TRUE;
}

/* This is called when a reply is received for the request.  */

/*ARGSUSED*/
static boolean
flocal_rec_await_reply (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct srecinfo *qinfo = (struct srecinfo *) qtrans->pinfo;

  qtrans->precfn = NULL;

  if (zdata[0] != 'R'
      || (zdata[1] != 'Y' && zdata[1] != 'N'))
    {
      ulog (LOG_ERROR, "%s: bad response to receive request: \"%s\"",
	    qtrans->s.zfrom, zdata);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return FALSE;
    }

  if (zdata[1] == 'N')
    {
      boolean fnever;
      const char *zerr;

      fnever = TRUE;
      if (zdata[2] == '2')
	zerr = "no such file";
      else if (zdata[2] == '6')
	{
	  /* We sent over the maximum file size we were prepared to
	     receive, and the remote system is telling us that the
	     file is larger than that.  Try again later.  It would be
	     better if we could know whether there will ever be enough
	     room.  */
	  zerr = "too large to receive now";
	  fnever = FALSE;
	}
      else
	zerr = "unknown reason";

      if (fnever)
	return flocal_rec_fail (qtrans, &qtrans->s, qdaemon->qsys, zerr);

      ulog (LOG_ERROR, "%s: %s", qtrans->s.zfrom, zerr);

      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);

      return TRUE;
    }

  /* The mode should have been sent as "RY 0%o".  If it wasn't, we use
     0666.  */
  qtrans->s.imode = (unsigned int) strtol (zdata + 2, (char **) NULL, 8);
  if (qtrans->s.imode == 0)
    qtrans->s.imode = 0666;

  qtrans->e = esysdep_open_receive (qdaemon->qsys, qinfo->zfile,
				    qtrans->s.ztemp);
  if (! ffileisopen (qtrans->e))
    return flocal_rec_fail (qtrans, &qtrans->s, qdaemon->qsys,
			    "cannot open file");

  ulog (LOG_NORMAL, "Receiving %s",
	qinfo->fspool ? qtrans->s.zto : qinfo->zfile);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, FALSE,
					(long) -1, &fhandled))
	{
	  (void) ffileclose (qtrans->e);
	  return flocal_rec_fail (qtrans, &qtrans->s, qdaemon->qsys,
				  (const char *) NULL);
	}
      if (fhandled)
	return TRUE;
    }

  qtrans->frecfile = TRUE;
  qtrans->precfn = frec_file_end;

  uqueue_receive (qtrans);

  return TRUE;
}

/* A remote request to send a file to the local system.  */

boolean
fremote_send_file_init (qdaemon, qcmd)
     struct sdaemon *qdaemon;
     struct scmd *qcmd;
{
  const struct uuconf_system *qsys;
  boolean fspool;
  char *zfile;
  FILE *e;
  char *ztemp;
  long cbytes;
  struct srecinfo *qinfo;
  struct stransfer *qtrans;

  qsys = qdaemon->qsys;

  if (! qsys->uuconf_fcall_request)
    {
      ulog (LOG_ERROR, "%s: remote system not permitted to send files",
	    qcmd->zfrom);
      return fremote_send_fail (FAILURE_PERM);
    }
		  
  fspool = fspool_file (qcmd->zto);

  if (fspool)
    {
      /* We don't accept remote command files.  */
      if (qcmd->zto[0] == 'C')
	{
	  ulog (LOG_ERROR, "%s: not permitted to receive", qcmd->zfrom);
	  return fremote_send_fail (FAILURE_PERM);
	}

      zfile = zsysdep_spool_file_name (qsys, qcmd->zto);
      if (zfile == NULL)
	return FALSE;
    }
  else
    {
      zfile = zsysdep_local_file (qcmd->zto, qsys->uuconf_zpubdir);
      if (zfile != NULL)
	{
	  char *zadd;

	  zadd = zsysdep_add_base (zfile, qcmd->zfrom);
	  ubuffree (zfile);
	  zfile = zadd;
	}
      if (zfile == NULL)
	return FALSE;

      /* Check permissions.  */
      if (! fin_directory_list (zfile, qsys->uuconf_pzremote_receive,
				qsys->uuconf_zpubdir, TRUE,
				FALSE, (const char *) NULL))
	{
	  ulog (LOG_ERROR, "%s: not permitted to receive", zfile);
	  ubuffree (zfile);
	  return fremote_send_fail (FAILURE_PERM);
	}

      if (strchr (qcmd->zoptions, 'f') == NULL)
	{
	  if (! fsysdep_make_dirs (zfile, TRUE))
	    {
	      ubuffree (zfile);
	      return fremote_send_fail (FAILURE_OPEN);
	    }
	}
    }

  ztemp = zsysdep_receive_temp (qsys, zfile, &cbytes);

  if (qcmd->cbytes != -1)
    {
      /* Adjust the number of bytes we are prepared to receive
	 according to the amount of free space we are supposed to
	 leave available and the maximum file size we are permitted to
	 transfer.  */
      if (cbytes != -1)
	{
	  cbytes -= qsys->uuconf_cfree_space;
	  if (cbytes < 0)
	    cbytes = 0;
	}
	      
      if (qdaemon->cremote_size != -1
	  && (cbytes == -1 || qdaemon->cremote_size < cbytes))
	cbytes = qdaemon->cremote_size;

      /* If the number of bytes we are prepared to receive is less
	 than the file size, we must fail.  */
      if (cbytes != -1 && cbytes < qcmd->cbytes)
	{
	  ulog (LOG_ERROR, "%s: too big to receive", zfile);
	  ubuffree (ztemp);
	  ubuffree (zfile);
	  return fremote_send_fail (FAILURE_SIZE);
	}
    }

  e = esysdep_open_receive (qsys, zfile, ztemp);
  if (! ffileisopen (e))
    {
      ubuffree (ztemp);
      ubuffree (zfile);
      return fremote_send_fail (FAILURE_OPEN);
    }

  qinfo = (struct srecinfo *) xmalloc (sizeof (struct srecinfo));
  if (strchr (qcmd->zoptions, 'n') == NULL)
    qinfo->zmail = NULL;
  else
    qinfo->zmail = zbufcpy (qcmd->znotify);
  qinfo->zfile = zfile;
  qinfo->fspool = fspool;
  qinfo->flocal = FALSE;

  qtrans = qtransalc (qcmd);
  qtrans->psendfn = fremote_send_reply;
  qtrans->pinfo = (pointer) qinfo;
  qtrans->e = e;
  qtrans->s.ztemp = ztemp;

  uqueue_remote (qtrans);

  return TRUE;
}

/* Reply to a send request, and prepare to receive the file.  */

static boolean
fremote_send_reply (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  struct srecinfo *qinfo = (struct srecinfo *) qtrans->pinfo;

  if (! (*qdaemon->qproto->pfsendcmd) (qdaemon, "SY"))
    {
      (void) ffileclose (qtrans->e);
      (void) remove (qtrans->s.ztemp);
      ubuffree (qinfo->zmail);
      ubuffree (qinfo->zfile);
      xfree (qtrans->pinfo);
      utransfree (qtrans);
      return FALSE;
    }

  ulog (LOG_NORMAL, "Receiving %s",
	qinfo->fspool ? qtrans->s.zto : qinfo->zfile);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, TRUE, FALSE,
					(long) -1, &fhandled))
	{
	  (void) ffileclose (qtrans->e);
	  (void) remove (qtrans->s.ztemp);
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}
      if (fhandled)
	return TRUE;
    }

  qtrans->frecfile = TRUE;
  qtrans->precfn = frec_file_end;

  uqueue_receive (qtrans);

  return TRUE;
}

/* If we can't receive a file, queue up a response to the remote
   system.  */

static boolean
fremote_send_fail (twhy)
     enum tfailure twhy;
{
  enum tfailure *ptinfo;
  struct stransfer *qtrans;

  ptinfo = (enum tfailure *) xmalloc (sizeof (enum tfailure));
  *ptinfo = twhy;

  qtrans = qtransalc ((struct scmd *) NULL);
  qtrans->psendfn = fremote_send_fail_send;
  qtrans->pinfo = (pointer) ptinfo;

  uqueue_remote (qtrans);

  return TRUE;
}

/* Send a failure string for a send command to the remote system;
   this is called when we are ready to reply to the command.  */

static boolean
fremote_send_fail_send (qtrans, qdaemon)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
{
  enum tfailure *ptinfo = (enum tfailure *) qtrans->pinfo;
  const char *z;
  boolean fret;

  switch (*ptinfo)
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
  
  fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, z);
  xfree (qtrans->pinfo);
  utransfree (qtrans);
  return fret;
}

/* This is called when a file has been completely received.  It sends
   a response to the remote system.  */

/*ARGSUSED*/
static boolean
frec_file_end (qtrans, qdaemon, zdata, cdata)
     struct stransfer *qtrans;
     struct sdaemon *qdaemon;
     const char *zdata;
     size_t cdata;
{
  struct srecinfo *qinfo = (struct srecinfo *) qtrans->pinfo;
  const char *zerr;
  boolean fnever;
  boolean fret;

  DEBUG_MESSAGE2 (DEBUG_UUCP_PROTO, "frec_file_end: %s to %s",
		  qtrans->s.zfrom, qtrans->s.zto);

  if (qdaemon->qproto->pffile != NULL)
    {
      boolean fhandled;

      if (! (*qdaemon->qproto->pffile) (qdaemon, qtrans, FALSE, FALSE,
					(long) -1, &fhandled))
	{
	  (void) ffileclose (qtrans->e);
	  (void) remove (qtrans->s.ztemp);
	  ubuffree (qinfo->zmail);
	  ubuffree (qinfo->zfile);
	  xfree (qtrans->pinfo);
	  utransfree (qtrans);
	  return FALSE;
	}
      if (fhandled)
	return TRUE;
    }

  qtrans->precfn = NULL;

  fnever = FALSE;

  if (! ffileclose (qtrans->e))
    {
      zerr = strerror (errno);
      ulog (LOG_ERROR, "%s: close: %s", qtrans->s.zto, zerr);
    }
  else if (! fsysdep_move_file (qtrans->s.ztemp, qinfo->zfile, qinfo->fspool,
				FALSE, ! qinfo->fspool,
				(qinfo->flocal
				 ? qtrans->s.zuser
				 : (const char *) NULL)))
    {
      zerr = "could not move to final location";
      ulog (LOG_ERROR, "%s: %s", qinfo->zfile, zerr);
      fnever = TRUE;
    }
  else
    {
      if (! qinfo->fspool)
	{
	  unsigned int imode;

	  /* Unless we can change the ownership of the file, the only
	     choice to make about these bits is whether to set the
	     execute bit or not.  */
	  if ((qtrans->s.imode & 0111) != 0)
	    imode = 0777;
	  else
	    imode = 0666;
	  (void) fsysdep_change_mode (qinfo->zfile, imode);
	}
  
      zerr = NULL;
    }

  /* Send the completion string to the remote system.  */
  if (zerr == NULL)
    fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, "CY");
  else
    fret = (*qdaemon->qproto->pfsendcmd) (qdaemon, "CN5");

  if (zerr != NULL)
    (void) remove (qtrans->s.ztemp);

  ustats (zerr == NULL, qtrans->s.zuser, qdaemon->qsys->uuconf_zname,
	  FALSE, qtrans->cbytes, qtrans->isecs, qtrans->imicros);

  if (zerr == NULL)
    {
      if (qinfo->zmail != NULL && *qinfo->zmail != '\0')
	(void) fmail_transfer (TRUE, qtrans->s.zuser, qinfo->zmail,
			       (const char *) NULL,
			       qtrans->s.zfrom, qdaemon->qsys->uuconf_zname,
			       qtrans->s.zto, (const char *) NULL,
			       (const char *) NULL);

      if (qtrans->s.pseq != NULL)
	(void) fsysdep_did_work (qtrans->s.pseq);
    }
  else
    {
      /* If the transfer failed, we send mail if it was requested
	 locally and if it can never succeed.  */
      if (qinfo->flocal && fnever)
	{
	  (void) fmail_transfer (FALSE, qtrans->s.zuser, qinfo->zmail,
				 zerr, qtrans->s.zfrom,
				 qdaemon->qsys->uuconf_zname,
				 qtrans->s.zto, (const char *) NULL,
				 (const char *) NULL);
	  (void) fsysdep_did_work (qtrans->s.pseq);
	}
    }

  ubuffree (qinfo->zmail);
  ubuffree (qinfo->zfile);
  xfree (qtrans->pinfo);
  utransfree (qtrans);

  return fret;
}
