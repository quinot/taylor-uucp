/* chat.c
   Chat routine for the UUCP package.

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
   Revision 1.1  1991/09/10  19:38:16  ian
   Initial revision

   */

#include "uucp.h"

#if USE_RCS_ID
char chat_rcsid[] = "$Id$";
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "port.h"
#include "system.h"

static int ccescape P((char *zbuf));
static int icexpect P((int cstrings, char **azstrings, int *aclens,
		       int ctimeout));
static boolean fcecho_send P((const char *z, int clen));
static boolean fcphone P((const struct sdialer *qdial, const char *zphone,
			  boolean (*pfwrite) P((const char *zwrite,
						int cwrite)),
			  boolean ftranslate));

/* Run a chat script with the other system.  The chat script is a
   series of expect send pairs.  We wait for the expect string to show
   up, and then we send the send string.  The chat string for a system
   holds the expect and send strings separated by a single space.  */

boolean
fchat (zchat, zchat_fail, ctimeout, qsys, qdial, zphone, ftranslate)
     const char *zchat;
     const char *zchat_fail;
     int ctimeout;
     const struct ssysteminfo *qsys;
     const struct sdialer *qdial;
     const char *zphone;
     boolean ftranslate;
{
  int cstrings;
  char **azstrings;
  int *aclens;
  char *zbuf;

  if (zchat_fail == NULL)
    {
      cstrings = 1;
      azstrings = (char **) alloca (sizeof (char *));
      aclens = (int *) alloca (sizeof (int));
    }
  else
    {
      const char *zlook;
      char *zcopy, *z;

      /* We leave string number 0 for the chat script; after that
	 we want 1 more than the number of spaces in the chat_fail
	 string (the fencepost problem).  */
      cstrings = 2;
      for (zlook = zchat_fail; *zlook != '\0'; zlook++)
	if (*zlook == ' ')
	  ++cstrings;

      azstrings = (char **) alloca (cstrings * sizeof (char *));
      aclens = (int *) alloca (cstrings * sizeof (int));

      zcopy = (char *) alloca (strlen (zchat_fail) + 1);
      strcpy (zcopy, zchat_fail);

      /* Get the strings into the array, and handle all the escape
	 characters.  */
      cstrings = 1;
      azstrings[1] = zcopy;
      for (z = zcopy; *z != '\0'; z++)
	{
	  if (*z == ' ')
	    {
	      *z++ = '\0';
	      aclens[cstrings] = ccescape (azstrings[cstrings]);
	      ++cstrings;
	      azstrings[cstrings] = z;
	    }
	}
      aclens[cstrings] = ccescape (azstrings[cstrings]);
      ++cstrings;
    }

  zbuf = (char *) alloca (strlen (zchat));

  while (*zchat != '\0')
    {
      int cchatlen;

      cchatlen = strcspn (zchat, " ");
      strncpy (zbuf, zchat, cchatlen);
      zbuf[cchatlen] = '\0';
      zchat += cchatlen;
      if (*zchat != '\0')
	++zchat;

      if (strcmp (zbuf, "\"\"") != 0)
	{
	  int istr;
	  char *znext;

	  azstrings[0] = zbuf;
	  znext = strchr (zbuf, '-');
	  if (znext != NULL)
	    *znext = '\0';
	  aclens[0] = ccescape (azstrings[0]);
	  while ((istr = icexpect (cstrings, azstrings, aclens, ctimeout))
		 != 0)
	    {
	      char *zsub;

	      /* If we got an error, return FALSE.  */
	      if (istr < -1)
		return FALSE;

	      /* If we found a failure string, log it and get out.  */
	      if (istr > 0)
		{
		  int clen;
		  char *zcopy;

		  for (--istr; istr > 0; --istr)
		    zchat_fail = strchr (zchat_fail, ' ') + 1;
		  clen = strcspn (zchat_fail, " ");
		  zcopy = (char *) alloca (clen + 1);
		  strncpy (zcopy, zchat_fail, clen);
		  zcopy[clen] = '\0';
		  ulog (LOG_ERROR, "Chat script failed: Got \"%s\"",
			zcopy);
		  return FALSE;
		}

	      /* We timed out; look for a send subsequence.  If none,
		 the chat script has failed.  */
	      if (znext == NULL)
		{
		  ulog (LOG_ERROR, "Timed out in chat script");
		  return FALSE;
		}

	      ++znext;
	      zsub = znext;
	      znext = strchr (zsub, '-');
	      if (znext != NULL)
		*znext = '\0';
	      if (! fchat_send (zsub, qsys, qdial, zphone, ftranslate))
		return FALSE;

	      if (znext == NULL)
		break;

	      ++znext;
	      azstrings[0] = znext;
	      znext = strchr (azstrings[0], '-');
	      if (znext != NULL)
		*znext = '\0';
	      aclens[0] = ccescape (azstrings[0]);
	    }
	}

      if (*zchat == '\0')
	return TRUE;

      cchatlen = strcspn (zchat, " ");
      strncpy (zbuf, zchat, cchatlen);
      zbuf[cchatlen] = '\0';
      zchat += cchatlen;
      if (*zchat != '\0')
	++zchat;

      if (! fchat_send (zbuf, qsys, qdial, zphone, ftranslate))
	return FALSE;
    }

  /* The chat sequence has been completed.  */
  return TRUE;
}

