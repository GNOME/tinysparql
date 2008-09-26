/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * gtestextensions
 * Copyright (C) Mikkel Kamstrup Erlandsen 2008 <mikkel.kamstrup@gmail.com>
 *		 Scott Asofyet 2008 (wait_for_signal code)
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

#include <gobject/gvaluecollector.h>
#include <string.h> /* memset(), from G_VALUE_COLLECT() */
#include "gtestextensions.h"

typedef struct {
    GClosure closure;
    GValue * return_value;
    GValueArray * param_values;
    GMainLoop * loop;
} WaitForSignalClosure;


gboolean
gtx_dispatch_test_case (GtxTestContext *ctx)
{
	g_assert (ctx != NULL);
	ctx->test_func (ctx->fixture, ctx->test_data);
	return FALSE;
}

void
gtx_flush_sources (gboolean may_block)
{
	GMainContext *ctx;

	ctx = g_main_context_default ();

	while (g_main_context_pending (ctx))
		g_main_context_iteration (ctx, may_block);
}

/**
 * gtx_quit_main_loop
 * @loop: The main loop to quit
 *
 * This is a convenience to close a #GMainLoop via a #GSourceFunc.
 *
 * Returns: Always %FALSE
 */
gboolean
gtx_quit_main_loop (GMainLoop *loop)
{
	g_assert (loop != NULL);

	if (g_main_loop_is_running (loop))
		g_main_loop_quit (loop);
	else
		g_warning ("Tried to quit non-running GMainLoop@%p", loop);

	return FALSE;
}

/**
 * gtx_yield_main_loop
 * @millis: Number of milli seconds to return control to the main loop
 *
 * Return control to the main loop for a specified amount of time. This
 * is to allow async test to process events in a blocking fashion.
 */
void
gtx_yield_main_loop (guint millis)
{
	GMainLoop *recursive_main;

	recursive_main = g_main_loop_new (NULL, FALSE);

	g_timeout_add (millis, (GSourceFunc)gtx_quit_main_loop, recursive_main);
	g_main_loop_run (recursive_main);

	gtx_flush_sources (FALSE);

	g_main_loop_unref (recursive_main);
}

static void
wait_for_signal_closure_marshal (GClosure *closure,
				 GValue *return_value,
				 guint n_param_values,
				 const GValue *param_values,
				 gpointer invocation_hint,
				 gpointer marshal_data)
{
    WaitForSignalClosure * wfsclosure = (WaitForSignalClosure *) closure;
    guint i;

    wfsclosure->param_values = g_value_array_new (n_param_values);
    for (i = 0 ; i < n_param_values ; i++)
	g_value_array_append (wfsclosure->param_values, param_values + i);

    if (return_value)
	g_value_copy (wfsclosure->return_value, return_value);

    gtx_quit_main_loop (wfsclosure->loop);

    (void) invocation_hint;
    (void) marshal_data;
}

static gboolean
wait_for_signal_values (GObject * object,
			gint max_wait_ms,
			guint signal_id,
			GQuark detail,
			GValue * return_value,
			GValueArray ** param_values)
{
    WaitForSignalClosure * wfs;
    guint handler_id;
    guint max_timeout_id;
    gboolean timed_out = FALSE;

    wfs = (WaitForSignalClosure *)
	g_closure_new_simple (sizeof (WaitForSignalClosure), NULL);
    wfs->loop = g_main_loop_new (NULL, FALSE);
    wfs->return_value = return_value;
    wfs->param_values = NULL;

    g_closure_set_marshal (&wfs->closure, wait_for_signal_closure_marshal);

    handler_id = g_signal_connect_closure_by_id (object, signal_id, detail,
						 &wfs->closure, FALSE);

    max_timeout_id = g_timeout_add (max_wait_ms, (GSourceFunc)gtx_quit_main_loop,
									wfs->loop);

    g_main_loop_run (wfs->loop);

    g_closure_invalidate (&wfs->closure);

    if (wfs->param_values) {
	if (param_values)
	    *param_values = wfs->param_values;
	else
	    g_value_array_free (wfs->param_values);

	wfs->param_values = NULL;

    } else {
	timed_out = TRUE;
    }

    g_main_loop_unref (wfs->loop);
    wfs->loop = NULL;

    /* Closure will be destroyed here */
    g_signal_handler_disconnect (object, handler_id);
	g_source_remove (max_timeout_id);

    return timed_out;
}


/**
 * gtx_wait_for_signal
 * @object:  The object
 * @max_wait_ms:  Maximum number of milliseconds to wait before giving up
 *		  and failing; passed directly to g_timeout_add().
 * @detailed_signal:  Detailed signal name for which to wait.
 * @...:  If @detailed_signal has a return value, the first vararg should
 *	  be the value to return from the signal handler.  The rest of the
 *	  varargs should be pointers to variables in which to store the
 *	  parameters passed to the signal.  Pass NULL for any param in
 *	  which you are not interested.  The caller is responsible for
 *	  freeing or unreffing any strings or objects returned here.
 *
 * returns: True if the signal was received, false if the timeout was
 *	    reached without the signal firing.
 */
