/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * gtestextensions
 * Copyright (C) Mikkel Kamstrup Erlandsen 2008 <mikkel.kamstrup@gmail.com>
 *
 * gtestextensions is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtestextensions is distributed in the hope that it will be useful,
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

#ifndef __G_TEST_EXTENSIONS_H__
#define __G_TEST_EXTENSIONS_H__

#include <glib.h>
#include <glib/gtestutils.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef void (*GtxTestFunc) (gpointer fixture, gconstpointer test_data);

typedef struct {
	gpointer		fixture;
	gconstpointer	test_data;
	GtxTestFunc	test_func;
	GMainLoop		*loop;
} GtxTestContext;

gboolean	gtx_wait_for_signal				(GObject		*object,
											 gint			max_wait_ms,
											 const gchar	*detailed_signal,
											 ...);

gboolean	gtx_dispatch_test_case			(GtxTestContext	*ctx);

gboolean	gtx_quit_main_loop				(GMainLoop		*loop);

void		gtx_yield_main_loop				(guint			millis);

void		gtx_flush_sources				(gboolean may_block);

/**
 * GTX_DEFINE_LOOPED
 * @func: A function to be called as an idle handler in a #GMainLoop
 *
 * Declare that a test function is runnable in a #GMainLoop idle handler.
 * Use GTX_LOOPED() to get a reference to a version of @func being run
 * in a main loop.
 */
#define GTX_DEFINE_LOOPED(func) \
							void \
							func##__gtx_looped (gpointer fixture,\
												gconstpointer test_data)\
							{\
								GMainLoop   *loop;\
								GtxTestContext *ctx;\
								\
								loop = g_main_loop_new (NULL, FALSE);\
								ctx = g_new0 (GtxTestContext, 1);\
								\
								ctx->fixture = fixture;\
								ctx->test_data = test_data;\
								ctx->test_func = func;\
								ctx->loop = loop;\
								\
								g_idle_add ((GSourceFunc)gtx_dispatch_test_case,\
											ctx);\
								g_idle_add ((GSourceFunc)gtx_quit_main_loop,\
											loop);\
								\
								g_main_loop_run (loop);\
								\
								g_free (ctx);\
							}

/**
 * GTX_LOOPED
 * @func: A function on which GTX_DEFINE_LOOPED has been set
 *
 * Get a reference to the version of @func being run inside a main loop.
 */
#define GTX_LOOPED(func) func##__gtx_looped

/**
 * gtx_assert_last_unref
 * @o: A #GObject
 *
 * Calls g_object_unref on @o and raises a critical error if the
 * #GObject is not finalized after this call.
 */
/* This macro is based on code by Benjamin Otte, April, 2008 */
#define gtx_assert_last_unref(o) G_STMT_START { \
	gpointer _tmp = o; \
	g_object_add_weak_pointer (G_OBJECT(o), &_tmp); \
	g_object_unref (G_OBJECT(o)); \
	if (_tmp != NULL) \
		g_critical ("Leak detected. Object %s@%p is not unreferenced",\
					g_type_name(G_OBJECT_TYPE(o)), o);\
} G_STMT_END

G_END_DECLS

#endif /* __G_TEST_EXTENSIONS_H__ */