/* Translate escape sequences within an expect string.  */

static int
ccescape (z)
     char *z;
{
  char *zto, *zfrom;
  
  zto = z;
  zfrom = z;
  while (*zfrom != '\0')
    {
      if (*zfrom != '\\')
	{
	  *zto++ = *zfrom++;
	  continue;
	}
      ++zfrom;
      switch (*zfrom)
	{
	case 'b':
	  *zto++ = '\b';
	  break;
	case 'n':
	  *zto++ = '\n';
	  break;
	case 'N':
	  *zto++ = '\0';
	  break;
	case 'r':
	  *zto++ = '\r';
	  break;
	case 's':
	  *zto++ = ' ';
	  break;
	case 't':
	  *zto++ = '\t';
	  break;
	case '\\':
	  *zto++ = '\\';
	  break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  {
	    int i;

	    i = *zfrom - '0';
	    if (zfrom[1] >= '0' && zfrom[1] <= '7')
	      i = 8 * i + *++zfrom - '0';
	    if (zfrom[1] >= '0' && zfrom[1] <= '7')
	      i = 8 * i + *++zfrom - '0';
	    *zto++ = (char) i;
	  }
	  break;
	case 'x':
	  {
	    int i;

	    i = 0;
	    while (isxdigit (BUCHAR (zfrom[1])))
	      {
		if (isdigit (BUCHAR (zfrom[1])))
		  i = 16 * i + *++zfrom - '0';
		else if (isupper (BUCHAR (zfrom[1])))
		  i = 16 * i + *++zfrom - 'A';
		else
		  i = 16 * i + *++zfrom - 'a';
	      }
	    *zto++ = (char) i;
	  }
	  break;
	default:
	  ulog (LOG_ERROR, "Unknown escape character '%d' in expect string",
		*zfrom);
	  return -1;
	}

      ++zfrom;
    }

  return zto - z;
}

/* Read characters and wait for one of a set of memory strings to come
   in.  This returns the index into the array of the string that
   arrives, or -1 on timeout, or -2 on error.  */

