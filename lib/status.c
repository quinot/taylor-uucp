/* status.c
   Strings for status codes.  */

#include "uucp.h"

#include "uudefs.h"

/* Status strings.  These must match enum tstatus_type.  */

#if SPOOLDIR_HDB || SPOOLDIR_SVR4

const char *azStatus[] =
{
  "SUCCESSFUL",
  "DEVICE FAILED",
  "DIAL FAILED",
  "LOGIN FAILED",
  "STARTUP FAILED",
  "CONVERSATION FAILED",
  "TALKING",
  "WRONG TIME TO CALL"
};

#else

const char *azStatus[] =
{
  "Conversation complete",
  "Port unavailable",
  "Dial failed",
  "Login failed",
  "Handshake failed",
  "Call failed",
  "Talking",
  "Wrong time to call"
};

#endif
