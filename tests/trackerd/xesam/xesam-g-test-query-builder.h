/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * xesam-glib
 * Copyright (C) Mikkel Kamstrup Erlandsen 2008 <mikkel.kamstrup@gmail.com>
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

#ifndef __XESAM_G_TEST_QUERY_BUILDER_H__
#define __XESAM_G_TEST_QUERY_BUILDER_H__

#include <glib.h>
#include <glib-object.h>
#include "xesam-glib/xesam-g-query-builder.h"
#include "xesam-glib/xesam-g-query-token.h"

G_BEGIN_DECLS


#define XESAM_TYPE_G_TEST_QUERY_BUILDER (xesam_g_test_query_builder_get_type ())
#define XESAM_G_TEST_QUERY_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XESAM_TYPE_G_TEST_QUERY_BUILDER, XesamGTestQueryBuilder))
#define XESAM_G_TEST_QUERY_BUILDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XESAM_TYPE_G_TEST_QUERY_BUILDER, XesamGTestQueryBuilderClass))
#define XESAM_IS_G_TEST_QUERY_BUILDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XESAM_TYPE_G_TEST_QUERY_BUILDER))
#define XESAM_IS_G_TEST_QUERY_BUILDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XESAM_TYPE_G_TEST_QUERY_BUILDER))
#define XESAM_G_TEST_QUERY_BUILDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XESAM_TYPE_G_TEST_QUERY_BUILDER, XesamGTestQueryBuilderClass))

typedef struct _XesamGTestQueryBuilder XesamGTestQueryBuilder;
typedef struct _XesamGTestQueryBuilderClass XesamGTestQueryBuilderClass;
typedef struct _XesamGTestQueryBuilderPrivate XesamGTestQueryBuilderPrivate;

struct _XesamGTestQueryBuilder {
	GObject parent;
	XesamGTestQueryBuilderPrivate *priv;
};
struct _XesamGTestQueryBuilderClass {
	GObjectClass parent;
};

XesamGTestQueryBuilder*		xesam_g_test_query_builder_new			(void);

GType						xesam_g_test_query_builder_get_type		(void);

void						xesam_g_test_query_builder_expect_data	(XesamGTestQueryBuilder	*self,
																	 XesamGQueryToken		token,
																	 const gchar			*attr1,
																	 const gchar			*attr2,
																	 const gboolean			*bool_attr,
																	 const gchar			*value);

void						xesam_g_test_query_builder_expect_close_clause
																	(XesamGTestQueryBuilder	*self);

void						xesam_g_test_query_builder_expect_close_query
																	(XesamGTestQueryBuilder	*self);

G_END_DECLS

#endif
