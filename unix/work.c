/* work.c
   Routines to read command files.

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
const char work_rcsid[] = "$Id$";
#endif

#include "system.h"
#include "sysdep.h"

#include <ctype.h>
#include <errno.h>

#if HAVE_OPENDIR
#if HAVE_DIRENT_H
#include <dirent.h>
#else /* ! HAVE_DIRENT_H */
#include <sys/dir.h>
#define dirent direct
#endif /* ! HAVE_DIRENT_H */
#endif /* HAVE_OPENDIR */

/* Local functions.  */

static char *zswork_directory P((const char *zsystem));
static boolean fswork_file P((const char *zsystem, const char *zfile,
			      char *pbgrade));
static int iswork_cmp P((constpointer pkey, constpointer pdatum));

/* These functions can support multiple actions going on at once.
   This allows the UUCP package to send and receive multiple files at
   the same time.  This is a very flexible feature, but I'm not sure
   it will actually be used all that much.

   The ssfile structure holds a command file name and all the lines
   read in from that command file.  The union within the ssline
   structure initially holds a line from the file and then holds a
   pointer back to the ssfile structure; a pointer to this union is
   used as a sequence pointer.  The ztemp entry of the ssline
   structure holds the name of a temporary file to delete, if any.  */

#define CFILELINES (10)

struct ssline
{
  char *zline;
  struct ssfile *qfile;
  char *ztemp;
};

struct ssfile
{
  char *zfile;
  int clines;
  int cdid;
  struct ssline aslines[CFILELINES];
};

/* Static variables for the work scan.  */

static char **azSwork_files;
static int cSwork_files;
static int iSwork_file;
static struct ssfile *qSwork_file;

/* Given a system name, return a directory to search for work.  */

static char *
zswork_directory (zsystem)
     const char *zsystem;
{
#if SPOOLDIR_V2
  return zbufcpy (".");
#endif /* SPOOLDIR_V2 */
#if SPOOLDIR_BSD42 | SPOOLDIR_BSD43
  return zbufcpy ("C.");
#endif /* SPOOLDIR_BSD42 | SPOOLDIR_BSD43 */
#if SPOOLDIR_HDB
  return zbufcpy (zsystem);
#endif /* SPOOLDIR_HDB */
#if SPOOLDIR_ULTRIX
  return zsappend3 ("sys",
		    (fsultrix_has_spool (zsystem)
		     ? zsystem
		     : "DEFAULT"),
		    "C.");
#endif /* SPOOLDIR_ULTRIX */
#if SPOOLDIR_TAYLOR
  return zsysdep_in_dir (zsystem, "C.");
#endif /* SPOOLDIR_TAYLOR */
}

/* See whether a file name from the directory returned by
   zswork_directory is really a command for a particular system.
   Return the command grade.  */

/*ARGSUSED*/
static boolean
fswork_file (zsystem, zfile, pbgrade)
     const char *zsystem;
     const char *zfile;
     char *pbgrade;
{
#if SPOOLDIR_V2 || SPOOLDIR_BSD42 || SPOOLDIR_BSD43 || SPOOLDIR_ULTRIX
  int cfilesys, csys;

  /* The file name should be C.ssssssgqqqq, where g is exactly one
     letter and qqqq is exactly four numbers.  The system name may be
     truncated to six or seven characters.  The system name of the
     file must match the system name we're looking for, since there
     could be work files for several systems in one directory.  */
  if (zfile[0] != 'C' || zfile[1] != '.')
    return FALSE;
  csys = strlen (zsystem);
  cfilesys = strlen (zfile) - 7;
  if (csys != cfilesys
      && (csys < 6 || (cfilesys != 6 && cfilesys != 7)))
    return FALSE;
  *pbgrade = zfile[cfilesys + 2];
  return strncmp (zfile + 2, zsystem, cfilesys) == 0;
#endif /* V2 || BSD42 || BSD43 || ULTRIX */
#if SPOOLDIR_HDB
  int clen;

  /* The file name should be C.ssssssgqqqq where g is exactly one
     letter and qqqq is exactly four numbers or letters.  We don't
     check the system name, because it is guaranteed by the directory
     we are looking in and AIX uucp sets it to the local system rather
     than the remote one.  */
  if (zfile[0] != 'C' || zfile[1] != '.')
    return FALSE;
  clen = strlen (zfile);
  if (clen < 7)
    return FALSE;
  *pbgrade = zfile[clen - 5];
  return TRUE;
#endif /* SPOOLDIR_HDB */
#if SPOOLDIR_TAYLOR
  /* We don't keep the system name in the file name, since that
     forces truncation.  Our file names are always C.gqqqq.  */
  *pbgrade = zfile[2];
  return (zfile[0] == 'C'
	  && zfile[1] == '.'
	  && strlen (zfile) == 7);
#endif /* SPOOLDIR_TAYLOR */
}

