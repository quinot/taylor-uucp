/* protf.c
   The 'f' protocol.

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
   Revision 1.1  1991/11/11  04:21:16  ian
   Initial revision

   */

#include "uucp.h"

#if USE_RCS_ID
char protf_rcsid[] = "$Id$";
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "prot.h"
#include "port.h"

/* This implementation is based on code by Piet Beertema, CWI,
   Amsterdam, Sep 1984.

   This code implements the 'f' protocol, which requires a
   flow-controlled error-free seven-bit data path.  It does check for
   errors, but only at the end of each file transmission, so a noisy
   line without error correcting modems will be unusable.

   The conversion to seven bit data is done as follows, where b
   represents the character to convert:

      0 <= b <=  037: 0172, b + 0100 (0100 to 0137)
    040 <= b <= 0171:       b        ( 040 to 0171)
   0172 <= b <= 0177: 0173, b - 0100 ( 072 to 077)
   0200 <= b <= 0237: 0174, b - 0100 (0100 to 0137)
   0240 <= b <= 0371: 0175, b - 0200 ( 040 to 0171)
   0372 <= b <= 0377: 0176, b - 0300 ( 072 to 077)

   This causes all output bytes to be in the range 040 to 0176; these
   are the printable ASCII characters.  */

/* The size of the buffer we allocate to store outgoing data in.  */
#define CFBUFSIZE (256)

/* The timeout to wait for data to arrive before giving up.  */
static int cFtimeout = 30;

/* The maximum number of retries.  */
static int cFmaxretries = 2;

/* The buffer we allocate for outgoing data.  */
static char *zFbuf;

/* TRUE if we are receiving a file rather than a command.  */
static boolean fFfile;

/* The checksum so far.  */
static unsigned int iFcheck;

/* The last special byte (0172 to 0176) or 0 if none.  */
static char bFspecial;

/* The number of times we have retried this file.  */
static int cFretries;

struct scmdtab asFproto_params[] =
{
  { "timeout", CMDTABTYPE_INT, (pointer) &cFtimeout, NULL },
  { "retries", CMDTABTYPE_INT, (pointer) &cFmaxretries, NULL },
  { NULL, 0, NULL, NULL }
};

/* Start the protocol.  */

boolean
ffstart (fmaster)
     boolean fmaster;
{
  /* Allow XON/XOFF to work.  */
  if (! fport_set (PORTSETTING_SEVEN))
    return FALSE;

  /* We sleep to allow the other side to reset the terminal; this is
     what Mr. Beertema's code does.  */
  sleep (2);

  return TRUE;
}

/* Shutdown the protocol.  */

boolean
ffshutdown ()
{
  xfree ((pointer) zFbuf);
  zFbuf = NULL;
  return TRUE;
}

/* Send a command string.  We just send the string followed by a carriage
   return.  */

boolean
ffsendcmd (z)
     const char *z;
{
  int clen;
  char *zalc;

  clen = strlen (z);
  zalc = (char *) alloca (clen + 2);
  sprintf (zalc, "%s\r", z);
  return fsend_data (zalc, clen + 1, TRUE);
}

/* Get space to be filled with data.  We allocate the space from the
   heap.  */

char *
zfgetspace (pclen)
     int *pclen;
{
  *pclen = CFBUFSIZE;
  if (zFbuf == NULL)
    zFbuf = (char *) xmalloc (CFBUFSIZE);
  return zFbuf;
}

/* Send out a data packet.  We have to encode the data into seven bits
   and accumulate a checksum.  */

boolean
ffsenddata (zdata, cdata)
     char *zdata;
     int cdata;
{
  char ab[CFBUFSIZE * 2];
  char *ze;
  register unsigned int itmpchk;
      
  ze = ab;
  itmpchk = iFcheck;
  while (cdata-- > 0)
    {
      register int b;

      /* Rotate the checksum left.  */
      if ((itmpchk & 0x8000) == 0)
	itmpchk <<= 1;
      else
	{
	  itmpchk <<= 1;
	  ++itmpchk;
	}

      /* Add the next byte into the checksum.  */
      b = *zdata++ & 0xff;
      itmpchk += b;

      /* Encode the byte.  */
      if (b <= 0177)
	{
	  if (b <= 037)
	    {
	      *ze++ = '\172';
	      *ze++ = b + 0100;
	    }
	  else if (b <= 0171)
	    *ze++ = b;
	  else
	    {
	      *ze++ = '\173';
	      *ze++ = b - 0100;
	    }
	}
      else
	{
	  if (b <= 0237)
	    {
	      *ze++ = '\174';
	      *ze++ = b - 0100;
	    }
	  else if (b <= 0371)
	    {
	      *ze++ = '\175';
	      *ze++ = b - 0200;
	    }
	  else
	    {
	      *ze++ = '\176';
	      *ze++ = b - 0300;
	    }
	}
    }

  iFcheck = itmpchk;

  /* Passing FALSE tells fsend_data not to bother looking for incoming
     information, since we really don't expect any.  */
  return fsend_data (ab, ze - ab, FALSE);
}