static int
icexpect (cstrings, azstrings, aclens, ctimeout)
     int cstrings;
     char **azstrings;
     int *aclens;
     int ctimeout;
{
  int i;
  int cmin, cmax;
  char *zhave;
  int chave;
  long iendtime;

  cmax = cmin = aclens[0];
  for (i = 1; i < cstrings; i++)
    {
      if (cmax < aclens[i])
	cmax = aclens[i];
      if (cmin > aclens[i])
	cmin = aclens[i];
    }

  zhave = (char *) alloca (cmax);
  chave = 0;

  iendtime = isysdep_time () + ctimeout;

  while (TRUE)
    {
      int bchar;

      /* If we have no more time, get out.  */
      if (ctimeout < 0)
	return -1;

      /* Read one character at a time.  We could use a more complex
	 algorithm to read in larger batches, but it's probably not
	 worth it.  If the buffer is full, shift it left; we already
	 know that no string matches, and the buffer holds the largest
	 string, so this can't lose a match.  */
      if (chave >= cmax)
	{
	  xmemmove (zhave, zhave + 1, cmax - 1);
	  --chave;
	}

      /* The timeout/error return values from breceive_char are the
	 same as for this function.  */
      bchar = breceive_char (ctimeout, TRUE);
      if (bchar < 0)
	return bchar;

      /* Some systems send out characters with parity bits turned on.
	 There should be some way for the chat script to specify
	 parity.  */
      zhave[chave] = bchar & 0x7f;

      ++chave;

      /* See if any of the strings can be found in the buffer.  Since
	 we read one character at a time, the string can only be found
	 at the end of the buffer.  */
      for (i = 0; i < cstrings; i++)
	{
	  if (aclens[i] <= chave
	      && memcmp (zhave + chave - aclens[i], azstrings[i],
			 aclens[i]) == 0)
	    return i;
	}

      ctimeout = (int) (iendtime - isysdep_time ());
    }
}

/* Send a string out.  This has to parse escape sequences as it goes.
   Note that it handles the dialer escape sequences (\e, \E, \D, \T)
   although they make no sense for chatting with a system.  */

