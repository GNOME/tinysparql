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

#include "xesam-g-test-query-builder.h"
#include <xesam-glib/xesam-g-query-builder.h>
#include <xesam-glib/xesam-g-query-token.h>
#include <xesam-glib/xesam-glib-globals.h>
#include "xesam-g-utils.h"
#include "xesam-g-debug-private.h"

enum  {
	XESAM_G_TEST_QUERY_BUILDER_DUMMY_PROPERTY
};

typedef struct {
	gchar				*attr1;
	gchar				*attr2;
	gboolean			*bool_attr;
	gchar				*text;
	XesamGQueryToken	token;
} ExpectedElementData;

struct _XesamGTestQueryBuilderPrivate {
	GSList	*expected_data;
};

static gboolean xesam_g_test_query_builder_real_start_query (XesamGQueryBuilder		*self,
															 XesamGQueryToken		token,
															 const gchar			*content_cat,
															 const gchar			*source_cat,
															 GError					**error);

static gboolean xesam_g_test_query_builder_real_add_clause (XesamGQueryBuilder		*self,
															XesamGQueryToken		token,
															const gchar				*boost,
															const gboolean			*negate,
															const gchar				**attr_names,
															const gchar				**attr_vals,
															GError					**error);

static gboolean xesam_g_test_query_builder_real_add_field (XesamGQueryBuilder		*self,
														   XesamGQueryToken			token,
														   const char				*name,
														   GError					**error);

static gboolean xesam_g_test_query_builder_real_add_value (XesamGQueryBuilder		*self,
														   XesamGQueryToken			token,
														   const gchar				*value,
														   const gchar				**attr_names,
														   const gchar				**attr_vals,
														   GError					**error);

static gboolean xesam_g_test_query_builder_real_close_clause
														  (XesamGQueryBuilder		*self,
														   GError					**error);

static gboolean xesam_g_test_query_builder_real_close_query
														  (XesamGQueryBuilder		*self,
														   GError					**error);

static ExpectedElementData*
			expected_element_data_new					(guint					element_detail,
														 const gchar			*attr1,
														 const gchar			*attr2,
														 const gboolean			*bool_attr,
														 const gchar			*text);

static void expected_element_data_destroy				(ExpectedElementData	*exp_data);

static void check_expected_data							(XesamGTestQueryBuilder	*self,
														 XesamGQueryToken		token,
														 const gchar			*attr1,
														 const gchar			*attr2,
														 const gboolean			*bool_attr,
														 const gchar			*text);

static gpointer xesam_g_test_query_builder_parent_class = NULL;
static XesamGQueryBuilderIface* xesam_g_test_query_builder_parent_iface = NULL;


static gboolean
xesam_g_test_query_builder_real_start_query (XesamGQueryBuilder		*base,
											 XesamGQueryToken		token,
											 const gchar			*content_cat,
											 const gchar			*source_cat,
											 GError					**error)
{
	XesamGTestQueryBuilder	*self;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	xesam_g_debug_object (self, "start_query (%s, content='%s', source='%s')",
						  xesam_g_query_token_get_name (token),
						  content_cat, source_cat);

	check_expected_data (self, token, content_cat, source_cat, NULL, NULL);

	return TRUE;
}

static gboolean
xesam_g_test_query_builder_real_add_clause (XesamGQueryBuilder		*base,
											XesamGQueryToken		token,
											const gchar				*boost,
											const gboolean			*negate,
											const gchar				**attr_names,
											const gchar				**attr_vals,
											GError					**error)
{
	XesamGTestQueryBuilder	*self;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	xesam_g_debug_object (self, "add_clause (%s, negate='%s' boost='%s')",
						  xesam_g_query_token_get_name (token),
						  negate ? (*negate ? "True" : "False") : "NULL",
						  boost);

	check_expected_data (self, token, boost, NULL, negate, NULL);

	return TRUE;
}

static gboolean
xesam_g_test_query_builder_real_add_field (XesamGQueryBuilder		*base,
										   XesamGQueryToken			token,
										   const char				*name,
										   GError					**error)
{
	XesamGTestQueryBuilder	*self;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	xesam_g_debug_object (self, "add_field (%s, name='%s')",
						  xesam_g_query_token_get_name (token), name);

	check_expected_data (self, token, name, NULL, NULL, NULL);

	return TRUE;
}

