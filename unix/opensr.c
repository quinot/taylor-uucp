/* opensr.c
   Open files for sending and receiving.

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

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "sysdep.h"

#include <errno.h>

#if HAVE_TIME_H
#include <time.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

/* Get the right header files for statfs and friends.  This stuff is
   from David MacKenzie's df program.  */

#if FS_STATVFS
#include <sys/statvfs.h>
#endif

#if FS_USG_STATFS
#include <sys/statfs.h>
#endif

#if FS_MNTENT
#include <sys/vfs.h>
#endif

#if FS_GETMNT
#include <sys/param.h>
#include <sys/mount.h>
#endif

#if FS_STATFS
#include <sys/mount.h>
#endif

#if FS_USTAT
#include <ustat.h>
#endif

#ifndef time
extern time_t time ();
#endif

/* Open a file to send to another system, and return the mode and
   the size.  */

openfile_t
esysdep_open_send (qsys, zfile, fcheck, zuser)
     const struct uuconf_system *qsys;
     const char *zfile;
     boolean fcheck;
     const char *zuser;
{
  struct stat s;
  openfile_t e;
  int o;
  
  if (fsysdep_directory (zfile))
    {
      ulog (LOG_ERROR, "%s: is a directory", zfile);
      return EFILECLOSED;
    }

#if USE_STDIO
  e = fopen (zfile, BINREAD);
  if (e == NULL)
    {
      ulog (LOG_ERROR, "fopen (%s): %s", zfile, strerror (errno));
      return NULL;
    }
  o = fileno (e);
#else
  e = open ((char *) zfile, O_RDONLY | O_NOCTTY, 0);
  if (e == -1)
    {
      ulog (LOG_ERROR, "open (%s): %s", zfile, strerror (errno));
      return -1;
    }
  o = e;
#endif

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) ffileclose (e);
      return EFILECLOSED;
    }

  if (fstat (o, &s) == -1)
    {
      ulog (LOG_ERROR, "fstat: %s", strerror (errno));
      s.st_mode = 0666;
    }

  /* We have to recheck the file permission, although we probably
     checked it already, because otherwise there would be a window in
     which somebody could change the contents of a symbolic link to
     point to some file which was only readable by uucp.  */
  if (fcheck)
    {
      if (! fsuser_access (&s, R_OK, zuser))
	{
	  ulog (LOG_ERROR, "%s: %s", zfile, strerror (EACCES));
	  (void) ffileclose (e);
	  return EFILECLOSED;
	}
    }

  return e;
}

/* Get a temporary file name to receive into.  This is supposed to set
   *pcbytes to the size of the largest file that can be accepted.  We
   use the ztemp argument to pick the file name, so that we relocate
   the file if the transmission is aborted.  */