/* Process any data in the receive buffer.  */

boolean
ffprocess (pfexit)
     boolean *pfexit;
{
  int i;
  register unsigned int itmpchk;

  if (! fFfile)
    {
      /* A command continues until a '\r' character, which we turn
	 into '\0' before calling fgot_data.  */

      while (iPrecstart != iPrecend)
	{
	  for (i = iPrecstart; i < CRECBUFLEN && i != iPrecend; i++)
	    {
	      if (abPrecbuf[i] == '\r')
		{
		  int istart;

		  abPrecbuf[i] = '\0';
		  istart = iPrecstart;
		  iPrecstart = (i + 1) % CRECBUFLEN;
		  return fgot_data (abPrecbuf + istart, i - istart + 1,
				    TRUE, FALSE, pfexit);
		}
	    }

	  if (! fgot_data (abPrecbuf + iPrecstart, i - iPrecstart,
			   TRUE, FALSE, pfexit))
	    return FALSE;

	  iPrecstart = i % CRECBUFLEN;
	}

      *pfexit = FALSE;
      return TRUE;
    }

  /* Here the data is destined for a file, and we must decode it.  */

  itmpchk = iFcheck;

  while (iPrecstart != iPrecend)
    {
      char *zstart, *zto, *zfrom;
      int c;

      zto = zfrom = zstart = abPrecbuf + iPrecstart;

      c = iPrecend - iPrecstart;
      if (c < 0)
	c = CRECBUFLEN - iPrecstart;

      while (c-- != 0)
	{
	  int b;

	  b = *zfrom++ & 0xff;
	  if (b < 040 || b > 0176)
	    {
	      ulog (LOG_ERROR, "Illegal byte %d", b);
	      return FALSE;
	    }

	  /* Characters >= 0172 are always special characters.  The
	     only legal pair of consecutive special characters
	     are 0176 0176 which immediately precede the four
	     digit checksum.  */

	  if (b >= 0172)
	    {
	      if (bFspecial != 0)
		{
		  if (bFspecial != 0176 || b != 0176)
		    {
		      ulog (LOG_ERROR, "Illegal bytes %d %d",
			    bFspecial, b);
		      return FALSE;
		    }

		  /* Pass any initial data.  */
		  if (zto != zstart)
		    {
		      if (! fgot_data (zstart, zto - zstart, FALSE,
				       TRUE, pfexit))
			return FALSE;
		    }

		  /* The next characters we want to read are the
		     checksum, so skip the second 0176.  */
		  iPrecstart = (iPrecstart + zfrom - zstart) % CRECBUFLEN;

		  iFcheck = itmpchk;

		  /* Tell fgot_data that we've read the entire file by
		     passing 0 length data.  This will set *pfexit to
		     TRUE and call fffile to verify the checksum.  */
		  return fgot_data ((char *) NULL, 0, FALSE, TRUE, pfexit);
		}

	      /* Here we have encountered a special character that
		 does not follow another special character.  */
	      bFspecial = b;
	    }
	  else
	    {
	      int bnext;

	      /* Here we have encountered a nonspecial character.  */

	      if (bFspecial == 0)
		bnext = b;
	      else
		{
		  switch (bFspecial)
		    {
		    case 0172:
		      bnext = b - 0100;
		      break;
		    case 0173:
		    case 0174:
		      bnext = b + 0100;
		      break;
		    case 0175:
		      bnext = b + 0200;
		      break;
		    case 0176:
		      bnext = b + 0300;
		      break;
		    }
		}

	      *zto++ = bnext;
	      bFspecial = 0;

	      /* Rotate the checksum left.  */
	      if ((itmpchk & 0x8000) == 0)
		itmpchk <<= 1;
	      else
		{
		  itmpchk <<= 1;
		  ++itmpchk;
		}

	      /* Add the next byte into the checksum.  */
	      itmpchk += bnext;
	    }
	}

      if (zto != zstart)
	{
#if DEBUG > 8
	  if (iDebug > 8)
	    ulog (LOG_DEBUG, "ffprocess: Calling fgot_data with %d bytes",
		  zto - zstart);
#endif
	  if (! fgot_data (zstart, zto - zstart, FALSE, TRUE, pfexit))
	    return FALSE;
	}

      iPrecstart = (iPrecstart + zfrom - zstart) % CRECBUFLEN;
    }

  iFcheck = itmpchk;

  *pfexit = FALSE;
  return TRUE;
}