/* A comparison function to look through the list of file names.  */

static int
iswork_cmp (pkey, pdatum)
     constpointer pkey;
     constpointer pdatum;
{
  const char * const *pzkey = (const char * const *) pkey;
  const char * const *pzdatum = (const char * const *) pdatum;

  return strcmp (*pzkey, *pzdatum);
}

/* See whether there is any work to do for a particular system.  */

boolean
fsysdep_has_work (qsys)
     const struct uuconf_system *qsys;
{
  char *zdir;
  DIR *qdir;
  struct dirent *qentry;

  zdir = zswork_directory (qsys->uuconf_zname);
  if (zdir == NULL)
    return FALSE;
  qdir = opendir ((char *) zdir);
  ubuffree (zdir);
  if (qdir == NULL)
    return FALSE;

  while ((qentry = readdir (qdir)) != NULL)
    {
      char bgrade;

      if (fswork_file (qsys->uuconf_zname, qentry->d_name, &bgrade))
	{
	  closedir (qdir);
	  return TRUE;
	}
    }

  closedir (qdir);
  return FALSE;
}

/* Initialize the work scan.  We have to read all the files in the
   work directory, so that we can sort them by work grade.  The bgrade
   argument is the minimum grade to consider.  We don't want to return
   files that we have already considered; usysdep_get_work_free will
   clear the data out when we are done with the system.  This returns
   FALSE on error.  */

#define CWORKFILES (10)

boolean
fsysdep_get_work_init (qsys, bgrade, fcheck)
     const struct uuconf_system *qsys;
     int bgrade;
     boolean fcheck;
{
  char *zdir;
  DIR *qdir;
  struct dirent *qentry;
  int chad;
  int callocated;

  zdir = zswork_directory (qsys->uuconf_zname);
  if (zdir == NULL)
    return FALSE;

  qdir = opendir ((char *) zdir);
  if (qdir == NULL)
    {
      if (errno != ENOENT)
	ulog (LOG_ERROR, "opendir (%s): %s", zdir, strerror (errno));
      ubuffree (zdir);
      return FALSE;
    }

  ubuffree (zdir);

  chad = cSwork_files;
  callocated = cSwork_files;

  /* Sort the files we already know about so that we can check the new
     ones with bsearch.  It would be faster to use a hash table, and
     the code should be probably be changed.  The sort done at the end
     of this function does not suffice because it only includes the
     files added last time, and does not sort the entire array.  Some
     (bad) qsort implementations are very slow when given a sorted
     array, which causes particularly bad effects here.  */
  if (chad > 0)
    qsort ((pointer) azSwork_files, chad, sizeof (char *), iswork_cmp);

  while ((qentry = readdir (qdir)) != NULL)
    {
      char bfilegrade;
      char *zname;

      zname = qentry->d_name;
      if (fswork_file (qsys->uuconf_zname, zname, &bfilegrade)
	  && (azSwork_files == NULL
	      || bsearch ((pointer) &zname,
			  (pointer) azSwork_files,
			  chad, sizeof (char *),
			  iswork_cmp) == NULL))
	{
	  if (UUCONF_GRADE_CMP (bgrade, bfilegrade) < 0)
	    continue;
	  
	  DEBUG_MESSAGE1 (DEBUG_SPOOLDIR,
			  "fsysdep_get_work_init: Found %s",
			  qentry->d_name);

	  if (cSwork_files >= callocated)
	    {
	      callocated += CWORKFILES;
	      azSwork_files =
		(char **) xrealloc ((pointer) azSwork_files,
				    callocated * sizeof (char *));
	    }

	  azSwork_files[cSwork_files] = zbufcpy (qentry->d_name);
	  ++cSwork_files;
	}
    }

  closedir (qdir);

  /* Sorting the files alphabetically will get the grades in the
     right order, since all the file prefixes are the same.  */

  if (cSwork_files > chad)
    qsort ((pointer) (azSwork_files + chad), cSwork_files - chad,
	   sizeof (char *), iswork_cmp);

  return TRUE;
}

/* Get the next work entry for a system.  This must parse the next
   line in the next work file.  The type of command is set into
   qcmd->bcmd; if there are no more commands we call
   fsysdep_get_work_init to rescan, in case any came in since the last
   call.  If there are still no commands, qcmd->bcmd is set to 'H'.
   Each field in the structure is set to point to a spot in an
   malloced string.  The only time we use the grade here is when
   calling fsysdep_get_work_init to rescan.  */

boolean
fsysdep_get_work (qsys, bgrade, fcheck, qcmd)
     const struct uuconf_system *qsys;
     int bgrade;
     boolean fcheck;
     struct scmd *qcmd;
{
  char *zdir;