gboolean
gtx_wait_for_signal (GObject * object,
		 gint max_wait_ms,
		 const gchar * detailed_signal,
		 ...)
{
    GSignalQuery query;
    GValue * return_value = NULL, the_return_value = { 0, };
    GValueArray * param_values = NULL;
    guint signal_id;
    GQuark detail;
    gboolean ret;
    va_list ap;

    if (! g_signal_parse_name (detailed_signal, G_OBJECT_TYPE (object),
			       &signal_id, &detail, FALSE))
	g_error ("Signal %s is invalid for object type %s",
		 detailed_signal, g_type_name (G_OBJECT_TYPE (object)));

    g_signal_query (signal_id, &query);

    va_start (ap, detailed_signal);

    if (query.return_type != G_TYPE_NONE) {
	GType t = query.return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
	gboolean static_scope = t & G_SIGNAL_TYPE_STATIC_SCOPE;
	gchar * error;
	g_value_init (&the_return_value, t);
	return_value = &the_return_value;
	G_VALUE_COLLECT (return_value, ap,
			 static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
			 &error);
	if (error)
	    g_error ("%s: %s", G_STRLOC, error);
    }

    ret = wait_for_signal_values (object,
				  max_wait_ms,
				  signal_id,
				  detail,
				  return_value,
				  &param_values);

    if (param_values) {
	guint i;

	/* Skip the instance */
	for (i = 1 ; i < param_values->n_values ; i++) {
	    GValue * v = g_value_array_get_nth (param_values, i);
	    switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (v))) {
		case G_TYPE_INVALID:
		case G_TYPE_NONE:
		case G_TYPE_INTERFACE:
		    break;

		case G_TYPE_CHAR:
		    {
			gchar * p = va_arg (ap, gchar *);
			if (p)
			    *p = g_value_get_char (v);
		    }
		    break;
		case G_TYPE_UCHAR:
		    {
			guchar * p = va_arg (ap, guchar *);
			if (p)
			    *p = g_value_get_uchar (v);
		    }
		    break;
		case G_TYPE_BOOLEAN:
		    {
			gboolean * p = va_arg (ap, gboolean *);
			if (p)
			    *p = g_value_get_boolean (v);
		    }
		    break;
		case G_TYPE_INT:
		    {
			gint * p = va_arg (ap, gint *);
			if (p)
			    *p = g_value_get_int (v);
		    }
		    break;
		case G_TYPE_UINT:
		    {
			guint * p = va_arg (ap, guint *);
			if (p)
			    *p = g_value_get_uint (v);
		    }
		    break;
		case G_TYPE_LONG:
		    {
			glong * p = va_arg (ap, glong *);
			if (p)
			    *p = g_value_get_long (v);
		    }
		    break;
		case G_TYPE_ULONG:
		    {
			gulong * p = va_arg (ap, gulong *);
			if (p)
			    *p = g_value_get_ulong (v);
		    }
		    break;
		case G_TYPE_INT64:
		    {
			gint64 * p = va_arg (ap, gint64 *);
			if (p)
			    *p = g_value_get_int64 (v);
		    }
		    break;
		case G_TYPE_UINT64:
		    {
			guint64 * p = va_arg (ap, guint64 *);
			if (p)
			    *p = g_value_get_uint64 (v);
		    }
		    break;
		case G_TYPE_ENUM:
		    {
			gint * p = va_arg (ap, gint *);
			if (p)
			    *p = g_value_get_int (v);
		    }
		    break;
		case G_TYPE_FLAGS:
		    {
			guint * p = va_arg (ap, guint *);
			if (p)
			    *p = g_value_get_uint (v);
		    }
		    break;
		case G_TYPE_FLOAT:
		    {
			gfloat * p = va_arg (ap, gfloat *);
			if (p)
			    *p = g_value_get_float (v);
		    }
		    break;
		case G_TYPE_DOUBLE:
		    {
			gdouble * p = va_arg (ap, gdouble *);
			if (p)
			    *p = g_value_get_double (v);
		    }
		    break;
		case G_TYPE_STRING:
		    {
			gchar ** p = va_arg (ap, gchar **);
			if (p)
			    *p = g_value_dup_string (v);
		    }
		    break;
		case G_TYPE_POINTER:
		    {
			gpointer * p = va_arg (ap, gpointer *);
			if (p)
			    *p = g_value_get_pointer (v);
		    }
		    break;
		case G_TYPE_BOXED:
		    {
			gpointer * p = va_arg (ap, gpointer *);
			if (p)
			    *p = g_value_dup_boxed (v);
		    }
		    break;
		case G_TYPE_PARAM:
		    {
			GParamSpec ** p = va_arg (ap, GParamSpec **);
			if (p)
			    *p = g_value_dup_param (v);
		    }
		    break;
		case G_TYPE_OBJECT:
		    {
			GObject ** p = va_arg (ap, GObject **);
			if (p)
			    *p = g_value_dup_object (v);
		    }
		    break;
	    }
	}

	g_value_array_free (param_values);
    }

    va_end (ap);

    return ret;
}
