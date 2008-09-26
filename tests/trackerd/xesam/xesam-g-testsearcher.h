/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * xesam-glib
 * Copyright (C) Mikkel Kamstrup Erlandsen 2007 <mikkel.kamstrup@gmail.com>
 *
 * xesam-glib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * xesam-glib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with xesam-glib.  If not, write to:
 *	The Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor
 *	Boston, MA  02110-1301, USA.
 */

#ifndef __XESAM_G_TESTSEARCHER_H__
#define __XESAM_G_TESTSEARCHER_H__

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include "xesam-glib/xesam-g-searcher.h"

G_BEGIN_DECLS


#define XESAM_TYPE_G_TEST_SEARCHER (xesam_g_test_searcher_get_type ())
#define XESAM_G_TEST_SEARCHER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XESAM_TYPE_G_TEST_SEARCHER, XesamGTestSearcher))
#define XESAM_G_TEST_SEARCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XESAM_TYPE_G_TEST_SEARCHER, XesamGTestSearcherClass))
#define XESAM_IS_G_TEST_SEARCHER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XESAM_TYPE_G_TEST_SEARCHER))
#define XESAM_IS_G_TEST_SEARCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XESAM_TYPE_G_TEST_SEARCHER))
#define XESAM_G_TEST_SEARCHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XESAM_TYPE_G_TEST_SEARCHER, XesamGTestSearcherClass))

typedef struct _XesamGTestSearcher XesamGTestSearcher;
typedef struct _XesamGTestSearcherClass XesamGTestSearcherClass;
typedef struct _XesamGTestSearcherPrivate XesamGTestSearcherPrivate;

struct _XesamGTestSearcher {
	GObject parent;
	XesamGTestSearcherPrivate *priv;
};
struct _XesamGTestSearcherClass {
	GObjectClass parent;
};

XesamGTestSearcher* xesam_g_test_searcher_new (void);
GType xesam_g_test_searcher_get_type (void);

G_END_DECLS

#endif
