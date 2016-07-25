//  crm114_sysincludes.h - Files that we include from the system.

// Copyright 2009-2010 William S. Yerazunis.
//
//   This file is part of the CRM114 Library.
//
//   The CRM114 Library is free software: you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   The CRM114 Library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public License
//   along with the CRM114 Library.  If not, see <http://www.gnu.org/licenses/>.

//   Files that we include from the system.
#ifndef __CRM114_SYSINCLUDES_H__
#define __CRM114_SYSINCLUDES_H__

// autoconf hooks
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// UNIX and Windows include files
#include <limits.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// Detect if compilation is occurring in a Microsoft compiler
#if (defined (WIN32) || defined (WIN64) || defined (_WIN32) || defined (_WIN64))
#define CRM_WINDOWS
#endif

#ifndef CRM_WINDOWS

#include <inttypes.h>

#else

//   Windows specific declarations follow

// Apparently Microsoft's C compiler (Visual Studio C++) is C89, with
// a few extensions.  No C99 "extended integer types" -- no stdint.h
// or inttypes.h -- but has a non-standard thing built into the
// compiler.

typedef  __int64 int64_t;
typedef  __int32 int32_t;
typedef  __int16 int16_t;
typedef  __int8  int8_t;

typedef  unsigned __int64 uint64_t;
typedef  unsigned __int32 uint32_t;
typedef  unsigned __int16 uint16_t;
typedef  unsigned __int8  uint8_t;

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <direct.h>
#include <io.h>
#include <windows.h>

#define snprintf _snprintf
#define strncasecmp(a,b,n) strnicmp((a), (b), (n))
#define strcasecmp(a,b) stricmp((a), (b))
typedef long int clock_t;

// Microsoft's C compiler is C89, which doesn't include inline
// functions, but Microsoft Web documentation of the C compiled by
// Visual Studio C++ says it has inline functions as an extension to
// C89, but not using the usual keyword: uses keyword "__inline".
// So here is a hack to use that keyword for Microsoft.

#define inline __inline

#endif	// CRM_WINDOWS

#endif	// !__CRM114_SYSINCLUDES_H__
