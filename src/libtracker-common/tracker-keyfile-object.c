/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-keyfile-object.h"

static GSList *
directory_string_list_to_gslist (const gchar **value)
{
	GSList *list = NULL;
	gint    i;

	if (!value) {
		return NULL;
	}

	for (i = 0; value[i]; i++) {
		const gchar *str;
		gchar       *validated;

		str = value[i];
		if (!str || str[0] == '\0') {
			continue;
		}

		/* For directories we validate any special characters,
		 * for example '~' and '../../'
		 */
		validated = tracker_path_evaluate_name (str);
		if (validated) {
			list = g_slist_prepend (list, validated);
		}
	}

	return g_slist_reverse (list);
}

const gchar *
tracker_keyfile_object_blurb (gpointer     object,
                              const gchar *property)
{
	GObjectClass *klass;
	GParamSpec *spec;

	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (property != NULL, NULL);

	klass = G_OBJECT_GET_CLASS (object);
	spec = g_object_class_find_property (G_OBJECT_CLASS (klass), property);
	g_return_val_if_fail (spec != NULL, NULL);

	return g_param_spec_get_blurb (spec);
}

gboolean
tracker_keyfile_object_default_boolean (gpointer     object,
                                        const gchar *property)
{
	GObjectClass *klass;
	GParamSpec *spec;
	GParamSpecBoolean *bspec;

	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (property != NULL, FALSE);

	klass = G_OBJECT_GET_CLASS (object);
	spec = g_object_class_find_property (G_OBJECT_CLASS (klass), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	bspec = G_PARAM_SPEC_BOOLEAN (spec);
	g_return_val_if_fail (bspec != NULL, FALSE);

	return bspec->default_value;
}

const gchar*
tracker_keyfile_object_default_string (gpointer     object,
                                       const gchar *property)
{
	GObjectClass *klass;
	GParamSpec *spec;
	GParamSpecString *bspec;

	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (property != NULL, FALSE);

	klass = G_OBJECT_GET_CLASS (object);
	spec = g_object_class_find_property (G_OBJECT_CLASS (klass), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	bspec = G_PARAM_SPEC_STRING (spec);
	g_return_val_if_fail (bspec != NULL, FALSE);

	return bspec->default_value;
}

gint
tracker_keyfile_object_default_int (gpointer     object,
                                    const gchar *property)
{
	GObjectClass *klass;
	GParamSpec *spec;
	GParamSpecInt *ispec;

	g_return_val_if_fail (G_IS_OBJECT (object), 0);
	g_return_val_if_fail (property != NULL, 0);

	klass = G_OBJECT_GET_CLASS (object);
	spec = g_object_class_find_property (G_OBJECT_CLASS (klass), property);
	g_return_val_if_fail (spec != NULL, 0);

	ispec = G_PARAM_SPEC_INT (spec);
	g_return_val_if_fail (ispec != NULL, 0);

	return ispec->default_value;
}

gboolean
tracker_keyfile_object_validate_int (gpointer     object,
                                     const gchar *property,
                                     gint         value)
{
#ifdef G_DISABLE_CHECKS
	GParamSpec *spec;
	GValue      gvalue = { 0 };
	gboolean    valid;
#endif

	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (property != NULL, FALSE);

#ifdef G_DISABLE_CHECKS
	spec = g_object_class_find_property (G_OBJECT_CLASS (object), property);
	g_return_val_if_fail (spec != NULL, FALSE);

	g_value_init (&gvalue, spec->value_type);
	g_value_set_int (&gvalue, value);
	valid = g_param_value_validate (spec, &gvalue);
	g_value_unset (&gvalue);

	g_return_val_if_fail (valid != TRUE, FALSE);
#endif

	return TRUE;
}

void
tracker_keyfile_object_load_int (gpointer     object,
                                 const gchar *property,
                                 GKeyFile    *key_file,
                                 const gchar *group,
                                 const gchar *key)
{
	GError *error = NULL;
	gint    value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	value = g_key_file_get_integer (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (object), property, value, NULL);
	} else {
		g_message ("Couldn't load object property '%s' (int) in group '%s', %s",
		           property, group, error->message);
		g_error_free (error);
	}
}

void
tracker_keyfile_object_load_boolean (gpointer     object,
                                     const gchar *property,
                                     GKeyFile    *key_file,
                                     const gchar *group,
                                     const gchar *key)
{
	GError   *error = NULL;
	gboolean  value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	value = g_key_file_get_boolean (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (object), property, value, NULL);
	} else {
		g_message ("Couldn't load object property '%s' (bool) in group '%s', %s",
		           property, group, error->message);
		g_error_free (error);
	}
}

