/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia (urho.konttori@nokia.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBTRACKER_COMMON_H__
#define __LIBTRACKER_COMMON_H__

#include <glib.h>

G_BEGIN_DECLS

#if !defined (TRACKER_ENABLE_INTERNALS) && !defined (TRACKER_COMPILATION)
#error "TRACKER_ENABLE_INTERNALS not defined, this must be defined to use tracker's internal functions"
#endif

#define __LIBTRACKER_COMMON_INSIDE__

#include "tracker-class.h"
#include "tracker-config.h"
#include "tracker-file-utils.h"
#include "tracker-language.h"
#include "tracker-module-config.h"
#include "tracker-namespace.h"
#include "tracker-ontology.h"
#include "tracker-parser.h"
#include "tracker-power.h"
#include "tracker-property.h"
#include "tracker-sparql-builder.h"
#include "tracker-storage.h"
#include "tracker-type-utils.h"
#include "tracker-utils.h"

#undef __LIBTRACKER_COMMON_INSIDE__


G_END_DECLS

#endif /* __LIBTRACKER_COMMON_H__ */