static gboolean
xesam_g_test_query_builder_real_add_value (XesamGQueryBuilder		*base,
										   XesamGQueryToken			token,
										   const gchar				*value,
										   const gchar				**attr_names,
										   const gchar				**attr_vals,
										   GError					**error)
{
	XesamGTestQueryBuilder	*self;
	const gchar				*attr1 = NULL, *attr2 = NULL;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	if (attr_names[0]) {
		attr1 = attr_names[0];
		if (attr_names[1])
			attr2 = attr_names[1];
	}

	xesam_g_debug_object (self, "add_value (%s='%s', attr1='%s', attr2='%s')",
						  xesam_g_query_token_get_name (token),
						  value,
						  attr1,
						  attr2);

	check_expected_data (self, token, attr1, attr2, NULL, value);

	return TRUE;
}

static gboolean
xesam_g_test_query_builder_real_close_clause (XesamGQueryBuilder	*base,
											  GError				**error)
{
	XesamGTestQueryBuilder	*self;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	xesam_g_debug_object (self, "close_clause ()");

	check_expected_data (self, XESAM_G_QUERY_TOKEN_NONE, NULL, NULL, NULL, NULL);

	return TRUE;
}

static gboolean
xesam_g_test_query_builder_real_close_query (XesamGQueryBuilder	*base,
											 GError				**error)
{
	XesamGTestQueryBuilder	*self;

	g_return_val_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (base), FALSE);

	self = XESAM_G_TEST_QUERY_BUILDER (base);

	xesam_g_debug_object (self, "close_query ()");

	check_expected_data (self, XESAM_G_QUERY_TOKEN_NONE, NULL, NULL, NULL, NULL);

	return TRUE;
}


static void
xesam_g_test_query_builder_finalize (GObject *obj)
{
	GSList							*iter;
	XesamGTestQueryBuilderPrivate	*priv;
	ExpectedElementData				*exp_data;

	xesam_g_debug_object (obj, "Finalizing");

	priv = XESAM_G_TEST_QUERY_BUILDER(obj)->priv;

	for (iter = priv->expected_data; iter; iter = iter->next) {
		exp_data = (ExpectedElementData*)iter->data;
		expected_element_data_destroy (exp_data);
	}

	if (priv->expected_data) {
		g_slist_free (priv->expected_data);
		g_critical ("TestQueryBuilder still contains expected data");
	}

}

static void
xesam_g_test_query_builder_class_init (XesamGTestQueryBuilderClass * klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	xesam_g_test_query_builder_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = xesam_g_test_query_builder_finalize;
}


static void
xesam_g_test_query_builder_interface_init (XesamGQueryBuilderIface * iface)
{
	xesam_g_test_query_builder_parent_iface = g_type_interface_peek_parent (iface);

	iface->start_query = xesam_g_test_query_builder_real_start_query;
	iface->add_clause = xesam_g_test_query_builder_real_add_clause;
	iface->add_field = xesam_g_test_query_builder_real_add_field;
	iface->add_value = xesam_g_test_query_builder_real_add_value;
	iface->close_clause = xesam_g_test_query_builder_real_close_clause;
	iface->close_query = xesam_g_test_query_builder_real_close_query;
}


static void
xesam_g_test_query_builder_init (XesamGTestQueryBuilder *self)
{
	self->priv = g_new0 (XesamGTestQueryBuilderPrivate, 1);

}