boolean
fchat_send (z, qsys, qdial, zphone, ftranslate)
     const char *z;
     const struct ssysteminfo *qsys;
     const struct sdialer *qdial;
     const char *zphone;
     boolean ftranslate;
{
  boolean fnocr;
  boolean (*pfwrite) P((const char *, int));
  char *zcallout_login;
  char *zcallout_pass;

  if (strcmp (z, "\"\"") == 0)
    return TRUE;

  fnocr = FALSE;
  pfwrite = fport_write;
  zcallout_login = NULL;
  zcallout_pass = NULL;

  while (*z != '\0')
    {
      const char *zlook;
      boolean fsend;
      char bsend;

      zlook = z + strcspn (z, "\\BE");

      if (zlook > z)
	{
	  if (! (*pfwrite) (z, zlook - z))
	    return FALSE;
	}

      if (*zlook == '\0')
	break;

      z = zlook;

      fsend = FALSE;
      switch (*z)
	{
	case 'B':
	  if (strncmp (z, "BREAK", 5) == 0)
	    {
	      if (! fport_break ())
		return FALSE;
	      z += 5;
	    }
	  else
	    {
	      fsend = TRUE;
	      bsend = 'B';
	      ++z;
	    }
	  break;
	case 'E':
	  if (strncmp (z, "EOT", 3) == 0)
	    {
	      fsend = TRUE;
	      bsend = '\004';
	    }
	  else
	    {
	      fsend = TRUE;
	      bsend = 'E';
	      ++z;
	    }
	  break;
	case '\\':
	  ++z;
	  switch (*z)
	    {
	    case 'b':
	      fsend = TRUE;
	      bsend = '\b';
	      break;
	    case 'c':
	      fnocr = TRUE;
	      break;
	    case 'd':
	      usysdep_sleep (1);
	      break;
	    case 'e':
	      pfwrite = fport_write;
	      break;
	    case 'E':
	      pfwrite = fcecho_send;
	      break;
	    case 'K':
	      if (! fport_break ())
		return FALSE;
	      break;
	    case 'n':
	      fsend = TRUE;
	      bsend = '\n';
	      break;
	    case 'N':
	      fsend = TRUE;
	      bsend = '\0';
	      break;
	    case 'p':
	      usysdep_pause ();
	      break;
	    case 'r':
	      fsend = TRUE;
	      bsend = '\r';
	      break;
	    case 's':
	      fsend = TRUE;
	      bsend = ' ';
	      break;
	    case 't':
	      fsend = TRUE;
	      bsend = '\t';
	      break;
	    case '\\':
	      fsend = TRUE;
	      bsend = '\\';
	      break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      fsend = TRUE;
	      bsend = *z - '0';
	      if (z[1] >= '0' && z[1] <= '7')
		bsend = (char) (8 * bsend + *++z - '0');
	      if (z[1] >= '0' && z[1] <= '7')
		bsend = (char) (8 * bsend + *++z - '0');
	      break;
	    case 'x':
	      fsend = TRUE;
	      bsend = 0;
	      while (isxdigit (BUCHAR (z[1])))
		{
		  if (isdigit (BUCHAR (z[1])))
		    bsend = (char) (16 * bsend + *++z - '0');
		  else if (isupper (BUCHAR (z[1])))
		    bsend = (char) (16 * bsend + *++z - 'A');
		  else
		    bsend = (char) (16 * bsend + *++z - 'a');
		}
	      break;
	    case 'L':
	      {
		const char *zlog;

		if (qsys == NULL)
		  {
		    ulog (LOG_ERROR, "Illegal use of \\L");
		    return FALSE;
		  }
		zlog = qsys->zcall_login;
		if (zlog == NULL)
		  {
		    ulog (LOG_ERROR, "\\L in chat but no login defined");
		    return FALSE;
		  }
		if (zlog[0] == '*' && zlog[1] == '\0')
		  {
		    if (zcallout_login == NULL
			&& ! fcallout_login (qsys, &zcallout_login,
					     &zcallout_pass))
		      return FALSE;
		    zlog = zcallout_login;
		  }
		if (! (*pfwrite) (zlog, strlen (zlog)))
		  return FALSE;
	      }
	      break;
	    case 'P':
	      {
		const char *zpass;

		if (qsys == NULL)
		  {
		    ulog (LOG_ERROR, "Illegal use of \\P");
		    return FALSE;
		  }
		zpass = qsys->zcall_password;
		if (zpass == NULL)
		  {
		    ulog (LOG_ERROR, "\\P in chat but no password defined");
		    return FALSE;
		  }
		if (zpass[0] == '*' && zpass[1] == '\0')
		  {
		    if (zcallout_pass == NULL
			&& ! fcallout_login (qsys, &zcallout_login,
					     &zcallout_pass))
		      return FALSE;
		    zpass = zcallout_pass;
		  }
		if (! (*pfwrite) (zpass, strlen (zpass)))
		  return FALSE;
	      }
	      break;
	    case 'D':
	      if (qdial == NULL || zphone == NULL)
		{
		  ulog (LOG_ERROR, "Illegal use of \\D");
		  return FALSE;
		}
	      if (! fcphone (qdial, zphone, pfwrite, ftranslate))
		return FALSE;
	      break;
	    case 'T':
	      if (qdial == NULL || zphone == NULL)
		{
		  ulog (LOG_ERROR, "Illegal use of \\T");
		  return FALSE;
		}
	      if (! fcphone (qdial, zphone, pfwrite, TRUE))
		return FALSE;
	      break;
	    case 'M':
	      if (qdial == NULL)
		{
		  ulog (LOG_ERROR, "Illegal use of \\M");
		  return FALSE;
		}
	      if (! fport_no_carrier ())
		return FALSE;
	      break;
	    case 'm':
	      if (qdial == NULL)
		{
		  ulog (LOG_ERROR, "Illegal use of \\m");
		  return FALSE;
		}
	      if (! fport_need_carrier ())
		return FALSE;
	      break;
	    default:
	      ulog (LOG_ERROR,
		    "Unknown escape character '%d' in send string", *z);
	      return FALSE;
	    }
	  ++z;
	  break;
#if DEBUG > 0
	default:
	  ulog (LOG_FATAL, "fchat_send: Can't happen");
	  break;
#endif /* DEBUG */
	}
      
      if (fsend)
	{
	  if (! (*pfwrite) (&bsend, 1))
	    return FALSE;
	}
    }

  /* Clobber and free the login and password names that came from
     the call out file.  We probably shouldn't even keep them around
     this long.  */

  if (zcallout_login != NULL)
    {
      memset (zcallout_login, 0, strlen (zcallout_login));
      xfree ((pointer) zcallout_login);
    }
  if (zcallout_pass != NULL)
    {
      memset (zcallout_pass, 0, strlen (zcallout_pass));
      xfree ((pointer) zcallout_pass);
    }

  /* Output a final carriage return, unless there was a \c.  Don't
     bother to check for an echo.  */
  if (! fnocr)
    {
      char b;

      b = '\r';
      if (! fport_write (&b, 1))
	return FALSE;
    }

  return TRUE;
}

