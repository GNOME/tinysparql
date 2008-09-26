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

#include "xesam-g-testsearcher.h"
#include <xesam-glib/xesam-g-searcher.h>
#include <xesam-glib/xesam-glib-globals.h>
#include "xesam-g-utils.h"

enum  {
	XESAM_G_TEST_SEARCHER_DUMMY_PROPERTY
};

typedef struct {
	GValue	live;					// Boolean
	GValue	hit_fields;			// GStrv
	GValue	hit_fields_extended;	// GStrv
	GValue	hit_snippet_length;	// UInt
	GValue	sort_primary;			// String
	GValue	sort_secondary;		// String
	GValue	sort_order;			// String
	GValue	vendor_id;				// String
	GValue	vendor_version;		// UInt
	GValue	vendor_display;		// String
	GValue	vendor_xesam;			// UInt
	GValue	vendor_ontology_fields;  // GStrv
	GValue	vendor_ontology_contents;// GStrv
	GValue	vendor_ontology_sources; // GStrv
	GValue	vendor_extensions;		// GStrv
	GValue	vendor_ontologies;		// GPtrArray(GStrv)
	GValue	vendor_maxhits;		// UInt
} Properties;

struct _XesamGTestSearcherPrivate {
	Properties		*props;
};

static void xesam_g_test_searcher_real_new_session		(XesamGSearcher		*base,
														 XesamGSearcherGotHandle callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_close_session	(XesamGSearcher		*base,
														 const gchar			*session_handle,
														 XesamGSearcherVoidResponse callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_get_property	(XesamGSearcher		*base,
														 const gchar			*session_handle,
														 const gchar			*prop_name,
														 XesamGSearcherGotProperty callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_set_property	(XesamGSearcher		*base,
														 const gchar		*session_handle,
														 const gchar		*prop_name,
														 const GValue		*value,
														 XesamGSearcherGotProperty callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_new_search		(XesamGSearcher		*base,
														 const gchar			*session_handle,
														 const gchar			*query,
														 XesamGSearcherGotHandle callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_start_search	(XesamGSearcher		*base,
														 const gchar			*search_handle,
														 XesamGSearcherVoidResponse callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_close_search	(XesamGSearcher		*base,
														 const gchar			*search_handle,
														 XesamGSearcherVoidResponse callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_get_hits			(XesamGSearcher		*base,
														 const gchar			*search_handle,
														 guint				count,
														 XesamGSearcherGotHits callback,
														 gpointer			user_data);

static void xesam_g_test_searcher_real_get_hit_data		(XesamGSearcher		*base,
														 const gchar		*search_handle,
														 GArray				*hit_ids,
														 GStrv				field_names,
														 XesamGSearcherGotHits callback,
														 gpointer user_data);

static void xesam_g_test_searcher_real_get_hit_count	(XesamGSearcher		*base,
														 const gchar			*search_handle,
														 XesamGSearcherGotHitCount callback,
														 gpointer user_data);

static void xesam_g_test_searcher_real_get_state		(XesamGSearcher		*base,
														 XesamGSearcherGotState callback,
														 gpointer			user_data);


static gpointer xesam_g_test_searcher_parent_class = NULL;
static XesamGSearcherIface* xesam_g_test_searcher_xesam_g_searcher_parent_iface = NULL;


static void
xesam_g_test_searcher_real_new_session (XesamGSearcher	*base,
										XesamGSearcherGotHandle callback,
										gpointer		user_data)
{
	static int session_num = 0;

	XesamGTestSearcher * self;
	GString *session_handle_s;

	self = XESAM_G_TEST_SEARCHER (base);
	session_handle_s = g_string_new (NULL);
	g_string_printf (session_handle_s, "dummy-session-%d", session_num);

	callback (base, g_string_free (session_handle_s, FALSE), user_data, NULL);
}


static void
xesam_g_test_searcher_real_close_session (XesamGSearcher	*base,
										  const gchar		*session_handle,
										  XesamGSearcherVoidResponse callback,
										  gpointer			user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (session_handle != NULL);

	callback (base, user_data, NULL);
}


static void
xesam_g_test_searcher_real_get_property (XesamGSearcher		*base,
										 const gchar		*session_handle,
										 const gchar		*prop_name,
										 XesamGSearcherGotProperty callback,
										 gpointer			user_data)
{
	XesamGTestSearcher  *self;
	GValue				*target_val;

	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (session_handle != NULL);
	g_return_if_fail (prop_name != NULL);

	if (g_str_equal (prop_name, "search.live"))
		target_val = &self->priv->props->live;
	else if (g_str_equal (prop_name, "hit.fields"))
		target_val = &self->priv->props->hit_fields;
	else if (g_str_equal (prop_name, "hit.fields.extended"))
		target_val = &self->priv->props->hit_fields_extended;
	else
		g_critical ("GetProperty '%s' not supported by test searcher", prop_name);

	callback (base, g_strdup (prop_name),
			  xesam_g_clone_value(target_val), user_data, NULL);
}


static void
xesam_g_test_searcher_real_set_property (XesamGSearcher	*base,
										 const gchar	*session_handle,
										 const gchar	*prop_name,
										 const GValue	*value,
										 XesamGSearcherGotProperty callback,
										 gpointer		user_data)
{
	XesamGTestSearcher  *self;
	GValue				*target_val;

	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (session_handle != NULL);
	g_return_if_fail (prop_name != NULL);
	g_return_if_fail (G_IS_VALUE(value));

	if (g_str_equal (prop_name, "search.live")) {
		target_val = &self->priv->props->live;
	} else if (g_str_equal (prop_name, "hit.fields")) {
		target_val =  &self->priv->props->hit_fields;
	} else if (g_str_equal (prop_name, "hit.fields.extended")) {
		target_val =  &self->priv->props->hit_fields_extended;
	} else
		g_critical ("SetProperty '%s' not supported by test searcher", prop_name);

	callback (base, g_strdup(prop_name),
			  xesam_g_clone_value(target_val), user_data, NULL);
}


static void
xesam_g_test_searcher_real_new_search (XesamGSearcher	*base,
									   const gchar		*session_handle,
									   const gchar		*query,
									   XesamGSearcherGotHandle callback,
									   gpointer user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (session_handle != NULL);
	g_return_if_fail (query != NULL);
}


static void
xesam_g_test_searcher_real_start_search (XesamGSearcher	*base,
										 const gchar		*search_handle,
										 XesamGSearcherVoidResponse callback,
										 gpointer		user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (search_handle != NULL);
}


static void
xesam_g_test_searcher_real_close_search (XesamGSearcher	*base,
										 const gchar		*search_handle,
										 XesamGSearcherVoidResponse callback,
										 gpointer user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (search_handle != NULL);
}


static void
xesam_g_test_searcher_real_get_hits (XesamGSearcher	*base,
									 const gchar		*search_handle,
									 guint			count,
									 XesamGSearcherGotHits callback,
									 gpointer user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (search_handle != NULL);
}


static void
xesam_g_test_searcher_real_get_hit_data (XesamGSearcher	*base,
										 const gchar	*search_handle,
										 GArray			*hit_ids,
										 GStrv			field_names,
										 XesamGSearcherGotHits callback,
										 gpointer user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (search_handle != NULL);
	g_return_if_fail (field_names != NULL);
	g_return_if_fail (hit_ids != NULL);
}


static void
xesam_g_test_searcher_real_get_hit_count (XesamGSearcher	*base,
										  const gchar		*search_handle,
										  XesamGSearcherGotHitCount callback,
										  gpointer			user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
	g_return_if_fail (search_handle != NULL);
}


static void
xesam_g_test_searcher_real_get_state (XesamGSearcher	*base,
									  XesamGSearcherGotState callback,
									  gpointer			user_data)
{
	XesamGTestSearcher * self;
	self = XESAM_G_TEST_SEARCHER (base);
}


XesamGTestSearcher*
xesam_g_test_searcher_new (void)
{
	XesamGTestSearcher * self;
	self = g_object_newv (XESAM_TYPE_G_TEST_SEARCHER, 0, NULL);
	return self;
}


static void
xesam_g_test_searcher_finalize (GObject *obj)
{
	Properties					*props;
	XesamGTestSearcher			*self;
	XesamGTestSearcherPrivate	*priv;

	self = XESAM_G_TEST_SEARCHER (obj);
	priv = self->priv;
	props = priv->props;

	/* FIXME: Free prop GValue contents */

	g_free (props);
	g_free (priv);

	g_debug ("Finalized test searcher\n");
}

static void
xesam_g_test_searcher_class_init (XesamGTestSearcherClass * klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	xesam_g_test_searcher_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = xesam_g_test_searcher_finalize;
}


static void
xesam_g_test_searcher_xesam_g_searcher_interface_init (XesamGSearcherIface * iface)
{
	xesam_g_test_searcher_xesam_g_searcher_parent_iface = g_type_interface_peek_parent (iface);
	iface->new_session = xesam_g_test_searcher_real_new_session;
	iface->close_session = xesam_g_test_searcher_real_close_session;
	iface->get_property = xesam_g_test_searcher_real_get_property;
	iface->set_property = xesam_g_test_searcher_real_set_property;
	iface->new_search = xesam_g_test_searcher_real_new_search;
	iface->start_search = xesam_g_test_searcher_real_start_search;
	iface->close_search = xesam_g_test_searcher_real_close_search;
	iface->get_hits = xesam_g_test_searcher_real_get_hits;
	iface->get_hit_data = xesam_g_test_searcher_real_get_hit_data;
	iface->get_hit_count = xesam_g_test_searcher_real_get_hit_count;
	iface->get_state = xesam_g_test_searcher_real_get_state;
}


static void
xesam_g_test_searcher_init (XesamGTestSearcher *self)
{
	Properties *props;

	self->priv = g_new0 (XesamGTestSearcherPrivate, 1);
	props = g_new0 (Properties, 1);
	self->priv->props = props;

	g_value_init(&props->live, G_TYPE_BOOLEAN);
	g_value_init(&props->hit_fields, G_TYPE_STRV);
	g_value_init(&props->hit_fields_extended, G_TYPE_STRV);
	g_value_init(&props->hit_snippet_length, G_TYPE_UINT);
	g_value_init(&props->sort_primary, G_TYPE_STRING);
	g_value_init(&props->sort_secondary, G_TYPE_STRING);
	g_value_init(&props->sort_order, G_TYPE_STRING);
	g_value_init(&props->vendor_id, G_TYPE_STRING);
	g_value_init(&props->vendor_version, G_TYPE_UINT);
	g_value_init(&props->vendor_display, G_TYPE_STRING);
	g_value_init(&props->vendor_xesam, G_TYPE_UINT);
	g_value_init(&props->vendor_ontology_fields, G_TYPE_STRV);
	g_value_init(&props->vendor_ontology_contents, G_TYPE_STRV);
	g_value_init(&props->vendor_ontology_sources, G_TYPE_STRV);
	g_value_init(&props->vendor_extensions, G_TYPE_STRV);
	g_value_init(&props->vendor_ontologies, XESAM_TYPE_STRV_ARRAY);
	g_value_init(&props->vendor_maxhits, G_TYPE_UINT);

	const gchar *hit_fields[2] = {"xesam:url", NULL};
	const gchar *hit_fields_extended[1] = {NULL};
	const gchar *fields[3] = {"xesam:url", "xesam:relevancyRating", NULL};
	const gchar *contents[2] = {"xesam:Content", NULL};
	const gchar *sources[2] = {"xesam:Source", NULL};
	const gchar *exts[1] = {NULL};
	const gchar *dummy_onto[4] = {"dummy-onto","0.1","/usr/share/xesam/ontologies/dummy-onto-0.1", NULL};
	GPtrArray *ontos = g_ptr_array_new ();
	g_ptr_array_add (ontos, dummy_onto);

	g_value_set_boolean (&props->live, FALSE);
	g_value_set_boxed (&props->hit_fields, hit_fields);
	g_value_set_boxed (&props->hit_fields_extended, hit_fields_extended);
	g_value_set_uint (&props->hit_snippet_length, 200);
	g_value_set_string (&props->sort_primary, "xesam:relevancyRating");
	g_value_set_string (&props->sort_secondary, "");
	g_value_set_string (&props->sort_order, "descending");
	g_value_set_string (&props->vendor_id, "XesamGLibTestSearcher");
	g_value_set_uint (&props->vendor_version, 90);
	g_value_set_string (&props->vendor_display, "Test Searcher for Xesam-GLib");
	g_value_set_uint (&props->vendor_xesam, 90);
	g_value_set_boxed (&props->vendor_ontology_fields, fields);
	g_value_set_boxed (&props->vendor_ontology_contents, contents);
	g_value_set_boxed (&props->vendor_ontology_sources, sources);
	g_value_set_boxed (&props->vendor_extensions, exts);
	g_value_set_boxed(&props->vendor_ontologies, ontos);
	g_value_set_uint (&props->vendor_maxhits, 50);
}


GType
xesam_g_test_searcher_get_type (void)
{
	static GType xesam_g_test_searcher_type_id = 0;

	if (G_UNLIKELY (xesam_g_test_searcher_type_id == 0)) {

		static const GTypeInfo g_define_type_info = {
			sizeof (XesamGTestSearcherClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) xesam_g_test_searcher_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (XesamGTestSearcher),
			0,
			(GInstanceInitFunc) xesam_g_test_searcher_init
		};

		static const GInterfaceInfo xesam_g_searcher_info = {
			(GInterfaceInitFunc) xesam_g_test_searcher_xesam_g_searcher_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL};

		xesam_g_test_searcher_type_id = g_type_register_static (G_TYPE_OBJECT,
																"XesamGTestSearcher",
																&g_define_type_info,
																0);

		g_type_add_interface_static (xesam_g_test_searcher_type_id,
									 XESAM_TYPE_G_SEARCHER,
									 &xesam_g_searcher_info);
	}

	return xesam_g_test_searcher_type_id;
}