void
tracker_keyfile_object_load_string (gpointer     object,
                                    const gchar *property,
                                    GKeyFile    *key_file,
                                    const gchar *group,
                                    const gchar *key)
{
	GError *error = NULL;
	gchar  *value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	value = g_key_file_get_string (key_file, group, key, &error);
	if (!error) {
		g_object_set (G_OBJECT (object), property, value, NULL);
	} else {
		g_message ("Couldn't load object property '%s' (string) in group '%s', %s",
		           property, group, error->message);
		g_error_free (error);
	}

	g_free (value);
}

void
tracker_keyfile_object_load_string_list (gpointer      object,
                                         const gchar  *property,
                                         GKeyFile     *key_file,
                                         const gchar  *group,
                                         const gchar  *key,
                                         GSList      **return_instead)
{
	GSList *l;
	gchar **value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	value = g_key_file_get_string_list (key_file, group, key, NULL, NULL);
	l = tracker_string_list_to_gslist (value, -1);
	g_strfreev (value);

	if (G_LIKELY (!return_instead)) {
		g_object_set (G_OBJECT (object), property, l, NULL);

		/* List is copied internally */
		g_slist_foreach (l, (GFunc) g_free, NULL);
		g_slist_free (l);
	} else {
		*return_instead = l;
	}
}

void
tracker_keyfile_object_load_directory_list (gpointer      object,
                                            const gchar  *property,
                                            GKeyFile     *key_file,
                                            const gchar  *group,
                                            const gchar  *key,
                                            gboolean      is_recursive,
                                            GSList      **return_instead)
{
	GSList *l;
	gchar **value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	value = g_key_file_get_string_list (key_file, group, key, NULL, NULL);
	l = directory_string_list_to_gslist ((const gchar **) value);
	g_strfreev (value);

	if (l) {
		GSList *filtered;

		/* Should we make the basename (2nd argument) here
		 * part of this function's API?
		 */
		filtered = tracker_path_list_filter_duplicates (l, ".", is_recursive);

		g_slist_foreach (l, (GFunc) g_free, NULL);
		g_slist_free (l);

		l = filtered;
	}

	if (G_LIKELY (!return_instead)) {
		g_object_set (G_OBJECT (object), property, l, NULL);

		/* List is copied internally */
		g_slist_foreach (l, (GFunc) g_free, NULL);
		g_slist_free (l);
	} else {
		*return_instead = l;
	}
}

void
tracker_keyfile_object_save_int (gpointer     object,
                                 const gchar *property,
                                 GKeyFile    *key_file,
                                 const gchar *group,
                                 const gchar *key)
{
	gint value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	g_object_get (G_OBJECT (object), property, &value, NULL);
	g_key_file_set_integer (key_file, group, key, value);
}

void
tracker_keyfile_object_save_boolean (gpointer     object,
                                     const gchar *property,
                                     GKeyFile    *key_file,
                                     const gchar *group,
                                     const gchar *key)
{
	gboolean value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	g_object_get (G_OBJECT (object), property, &value, NULL);
	g_key_file_set_boolean (key_file, group, key, value);
}

void
tracker_keyfile_object_save_string (gpointer     object,
                                    const gchar *property,
                                    GKeyFile    *key_file,
                                    const gchar *group,
                                    const gchar         *key)
{
	gchar *value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	g_object_get (G_OBJECT (object), property, &value, NULL);
	g_key_file_set_string (key_file, group, key, value);
	g_free (value);
}

void
tracker_keyfile_object_save_string_list (gpointer     object,
                                         const gchar *property,
                                         GKeyFile    *key_file,
                                         const gchar *group,
                                         const gchar *key)
{
	GSList *list;
	gchar **value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	g_object_get (G_OBJECT (object), property, &list, NULL);

	value = tracker_gslist_to_string_list (list);
	g_key_file_set_string_list (key_file,
	                            group,
	                            key,
	                            (const gchar * const *) value,
	                            (gsize) g_slist_length (list));
	g_strfreev (value);
}

void
tracker_keyfile_object_save_directory_list (gpointer     object,
                                            const gchar *property,
                                            GKeyFile    *key_file,
                                            const gchar *group,
                                            const gchar *key)
{
	GSList *list;
	gchar **value;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property != NULL);
	g_return_if_fail (key_file != NULL);
	g_return_if_fail (group != NULL);
	g_return_if_fail (key != NULL);

	g_object_get (G_OBJECT (object), property, &list, NULL);

	value = tracker_gslist_to_string_list (list);
	g_key_file_set_string_list (key_file,
	                            group,
	                            key,
	                            (const gchar * const *) value,
	                            (gsize) g_slist_length (list));
	g_strfreev (value);
}