/* Wait for data to come in and process it until we've finished a
   command or a file.  */

boolean
ffwait ()
{
  while (TRUE)
    {
      boolean fexit;
      int crec;

      if (! ffprocess (&fexit))
	return FALSE;
      if (fexit)
	return TRUE;

      /* If we are in file mode, we ask for a bunch of data with a one
	 second timeout.  This avoids the problem of making a lot of
	 system calls each of which returns a single character, at the
	 cost of up to a second per file transfer.  I don't know if
	 this is a good idea or not.  If we don't get anything back,
	 or we're in command mode, we ask for a single character with
	 a long timeout.  We don't have any way to jog the other side,
	 so if the timeout fails we have to error out.  */

      if (! fFfile)
	crec = 0;
      else
	{
	  if (! freceive_data (128, &crec, 1))
	    return FALSE;
	}

      if (crec == 0)
	{
	  if (! freceive_data (1, &crec, cFtimeout))
	    return FALSE;
	}

      if (crec == 0)
	{
	  ulog (LOG_ERROR, "Timed out waiting for data");
	  return FALSE;
	}
    }
}

/* File level operations.  Reset the checksums when starting to send
   or receive a file, and output the checksum when we've finished
   sending a file.  */

boolean
fffile (fstart, fsend, pfredo)
     boolean fstart;
     boolean fsend;
     boolean *pfredo;
{
  if (fstart)
    {
      iFcheck = 0xffff;
      cFretries = 0;
      if (! fsend)
	{
	  bFspecial = 0;
	  fFfile = TRUE;
	}
      return TRUE;
    }
  else
    {
      const char *z;

      *pfredo = FALSE;

      if (fsend)
	{
	  char ab[8];

	  /* Send the final checksum.  */

	  sprintf (ab, "\176\176%04x\r", iFcheck & 0xffff);
	  if (! fsend_data (ab, 7, TRUE))
	    return FALSE;

	  /* Now look for the acknowledgement.  */
	  z = zgetcmd ();
	  if (z == NULL)
	    return FALSE;

	  /* An R means to retry sending the file.  */
	  if (*z == 'R')
	    {
	      ++cFretries;
	      if (cFretries > cFmaxretries)
		{
		  ulog (LOG_ERROR, "Too many retries");
		  return FALSE;
		}
	      *pfredo = TRUE;
	      iFcheck = 0xffff;
	      return TRUE;
	    }

	  if (*z == 'G')
	    return TRUE;

#if DEBUG > 4
	  if (iDebug > 4)
	    ulog (LOG_DEBUG, "fffile: Got \"%s\"", z);
#endif

	  ulog (LOG_ERROR, "File send failed");
	  return FALSE;
	}
      else
	{
	  unsigned int icheck;

	  /* We next expect to receive a command.  */
	  fFfile = FALSE;

	  /* Get the checksum.  */
	  z = zgetcmd ();
	  if (z == NULL)
	    return FALSE;

	  if (strlen (z) != 4
	      || ! isxdigit (z[0])
	      || ! isxdigit (z[1])
	      || ! isxdigit (z[2])
	      || ! isxdigit (z[3]))
	    {
	      ulog (LOG_ERROR, "Bad checksum format");
	      return FALSE;
	    }
	  
	  icheck = strtol (z, (char **) NULL, 16);

	  if (icheck != (iFcheck & 0xffff))
	    {
#if DEBUG > 4
	      if (iDebug > 4)
		ulog (LOG_DEBUG, "Checksum failed; calculated 0x%x, got 0x%x",
		      iFcheck & 0xffff, icheck);
#endif
	      ++cFretries;
	      if (cFretries > cFmaxretries)
		{
		  ulog (LOG_ERROR, "Too many retries");
		  (void) ffsendcmd ("Q");
		  return FALSE;
		}

	      *pfredo = TRUE;
	      iFcheck = 0xffff;
	      bFspecial = 0;
	      fFfile = TRUE;

	      /* Send an R to tell the other side to resend the file.  */
	      return ffsendcmd ("R");
	    }

	  /* Send a G to tell the other side the file was received
	     correctly.  */
	  return ffsendcmd ("G");
	}
    }
}