  if (qSwork_file != NULL && qSwork_file->cdid >= qSwork_file->clines)
    qSwork_file = NULL;

  if (azSwork_files == NULL)
    {
      qcmd->bcmd = 'H';
      return TRUE;
    }

  zdir = NULL;

  /* This loop continues until a line is returned.  */
  while (TRUE)
    {
      /* This loop continues until a file is opened and read in.  */
      while (qSwork_file == NULL)
	{
	  FILE *e;
	  struct ssfile *qfile;
	  int iline, callocated;
	  char *zline;
	  size_t cline;
	  char *zname;

	  /* Read all the lines of a command file into memory.  */

	  do
	    {
	      if (iSwork_file >= cSwork_files)
		{
		  /* Rescan the work directory.  */
		  if (! fsysdep_get_work_init (qsys, bgrade, fcheck))
		    {
		      ubuffree (zdir);
		      return FALSE;
		    }
		  if (iSwork_file >= cSwork_files)
		    {
		      qcmd->bcmd = 'H';
		      ubuffree (zdir);
		      return TRUE;
		    }
		}

	      if (zdir == NULL)
		{
		  zdir = zswork_directory (qsys->uuconf_zname);
		  if (zdir == NULL)
		    return FALSE;
		}

	      zname = zsysdep_in_dir (zdir, azSwork_files[iSwork_file]);

	      ++iSwork_file;

	      e = fopen (zname, "r");
	      if (e == NULL)
		{
		  ulog (LOG_ERROR, "fopen (%s): %s", zname,
			strerror (errno));
		  ubuffree (zname);
		}
	    }
	  while (e == NULL);
	  
	  qfile = (struct ssfile *) xmalloc (sizeof (struct ssfile));
	  callocated = CFILELINES;
	  iline = 0;

	  zline = NULL;
	  cline = 0;
	  while (getline (&zline, &cline, e) > 0)
	    {
	      if (iline >= callocated)
		{
		  /* The sizeof (struct ssfile) includes CFILELINES
		     entries already, so using callocated * sizeof
		     (struct ssline) will give us callocated *
		     CFILELINES entries.  */
		  qfile =
		    ((struct ssfile *)
		     xrealloc ((pointer) qfile,
			       (sizeof (struct ssfile) +
				(callocated * sizeof (struct ssline)))));
		  callocated += CFILELINES;
		}
	      qfile->aslines[iline].zline = zbufcpy (zline);
	      qfile->aslines[iline].qfile = NULL;
	      qfile->aslines[iline].ztemp = NULL;
	      iline++;
	    }

	  xfree ((pointer) zline);

	  if (fclose (e) != 0)
	    ulog (LOG_ERROR, "fclose: %s", strerror (errno));

	  if (iline == 0)
	    {
	      /* There was nothing in the file; remove it and look
		 for the next one.  */
	      xfree ((pointer) qfile);
	      if (! fcheck)
		{
		  if (remove (zname) != 0)
		    ulog (LOG_ERROR, "remove (%s): %s", zname,
			  strerror (errno));
		}
	      ubuffree (zname);
	    }
	  else
	    {
	      qfile->zfile = zname;
	      qfile->clines = iline;
	      qfile->cdid = 0;
	      qSwork_file = qfile;
	    }
	}

      /* This loop continues until all the lines from the current file
	 are used up, or a line is returned.  */
      while (TRUE)
	{
	  int iline;
	  
	  if (qSwork_file->cdid >= qSwork_file->clines)
	    {
	      /* We don't want to free qSwork_file here, since it must
		 remain until all the lines have been completed.  It
		 is freed in fsysdep_did_work.  */
	      qSwork_file = NULL;
	      /* Go back to the main loop which finds another file.  */
	      break;
	    }

	  iline = qSwork_file->cdid;
	  ++qSwork_file->cdid;

	  /* Now parse the line into a command.  */
	  if (! fparse_cmd (qSwork_file->aslines[iline].zline, qcmd))
	    {
	      ulog (LOG_ERROR, "Bad line in command file %s",
		    qSwork_file->zfile);
	      ubuffree (qSwork_file->aslines[iline].zline);
	      qSwork_file->aslines[iline].zline = NULL;
	      continue;
	    }

	  if (qcmd->bcmd == 'S' || qcmd->bcmd == 'E')
	    {
	      char *zreal;

	      zreal = zsysdep_spool_file_name (qsys, qcmd->ztemp);
	      if (zreal == NULL)
		{
		  ubuffree (qSwork_file->aslines[iline].zline);
		  qSwork_file->aslines[iline].zline = NULL;
		  ubuffree (zdir);
		  return FALSE;
		}
	      qSwork_file->aslines[iline].ztemp = zreal;
	    }

	  qSwork_file->aslines[iline].qfile = qSwork_file;
	  qcmd->pseq = (pointer) (&qSwork_file->aslines[iline]);

	  ubuffree (zdir);
	  return TRUE;
	}
    }
}