/* Write out a phone number with optional dialcode translation.  */

static boolean
fcphone (qdial, zphone, pfwrite, ftranslate)
     const struct sdialer *qdial;
     const char *zphone;
     boolean (*pfwrite) P((const char *zwrite, int cwrite));
     boolean ftranslate;
{
  /* If we are doing dialcode translation, attempt to translate all
     alphabetic characters found at the start of the phone number.  */

 if (ftranslate)
    {
      char *zdialcode, *zto;
      const char *zfrom;

      zdialcode = (char *) alloca (strlen (zphone) + 1);
      zfrom = zphone;
      zto = zdialcode;
      while (*zfrom != '\0' && isalpha (BUCHAR (*zfrom)))
	*zto++ = *zfrom++;
      *zto = '\0';

      if (*zdialcode != '\0')
	{
	  struct smulti_file *qmulti;
	  struct scmdtab as[2];
	  const char *zprefix;

	  qmulti = qmulti_open (zDialcodefile);
	  if (qmulti == NULL)
	    return FALSE;

	  as[0].zcmd = zdialcode;
	  as[0].itype = CMDTABTYPE_STRING;
	  as[0].pvar = (pointer) &zprefix;
	  as[0].ptfn = NULL;
	  as[1].zcmd = NULL;

	  zprefix = NULL;

	  uprocesscmds ((FILE *) NULL, qmulti, as, (const char *) NULL, 0);

	  (void) fmulti_close (qmulti);

	  if (zprefix == NULL)
	    ulog (LOG_ERROR, "Unknown dial code %s", zdialcode);
	  else
	    {
	      /* Output the dialcode prefix recursively.  */

	      if (! fcphone (qdial, zprefix, pfwrite, FALSE))
		return FALSE;
	      zphone = zfrom;
	    }
	}
    }

  while (TRUE)
    {
      const char *z;
      const char *zstr;

      z = zphone + strcspn (zphone, "=-");
      if (z > zphone)
	{
	  if (! (*pfwrite) (zphone, z - zphone))
	    return FALSE;
	}

      if (*z == '=')
	zstr = qdial->zdialtone;
      else if (*z == '-')
	zstr = qdial->zpause;
      else /* *z == '\0' */
	return TRUE;

      if (zstr != NULL)
	{
	  if (! (*pfwrite) (zstr, strlen (zstr)))
	    return FALSE;
	}

      zphone = z + 1;
    }

 /*NOTREACHED*/
}

/* Write out a string making sure the each character is echoed back.  */

static boolean
fcecho_send (zwrite, cwrite)
     const char *zwrite;
     int cwrite;
{
  const char *zend;

  zend = zwrite + cwrite;

  for (; zwrite < zend; zwrite++)
    {
      int b;

      if (! fport_write (zwrite, 1))
	return FALSE;
      do
	{
	  /* We arbitrarily wait five seconds for the echo.  */
	  b = breceive_char (5, TRUE);
	  /* Now b == -1 on timeout, -2 on error.  */
	  if (b < 0)
	    {
	      if (b == -1)
		ulog (LOG_ERROR, "Character not echoed");
	      return FALSE;
	    }
	}
      while (b != BUCHAR (*zwrite));
    }

  return TRUE;
}
