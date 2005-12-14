/*
     This file is part of GNUnet.
     (C) 2001, 2002, 2003, 2004, 2005 Christian Grothoff (and other contributing authors)

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file include/platform.h
 * @brief plaform specifics
 *
 * @author Nils Durner
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "config.h"
#include "gettext.h"
#define _(a) dgettext("libextractor", a)

#include "plibc.h"

#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MINGW
 #include <sys/socket.h>
 #include <sys/mman.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
#else
 #include <winsock2.h>
#endif
#include <locale.h>
#include <iconv.h>
#include <langinfo.h>

#ifdef OSX
 #define socklen_t unsigned int
#endif

#endif
