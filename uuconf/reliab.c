/* reliab.c
   Subroutines to handle reliability commands for ports and dialers.

   Copyright (C) 1992, 2002 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_reliab_rcsid[] = "$Id$";
#endif

/* Handle the "seven-bit" command for a port or a dialer.  The pvar
   argument points to an integer which should be set to hold
   reliability information.  */

/*ARGSUSED*/
int
_uuconf_iseven_bit (pointer pglobal, int argc ATTRIBUTE_UNUSED, char **argv, pointer pvar, pointer pinfo ATTRIBUTE_UNUSED)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int *pi = (int *) pvar;
  int fval;
  int iret;

  iret = _uuconf_iboolean (qglobal, argv[1], &fval);
  if ((iret &~ UUCONF_CMDTABRET_KEEP) != UUCONF_SUCCESS)
    return iret;

  *pi |= UUCONF_RELIABLE_SPECIFIED;
  if (fval)
    *pi &=~ UUCONF_RELIABLE_EIGHT;
  else
    *pi |= UUCONF_RELIABLE_EIGHT;

  return iret;
}

/* Handle the "reliable" command for a port or a dialer.  The pvar
   argument points to an integer which should be set to hold
   reliability information.  */

/*ARGSUSED*/
int
_uuconf_ireliable (pointer pglobal, int argc ATTRIBUTE_UNUSED, char **argv, pointer pvar, pointer pinfo ATTRIBUTE_UNUSED)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int *pi = (int *) pvar;
  int fval;
  int iret;

  iret = _uuconf_iboolean (qglobal, argv[1], &fval);
  if ((iret &~ UUCONF_CMDTABRET_KEEP) != UUCONF_SUCCESS)
    return iret;

  *pi |= UUCONF_RELIABLE_SPECIFIED;
  if (fval)
    *pi |= UUCONF_RELIABLE_RELIABLE;
  else
    *pi &=~ UUCONF_RELIABLE_RELIABLE;

  return iret;
}

/* Handle the "half-duplex" command for a port or a dialer.  The pvar
   argument points to an integer which should be set to hold
   reliability information.  */

/*ARGSUSED*/
int
_uuconf_ihalf_duplex (pointer pglobal, int argc ATTRIBUTE_UNUSED, char **argv, pointer pvar, pointer pinfo ATTRIBUTE_UNUSED)
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int *pi = (int *) pvar;
  int fval;
  int iret;

  iret = _uuconf_iboolean (qglobal, argv[1], &fval);
  if ((iret &~ UUCONF_CMDTABRET_KEEP) != UUCONF_SUCCESS)
    return iret;

  *pi |= UUCONF_RELIABLE_SPECIFIED;
  if (fval)
    *pi &=~ UUCONF_RELIABLE_FULLDUPLEX;
  else
    *pi |= UUCONF_RELIABLE_FULLDUPLEX;

  return iret;
}