char *
zsysdep_receive_temp (qsys, zto, ztemp, pcbytes)
     const struct uuconf_system *qsys;
     const char *zto;
     const char *ztemp;
     long *pcbytes;
{
  char *zret;
  long c1, c2;
  char *z1, *z2, *zslash;

  if (ztemp != NULL
      && *ztemp == 'D'
      && strcmp (ztemp, "D.0") != 0)
    zret = zsappend3 (".Temp", qsys->uuconf_zname, ztemp);
  else
    zret = zstemp_file (qsys);
  if (zret == NULL)
    return NULL;

  /* Try to determine the amount of free space available for the
     temporary file and for the final destination.  This code is
     mostly from David MacKenzie's df program.  */
  c1 = (long) -1;
  c2 = (long) -1;

  z1 = zbufcpy (zret);
  zslash = strrchr (z1, '/');
  if (zslash != NULL)
    *zslash = '\0';
  else
    {
      z1[0] = '.';
      z1[1] = '\0';
    }

  z2 = zbufcpy (zto);
  zslash = strrchr (z2, '/');
  if (zslash != NULL)
    *zslash = '\0';
  else
    {
      z2[0] = '.';
      z2[1] = '\0';
    }

  {
#if FS_STATVFS
    struct statvfs s;

    if (statvfs (z1, &s) >= 0)
      c1 = (long) s.f_bavail * (long) s.f_frsize;
    if (statvfs (z2, &s) >= 0)
      c2 = (long) s.f_bavail * (long) s.f_frsize;
#endif
#if FS_USG_STATFS
    struct statfs s;

    /* This structure has an f_bsize field, but on many systems
       f_bfree is measured in 512 byte blocks.  On some systems,
       f_bfree is measured in f_bsize byte blocks.  Rather than
       overestimate the amount of free space, this code assumes that
       f_bfree is measuring 512 byte blocks.  */
    if (statfs (z1, &s, sizeof s, 0) >= 0)
      c1 = (long) s.f_bfree * (long) 512;
    if (statfs (z2, &s, sizeof s, 0) >= 0)
      c2 = (long) s.f_bfree * (long) 512;
#endif
#if FS_MNTENT
    struct statfs s;

    if (statfs (z1, &s) == 0)
      c1 = (long) s.f_bavail * (long) s.f_bsize;
    if (statfs (z2, &s) == 0)
      c2 = (long) s.f_bavail * (long) s.f_bsize;
#endif
#if FS_GETMNT
    struct fs_data s;

    if (statfs (z1, &s) == 1)
      c1 = (long) s.fd_req.bfreen * (long) 1024;
    if (statfs (z2, &s) == 1)
      c2 = (long) s.fd_req.bfreen * (long) 1024;
#endif
#if FS_STATFS
    struct statfs s;

    if (statfs (z1, &s) >= 0)
      c1 = (long) s.f_bavail * (long) s.f_fsize;
    if (statfs (z2, &s) >= 0)
      c2 = (long) s.f_bavail * (long) s.f_fsize;
#endif
#if FS_USTAT
    struct stat sstat;
    struct ustat s;

    if (stat (z1, &sstat) == 0
	&& ustat (sstat.st_dev, &s) == 0)
      c1 = (long) s.f_tfree * (long) 512;
    if (stat (z2, &sstat) == 0
	&& ustat (sstat.st_dev, &s) == 0)
      c2 = (long) s.f_tfree * (long) 512;
#endif
  }

  ubuffree (z1);
  ubuffree (z2);

  if (c1 == (long) -1)
    *pcbytes = c2;
  else if (c2 == (long) -1)
    *pcbytes = c1;
  else if (c1 < c2)
    *pcbytes = c1;
  else
    *pcbytes = c2;

  return zret;
}  

/* Open a temporary file to receive into.  This should, perhaps, check
   that we have write permission on the receiving directory, but it
   doesn't.  */

openfile_t
esysdep_open_receive (qsys, zto, ztemp, zreceive, pcrestart)
     const struct uuconf_system *qsys;
     const char *zto;
     const char *ztemp;
     const char *zreceive;
     long *pcrestart;
{
  int o;
  openfile_t e;

  /* If we used the ztemp argument in zsysdep_receive_temp, above,
     then we will have a name consistent across conversations.  In
     that case, we may have already received some portion of this
     file.  */
  o = -1;
  *pcrestart = -1;
  if (ztemp != NULL
      && *ztemp == 'D'
      && strcmp (ztemp, "D.0") != 0)
    {
      o = open ((char *) zreceive, O_WRONLY);
      if (o >= 0)
	{
	  struct stat s;

	  /* For safety, we insist on the file being less than 1 week
	     old.  This can still catch people, unfortunately.  I
	     don't know of any good solution to the problem of old
	     files hanging around.  If anybody has a file they want
	     restarted, and they know about this issue, they can touch
	     it to bring it up to date.  */
	  if (fstat (o, &s) < 0
	      || s.st_mtime + 7 * 24 * 60 * 60 < time ((time_t *) NULL))
	    {
	      (void) close (o);
	      o = -1;
	    }
	  else
	    {
	      DEBUG_MESSAGE1 (DEBUG_SPOOLDIR,
			      "esysdep_open_receive: Reusing %s",
			      zreceive);
	      *pcrestart = (long) s.st_size;
	    }
	}
    }

  if (o < 0)
    o = creat ((char *) zreceive, IPRIVATE_FILE_MODE);

  if (o < 0)
    {
      if (errno == ENOENT)
	{
	  if (! fsysdep_make_dirs (zreceive, FALSE))
	    return EFILECLOSED;
	  o = creat ((char *) zreceive, IPRIVATE_FILE_MODE);
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "creat (%s): %s", zreceive, strerror (errno));
	  return EFILECLOSED;
	}
    }

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) close (o);
      (void) remove (zreceive);
      return EFILECLOSED;
    }

#if USE_STDIO
  e = fdopen (o, (char *) BINWRITE);

  if (e == NULL)
    {
      ulog (LOG_ERROR, "fdopen (%s): %s", zreceive, strerror (errno));
      (void) close (o);
      (void) remove (zreceive);
      return EFILECLOSED;
    }
#else
  e = o;
#endif

  return e;
}