/* When a command has been complete, fsysdep_did_work is called.  The
   sequence entry was set above to be the address of an aslines
   structure whose pfile entry points to the ssfile corresponding to
   this file.  We can then check whether all the lines have been
   completed (they will have been if the pfile entry is NULL) and
   remove the file if they have been.  This means that we only remove
   a command file if we manage to complete every transfer it specifies
   in a single UUCP session.  I don't know if this is how regular UUCP
   works.  */

boolean
fsysdep_did_work (pseq)
     pointer pseq;
{
  struct ssfile *qfile;
  struct ssline *qline;
  int i;
  
  qline = (struct ssline *) pseq;

  ubuffree (qline->zline);
  qline->zline = NULL;

  qfile = qline->qfile;
  qline->qfile = NULL;

  /* Remove the temporary file, if there is one.  It really doesn't
     matter if this fails, and not checking the return value lets us
     attempt to remove D.0 or whatever an unused temporary file is
     called without complaining.  */
  if (qline->ztemp != NULL)
    {
      (void) remove (qline->ztemp);
      ubuffree (qline->ztemp);
      qline->ztemp = NULL;
    }

  /* If not all the lines have been returned from fsysdep_get_work,
     we can't remove the file yet.  */
  if (qfile->cdid < qfile->clines)
    return TRUE;

  /* See whether all the commands have been completed.  */
  for (i = 0; i < qfile->clines; i++)
    if (qfile->aslines[i].qfile != NULL)
      return TRUE;

  /* All commands have finished.  */
  if (remove (qfile->zfile) != 0)
    {
      ulog (LOG_ERROR, "remove (%s): %s", qfile->zfile,
	    strerror (errno));
      return FALSE;
    }

  ubuffree (qfile->zfile);
  xfree ((pointer) qfile);

  if (qfile == qSwork_file)
    qSwork_file = NULL;

  return TRUE;
}

/* Free up the results of a work scan, when we're done with this
   system.  */

/*ARGSUSED*/
void
usysdep_get_work_free (qsys)
     const struct uuconf_system *qsys;
{
  if (azSwork_files != NULL)
    {
      int i;

      for (i = 0; i < cSwork_files; i++)
	ubuffree ((pointer) azSwork_files[i]);
      xfree ((pointer) azSwork_files);
      azSwork_files = NULL;
      cSwork_files = 0;
      iSwork_file = 0;
    }
  if (qSwork_file != NULL)
    {
      int i;

      ubuffree (qSwork_file->zfile);
      for (i = 0; i < qSwork_file->cdid; i++)
	{
	  ubuffree (qSwork_file->aslines[i].zline);
	  ubuffree (qSwork_file->aslines[i].ztemp);
	}
      for (i = qSwork_file->cdid; i < qSwork_file->clines; i++)
	ubuffree (qSwork_file->aslines[i].zline);
      xfree ((pointer) qSwork_file);
      qSwork_file = NULL;
    }
}

/* Save the temporary file used by a send command, and return an
   informative message to mail to the requestor.  This is called when
   a file transfer failed, to make sure that the potentially valuable
   file is not completely lost.  */

const char *
zsysdep_save_temp_file (pseq)
     pointer pseq;
{
  struct ssline *qline = (struct ssline *) pseq;
  char *zto, *zslash;
  int cwant;
  static char *zbuf;
  static int cbuf;

  if (! fsysdep_file_exists (qline->ztemp))
    return NULL;

  zslash = strrchr (qline->ztemp, '/');
  if (zslash == NULL)
    zslash = qline->ztemp;
  else
    ++zslash;

  zto = (char *) alloca (sizeof PRESERVEDIR + 1 + strlen (zslash));
  sprintf (zto, "%s/%s", PRESERVEDIR, zslash);

  if (! fsysdep_move_file (qline->ztemp, zto, TRUE, FALSE, FALSE,
			   (const char *) NULL))
    return "Could not move file to preservation directory";
    
  cwant = sizeof "File saved as\n\t" + strlen (zSspooldir) + 1 + strlen (zto);
  if (cwant > cbuf)
    {
      zbuf = (char *) xrealloc ((pointer) zbuf, cwant);
      cbuf = cwant;
    }

  sprintf (zbuf, "File saved as\n\t%s/%s", zSspooldir, zto);
  return zbuf;
}

/* Get the jobid of a work file.  This is needed by uustat.  */

char *
zsysdep_jobid (qsys, pseq)
     const struct uuconf_system *qsys;
     pointer pseq;
{
  return zsfile_to_jobid (qsys, ((struct ssline *) pseq)->qfile->zfile);
}
