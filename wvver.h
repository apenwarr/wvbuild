/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2004 Net Integration Technologies, Inc.
 *
 * Version numbers for the different parts of Weaver included here.
 *
 */
#ifndef __WVVER_H
#define __WVVER_H

// *_VER is a numerical version number for use ONLY in comparisons and
// internal data structures.
// *_VER_STRING is a string for use ONLY in display strings.
//
// Never mix and match them!  WEAVER_VER_STRING may be any
//  valid string, eg "4.5 beta 6" 
// 

#ifndef NO_AUTOCONF
#include "wvautoconf.h"
#endif

#define WVDIAL_VER_STRING		"1.60"
#define RETCHMAIL_VER_STRING		"1.1"
#define WVTFTP_VER_STRING		"1.0"
#define WVSTREAMS_VER_STRING		"4.4" 
#define UNITY_VER_STRING		"0.1"

#endif // __WVVER_H
