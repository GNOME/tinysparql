/*
     This file is part of libextractor.
     (C) 2001, 2002, 2003, 2003, 2005 Christian Grothoff (and other contributing authors)

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
 * @file include/winproc.h
 * @brief Definitions for MS Windows
 * @author Nils Durner
 * @note This file differs from GNUnet's winproc.h
 */

#ifndef WINPROC_H
#define WINPROC_H

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitWinEnv();
void ShutdownWinEnv();

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