GType
xesam_g_test_query_builder_get_type (void)
{
	static GType xesam_g_test_query_builder_type_id = 0;

	if (G_UNLIKELY (xesam_g_test_query_builder_type_id == 0)) {

		static const GTypeInfo g_define_type_info = {
			sizeof (XesamGTestQueryBuilderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) xesam_g_test_query_builder_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (XesamGTestQueryBuilder),
			0,
			(GInstanceInitFunc) xesam_g_test_query_builder_init
		};

		static const GInterfaceInfo xesam_g_query_builder_info = {
			(GInterfaceInitFunc) xesam_g_test_query_builder_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		xesam_g_test_query_builder_type_id = g_type_register_static (G_TYPE_OBJECT,
																	 "XesamGTestQueryBuilder",
																	 &g_define_type_info,
																	 0);

		g_type_add_interface_static (xesam_g_test_query_builder_type_id,
									 XESAM_TYPE_G_QUERY_BUILDER,
									 &xesam_g_query_builder_info);
	}

	return xesam_g_test_query_builder_type_id;
}

XesamGTestQueryBuilder*
xesam_g_test_query_builder_new (void)
{
	XesamGTestQueryBuilder	*self;

	self = g_object_new (XESAM_TYPE_G_TEST_QUERY_BUILDER, NULL);

	return self;
}

void
xesam_g_test_query_builder_expect_data (XesamGTestQueryBuilder	*self,
										XesamGQueryToken		token,
										const gchar				*attr1,
										const gchar				*attr2,
										const gboolean			*bool_attr,
										const gchar				*text)
{
	ExpectedElementData	*exp_data;

	g_return_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER(self));

	exp_data = expected_element_data_new (token, attr1, attr2, bool_attr, text);

	/* Yes, appending data to a GSList is silly, but we don't care the slightest
	 * about performance here */
	self->priv->expected_data = g_slist_append (self->priv->expected_data, exp_data);

}

void
xesam_g_test_query_builder_expect_close_clause (XesamGTestQueryBuilder	*self)
{
	g_return_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (self));

	xesam_g_test_query_builder_expect_data (self,
											XESAM_G_QUERY_TOKEN_NONE,
											NULL, NULL, NULL, NULL);
}

void
xesam_g_test_query_builder_expect_close_query (XesamGTestQueryBuilder	*self)
{
	g_return_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER (self));

	xesam_g_test_query_builder_expect_data (self,
											XESAM_G_QUERY_TOKEN_NONE,
											NULL, NULL, NULL, NULL);
}

static ExpectedElementData*
expected_element_data_new (XesamGQueryToken			token,
						   const gchar				*attr1,
						   const gchar				*attr2,
						   const gboolean			*bool_attr,
						   const gchar				*text)
{
	ExpectedElementData	*exp_data;

	exp_data = g_slice_new (ExpectedElementData);
	exp_data->token = token;
	exp_data->attr1 = g_strdup (attr1);
	exp_data->attr2 = g_strdup (attr2);
	exp_data->bool_attr = g_memdup (bool_attr, sizeof(gboolean));
	exp_data->text = g_strdup (text);

	return exp_data;
}

static void
expected_element_data_destroy (ExpectedElementData *exp_data)
{
	if (exp_data->attr1) g_free (exp_data->attr1);
	if (exp_data->attr2) g_free (exp_data->attr2);
	if (exp_data->bool_attr != NULL) g_free (exp_data->bool_attr);
	if (exp_data->text) g_free (exp_data->text);

	g_slice_free (ExpectedElementData, exp_data);
}

static void
check_expected_data (XesamGTestQueryBuilder *self,
					 XesamGQueryToken		token,
					 const gchar			*attr1,
					 const gchar			*attr2,
					 const gboolean			*bool_attr,
					 const gchar			*text)
{
	ExpectedElementData	*exp_data;

	g_return_if_fail (XESAM_IS_G_TEST_QUERY_BUILDER(self));

	if (!self->priv->expected_data) {
		/* No data to check */
		return;
	}

	exp_data = (ExpectedElementData*) self->priv->expected_data->data;

	g_assert_cmpstr (xesam_g_query_token_get_name (token), ==,
					 xesam_g_query_token_get_name (exp_data->token));

	g_assert_cmpstr (attr1, ==, exp_data->attr1);
	g_assert_cmpstr (attr2, ==, exp_data->attr2);
	g_assert_cmpstr (text, ==, exp_data->text);

	if (bool_attr != NULL && exp_data->bool_attr != NULL)
		g_assert_cmpuint (*bool_attr, ==, *exp_data->bool_attr);
	else if (bool_attr != NULL && !*bool_attr && exp_data->bool_attr == NULL)
		/* bool_attr is False and we expect it to be unset. This is ok. */;
	else if (!(bool_attr == NULL && exp_data->bool_attr == NULL))
		g_critical ("bool_attr and exp_data->bool_attr differ. %p != %p",
					bool_attr, exp_data->bool_attr);
	/* else : both bool_attr are NULL */


	self->priv->expected_data = g_slist_delete_link (self->priv->expected_data,
													 self->priv->expected_data);

	expected_element_data_destroy (exp_data);
}
