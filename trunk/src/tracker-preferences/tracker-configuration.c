/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "tracker-configuration.h"
#include "tracker-configuration-private.h"
#include "config.h"


#ifndef HAVE_RECENT_GLIB
/**********************************************************************
 *
 * The following functions are copied from the GLIB 2.12
 * source code, to lower requirement on glib to 2.10 that ships with 
 * Dapper
 *
 **********************************************************************/

static gchar *
_g_utf8_make_valid (const gchar *name)
{
  GString *string;
  const gchar *remainder, *invalid;
  gint remaining_bytes, valid_bytes;
  
  string = NULL;
  remainder = name;
  remaining_bytes = strlen (name);
  
  while (remaining_bytes != 0) 
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid)) 
	break;
      valid_bytes = invalid - remainder;
    
      if (string == NULL) 
	string = g_string_sized_new (remaining_bytes);

      g_string_append_len (string, remainder, valid_bytes);
      /* append U+FFFD REPLACEMENT CHARACTER */
      g_string_append (string, "\357\277\275");
      
      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }
  
  if (string == NULL)
    return g_strdup (name);
  
  g_string_append (string, remainder);

  g_assert (g_utf8_validate (string->str, -1, NULL));
  
  return g_string_free (string, FALSE);
}

static gdouble
g_key_file_parse_value_as_double  (GKeyFile     *key_file,
                                   const gchar  *value,
                                   GError      **error)
{
  gchar *end_of_valid_d;
  gdouble double_value = 0;

  double_value = g_ascii_strtod (value, &end_of_valid_d);

  if (*end_of_valid_d != '\0' || end_of_valid_d == value)
    {
      gchar *value_utf8 = _g_utf8_make_valid (value);
      g_set_error (error, G_KEY_FILE_ERROR,
		   G_KEY_FILE_ERROR_INVALID_VALUE,
		   ("Value '%s' cannot be interpreted "
		     "as a float number."), 
		   value_utf8);
      g_free (value_utf8);
    }

  return double_value;
}

gdouble
g_key_file_get_double (GKeyFile *key_file, const gchar *group_name,
                       const gchar *key, GError **error)
{
  GError *key_file_error;
  gchar *value;
  gdouble double_value;

  g_return_val_if_fail (key_file != NULL, -1);
  g_return_val_if_fail (group_name != NULL, -1);
  g_return_val_if_fail (key != NULL, -1);

  key_file_error = NULL;

  value = g_key_file_get_value (key_file, group_name, key, &key_file_error);

  if (key_file_error)
    {
      g_propagate_error (error, key_file_error);
      return 0;
    }

  double_value = g_key_file_parse_value_as_double (key_file, value,
                                                  &key_file_error);
  g_free (value);

  if (key_file_error)
    {
      if (g_error_matches (key_file_error,
                           G_KEY_FILE_ERROR,
                           G_KEY_FILE_ERROR_INVALID_VALUE))
        {
          g_set_error (error, G_KEY_FILE_ERROR,
                       G_KEY_FILE_ERROR_INVALID_VALUE,
                       ("Key file contains key '%s' in group '%s' "
                         "which has a value that cannot be interpreted."), key,
                       group_name);
          g_error_free (key_file_error);
        }
      else
        g_propagate_error (error, key_file_error);
    }

  return double_value;
}

void
g_key_file_set_double  (GKeyFile    *key_file,
                        const gchar *group_name,
                        const gchar *key,
                        gdouble      value)
{
  gchar result[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_if_fail (key_file != NULL);

  g_ascii_dtostr (result, sizeof (result), value);
  g_key_file_set_value (key_file, group_name, key, result);
}

gdouble*
g_key_file_get_double_list (GKeyFile *key_file,
                            const gchar *group_name,
                            const gchar *key,
                            gsize *length,
                            GError **error)
{
  GError *key_file_error = NULL;
  gchar **values;
  gdouble *double_values;
  gsize i, num_doubles;

  g_return_val_if_fail (key_file != NULL, NULL);
  g_return_val_if_fail (group_name != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  values = g_key_file_get_string_list (key_file, group_name, key,
                                       &num_doubles, &key_file_error);

  if (key_file_error)
    g_propagate_error (error, key_file_error);

  if (!values)
    return NULL;

  double_values = g_new0 (gdouble, num_doubles);

  for (i = 0; i < num_doubles; i++)
    {
      double_values[i] = g_key_file_parse_value_as_double (key_file,
							   values[i],
							   &key_file_error);

      if (key_file_error)
        {
          g_propagate_error (error, key_file_error);
          g_strfreev (values);
          g_free (double_values);

          return NULL;
        }
    }
  g_strfreev (values);

  if (length)
    *length = num_doubles;

  return double_values;
}

void
g_key_file_set_double_list (GKeyFile     *key_file,
			    const gchar  *group_name,
			    const gchar  *key,
			    gdouble       list[],
			    gsize         length)
{
  GString *values;
  gsize i;

  g_return_if_fail (key_file != NULL);
  g_return_if_fail (list != NULL);

  values = g_string_sized_new (length * 16);
  for (i = 0; i < length; i++)
    {
      gchar result[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_dtostr( result, sizeof (result), list[i] );

      g_string_append (values, result);
      g_string_append_c (values, ';');
    }

  g_key_file_set_value (key_file, group_name, key, values->str);
  g_string_free (values, TRUE);
}


/**********************************************************************
 *
 *                     End of copied functions.
 *
 **********************************************************************/
#endif /* !HAVE_RECENT_GLIB */


Matches tmap[] = {
		{"da", "Danish"},
		{"nl", "Dutch"},
		{"en", "English"},
 		{"fi", "Finnish"},
		{"fr", "French"},
		{"de", "German"},
		{"it", "Italian"},
		{"nb", "Norwegian"},
		{"pt", "Portuguese"},
		{"ru", "Russian"},
		{"es", "Spanish"},
		{"sv", "Swedish"},
		{NULL, NULL}
};

static GObjectClass *parent_class = NULL;

static void
tracker_configuration_class_init (TrackerConfigurationClass * klass)
{
	GObjectClass *g_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (klass,
				  sizeof (TrackerConfigurationPrivate));

	g_class->finalize = tracker_configuration_finalize;

	/* Methods */
	klass->write = _write;

	klass->get_bool = _get_bool;
	klass->set_bool = _set_bool;

	klass->get_int = _get_int;
	klass->set_int = _set_int;

	klass->get_string = _get_string;
	klass->set_string = _set_string;

	klass->get_list = _get_list;
	klass->set_list = _set_list;

	/* Properties */
}

static gchar *
get_default_language_code (void)
{
	gchar **langs, **plangs;

	/* get langauges for user's locale */
	langs = (char**) g_get_language_names ();

	for (plangs = langs; *plangs; plangs++) {
		if (strlen (*plangs) > 1) {
                        gint i;
			for (i = 0; tmap[i].lang; i++) {
				if (g_str_has_prefix (*plangs, tmap[i].lang)) {
					return g_strndup (*plangs, 2);
				}
			}
		}
	}

	return g_strdup ("en");
}

static void
create_config_file ()
{
	gchar	 *filename;

	filename = g_build_filename (g_get_user_config_dir (), "/tracker/tracker.cfg", NULL);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
                gchar *tracker_dir = g_build_filename (g_get_user_config_dir (),"/tracker",NULL);

                if (!g_file_test (tracker_dir, G_FILE_TEST_EXISTS)) {
                        g_mkdir_with_parents (tracker_dir, 0700);
                }

		g_free (tracker_dir);

		gchar *contents, *language;

		language = get_default_language_code ();

		contents  = g_strconcat (
					 "[General]\n",
					 "# Log Verbosity - Valid values are 0 (displays/logs only errors), 1 (minimal), 2 (detailed), and 3 (debug)\n",
					 "Verbosity=0\n\n",
					 "# Minimizes the use of memory but may slow indexing down\n", 
					 "LowMemoryMode=false\n\n",
					 "# Set the initial sleeping time, in seconds\n",
					 "InitialSleep=60\n",
					 "[Watches]\n",
					 "# List of directory roots to index and watch seperated by semicolons\n",
					 "WatchDirectoryRoots=", g_get_home_dir (), ";\n",
					 "# List of directory roots to index but not watch (no live updates but are refreshed when trackerd is next restarted) seperated by semicolons\n",
					 "CrawlDirectory=\n",
					 "# List of directory roots to not index and not watch seperated by semicolons\n",
					 "NoWatchDirectory=\n",
					 "# Set to false to prevent watching of any kind\n",
					 "EnableWatching=true\n\n",
					 "[Indexing]\n",
					 "# Throttles the indexing process. Allowable values are 0-20. higher values decrease indexing speed\n",
					 "Throttle=0\n",
					 "# Disables the indexing process\n",
					 "EnableIndexing=true\n",
					 "# Enables indexing of a file's text contents\n",
					 "EnableFileContentIndexing=true\n",
					 "# Enables generation of thumbnails\n",
					 "EnableThumbnails=false\n",
					 "# List of partial file patterns (glob) seperated by semicolons that specify files to not index (basic stat info is only indexed for files that match these patterns)\n",
					 "NoIndexFileTypes=;\n\n",
					  "# Sets minimum length of words to index\n",
					 "MinWordLength=3\n",
					  "# Sets maximum length of words to index (words are cropped if bigger than this)\n",
					 "MaxWordLength=30\n",
					  "# Sets the language specific stemmer and stopword list to use \n",
					  "# Valid values are 'en' (english), 'da' (danish), 'nl' (dutch), 'fi' (finnish), 'fr' (french), 'de' (german), 'it' (italien), 'nb' (norwegian), 'pt' (portugese), 'ru' (russian), 'es' (spanish), 'sv' (swedish)\n",
					 "Language=", language, "\n",
					 "# Enables use of language-specific stemmer\n",
					 "EnableStemmer=true\n",
					 "[Emails]\n",
					 "IndexEvolutionEmails=true\n",
					 "[Performance]\n",
					 "# Maximum size of text in bytes to index from a file's text contents\n",
					 "MaxTextToIndex=1048576\n",
					 "# Maximum number of unique words to index from a file's text contents\n",
					 "MaxWordsToIndex=10000\n",
					 "# Specifies the no of entities to index before determining whether to perform index optimization\n",
					 "OptimizationSweepCount=10000\n",
					 "# Sets the maximum bucket count for the indexer\n",
					 "MaxBucketCount=524288\n",
					 "# Sets the minimum bucket count\n",
					 "MinBucketCount=65536\n",
					 "# Sets no. of divisions of the index file\n",
					 "Dvisions=4\n",
					 "# Selects the desired ratio of used records to buckets to be used when optimizing index (should be a value between 0 and 4) \n",
					 "BucketRatio=1\n",
					 "# Alters how much padding is used to prevent index relocations. Higher values improve indexing speed but waste more disk space. Value should be in range (1..8)\n",
					 "Padding=2\n",
					 NULL);

		g_file_set_contents (filename, contents, strlen (contents), NULL);
		g_free (contents);
	}

	g_free (filename);
}

static void
tracker_configuration_init (GTypeInstance * instance, gpointer data)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (instance);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	GError *error = NULL;

	priv->dirty = FALSE;
	priv->filename = g_build_filename (g_strdup (g_get_user_config_dir ()), "/tracker/tracker.cfg", NULL);
	priv->keyfile = g_key_file_new ();

	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
		create_config_file ();
	}
	

	g_key_file_load_from_file (priv->keyfile, priv->filename,
				   G_KEY_FILE_KEEP_COMMENTS, &error);

	if (error)
		g_error ("failed: g_key_file_load_from_file(): %s\n",
			 error->message);
}

static void
tracker_configuration_finalize (GObject * object)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (object);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	if (priv->dirty)
		_write (self);

	g_free (priv->filename);
	g_key_file_free (priv->keyfile);
}

TrackerConfiguration *
tracker_configuration_new (void)
{
	TrackerConfiguration *config;
	config = g_object_new (TRACKER_TYPE_CONFIGURATION, NULL);
	return TRACKER_CONFIGURATION (config);
}

/* Class Hooks */
void
tracker_configuration_write (TrackerConfiguration * configuration)
{
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		write (configuration);
}

#define MAKE_TRACKER_CONFIGURATION_GET_ATYPE_FCT(TypeName, Type)                \
  Type                                                                          \
  tracker_configuration_get_##TypeName (TrackerConfiguration * configuration,   \
                                        const gchar * const key,                \
                                        GError ** error)                        \
  {                                                                             \
	return TRACKER_CONFIGURATION_GET_CLASS (configuration)->                \
                get_##TypeName (configuration, key, error);                     \
  }

#define MAKE_TRACKER_CONFIGURATION_SET_ATYPE_FCT(TypeName, Type)                \
  void                                                                          \
  tracker_configuration_set_##TypeName (TrackerConfiguration * configuration,   \
                                        const gchar * const key,                \
                                        const Type value)                       \
  {                                                                             \
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->                       \
		set_##TypeName (configuration, key, value);                     \
  }

MAKE_TRACKER_CONFIGURATION_GET_ATYPE_FCT (bool, gboolean)
MAKE_TRACKER_CONFIGURATION_SET_ATYPE_FCT (bool, gboolean)

MAKE_TRACKER_CONFIGURATION_GET_ATYPE_FCT (int, gint)
MAKE_TRACKER_CONFIGURATION_SET_ATYPE_FCT (int, gint)

MAKE_TRACKER_CONFIGURATION_GET_ATYPE_FCT (string, gchar*)
MAKE_TRACKER_CONFIGURATION_SET_ATYPE_FCT (string, gchar*)

#undef MAKE_TRACKER_CONFIGURATION_GET_ATYPE_FCT
#undef MAKE_TRACKER_CONFIGURATION_SET_ATYPE_FCT

GSList *
tracker_configuration_get_list (TrackerConfiguration * configuration,
				const gchar * const key, GType type,
				GError ** error)
{
	return TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		get_list (configuration, key, type, error);
}

void
tracker_configuration_set_list (TrackerConfiguration * configuration,
				const gchar * const key,
				const GSList * const value, GType type)
{
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		set_list (configuration, key, value, type);
}

static gchar *
string_replace (const gchar *haystack, gchar *needle, gchar *replacement)
{
        GString *str;
        gint pos, needle_len;

	g_return_val_if_fail (haystack && needle, NULL);

	needle_len = strlen (needle);

        str = g_string_new ("");

        for (pos = 0; haystack[pos]; pos++) {
                if (strncmp (&haystack[pos], needle, needle_len) == 0) {
			if (replacement) {
	                        str = g_string_append (str, replacement);
			}

                        pos += needle_len - 1;

                } else {
                        str = g_string_append_c (str, haystack[pos]);
		}
        }

        return g_string_free (str, FALSE);
}

static void
_write (TrackerConfiguration * configuration)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	if (!priv->dirty) {
		return;
        }

	gsize length = 0;
	GError *error = NULL;
	gchar *contents = NULL;

/*
	char *my_contents = g_key_file_to_data (priv->keyfile, &length, &error);

	char **array = g_strsplit (my_contents, "\n", 0);

	g_free (my_contents);

	GString *gstr = g_string_new ("");

	char **array2;


	for (array2=array; *array2; array2++) {
		if (*array2[0] != '\0') {
			gstr = g_string_append (gstr, *array2);
			g_string_append_c (gstr, '\n');
		}
	}

	g_strfreev (array);

	contents = g_string_free (gstr, FALSE);
*/
	char *my_contents = g_key_file_to_data (priv->keyfile, &length, &error);

	if (error) {
		g_error ("failed: g_key_file_to_data(): %s\n", error->message);
        }

	contents = string_replace (my_contents, "\n\n\n", "\n\n");

	g_free (my_contents);


	g_file_set_contents (priv->filename, contents, -1, NULL);

	g_free (contents);
	priv->dirty = FALSE;
}

#define MAKE_GET_ATYPE_FCT(TypeName, Type, Type_g_key_file_fct)                 \
  static Type                                                                   \
  _get_##TypeName (TrackerConfiguration * configuration,                        \
                   const gchar * const key,                                     \
                   GError ** error)                                             \
  {                                                                             \
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);     \
	TrackerConfigurationPrivate *priv =                                     \
		TRACKER_CONFIGURATION_GET_PRIVATE (self);                       \
                                                                                \
	gchar **data = g_strsplit (key, "/", 3);                                \
                                                                                \
        if (g_key_file_has_key (priv->keyfile, data[1], data[2], error)) {      \
                Type value;                                                     \
                value = g_key_file_get_##Type_g_key_file_fct (priv->keyfile,    \
                                                              data[1],          \
                                                              data[2],          \
                                                              error);           \
                                                                                \
                g_strfreev (data);                                              \
                                                                                \
                return value;                                                   \
        } else {                                                                \
                return FALSE;                                                   \
        }                                                                       \
  }

#define MAKE_SET_ATYPE(TypeName, Type, Type_g_key_file_fct)                     \
  static void                                                                   \
  _set_##TypeName (TrackerConfiguration * configuration,                        \
                   const gchar * const key,                                     \
                   const Type value)                                            \
  {                                                                             \
        TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);     \
        TrackerConfigurationPrivate *priv =                                     \
                TRACKER_CONFIGURATION_GET_PRIVATE (self);                       \
                                                                                \
        gchar **data = g_strsplit (key, "/", 3);                                \
        g_key_file_set_##Type_g_key_file_fct (priv->keyfile, data[1],           \
                                              data[2], value);                  \
                                                                                \
        g_strfreev (data);                                                      \
        priv->dirty = TRUE;                                                     \
  }

MAKE_GET_ATYPE_FCT (bool, gboolean, boolean)
MAKE_SET_ATYPE (bool, gboolean, boolean)

MAKE_GET_ATYPE_FCT (int, int, integer)
MAKE_SET_ATYPE (int, int, integer)

MAKE_GET_ATYPE_FCT (string, gchar*, string)

#undef MAKE_GET_ATYPE_FCT
#undef MAKE_SET_ATYPE_FCT

static void
_set_string (TrackerConfiguration * configuration, const gchar * const key,
	     const gchar * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar **data = g_strsplit (key, "/", 3);
	g_key_file_set_string (priv->keyfile, data[1], data[2], value);
	g_strfreev (data);

	priv->dirty = TRUE;
}

static GSList *
_get_list (TrackerConfiguration * configuration, const gchar * const key,
	   GType type, GError ** error)
{
	switch (type) {
	case G_TYPE_BOOLEAN:
		return _get_boolean_list (configuration, key, error);
		break;
	case G_TYPE_DOUBLE:
		return _get_double_list (configuration, key, error);
		break;
	case G_TYPE_INT:
		return _get_int_list (configuration, key, error);
		break;
	case G_TYPE_STRING:
		return _get_string_list (configuration, key, error);
		break;
	default:
		g_error ("Invalid list type\n");
		break;
	}

	return NULL;
}

#define MAKE_GET_ATYPE_LIST_FCT(TypeName, Type, Type_g_key_file_fct)            \
  static GSList *                                                               \
  _get_##TypeName##_list (TrackerConfiguration * configuration,                 \
                          const gchar * const key,                              \
                          GError ** error)                                      \
  {                                                                             \
        TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);     \
	TrackerConfigurationPrivate *priv =                                     \
		TRACKER_CONFIGURATION_GET_PRIVATE (self);                       \
                                                                                \
        gchar **data = g_strsplit (key, "/", 3);                                \
	GSList *retval = NULL;                                                  \
                                                                                \
	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error)) {      \
                gsize length = 0;                                               \
                Type *values;                                                   \
                                                                                \
		values =                                                        \
                  g_key_file_get_##Type_g_key_file_fct##_list (priv->keyfile,   \
                                                               data[1],         \
                                                               data[2],         \
                                                               &length,         \
                                                               error);          \
                if (values) {                                                   \
                        gsize i;                                                \
                        for (i = 0; i < length; i++) {                          \
                                Type *value = g_new0 (Type, 1);                 \
                                *value = values[i];                             \
                                retval = g_slist_prepend (retval, value);       \
                        }                                                       \
                }                                                               \
                                                                                \
                g_strfreev (data);                                              \
                g_free (values);                                                \
        }                                                                       \
                                                                                \
        return g_slist_reverse (retval);                                        \
  }

MAKE_GET_ATYPE_LIST_FCT (boolean, gboolean, boolean)
MAKE_GET_ATYPE_LIST_FCT (double, gdouble, double)
MAKE_GET_ATYPE_LIST_FCT (int, gint, integer)

#undef MAKE_GET_ATYPE_LIST_FCT

static GSList *
_get_string_list (TrackerConfiguration * configuration,
                  const gchar * const key,
                  GError ** error)
{
        TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
        TrackerConfigurationPrivate *priv = TRACKER_CONFIGURATION_GET_PRIVATE (self);

        gchar **data = g_strsplit (key, "/", 3);
        GSList *retval = NULL;

        if (g_key_file_has_key (priv->keyfile, data[1], data[2], error)) {
                gsize length = 0;
                gchar **values;

                values = g_key_file_get_string_list (priv->keyfile,
                                                     data[1],
                                                     data[2],
                                                     &length,
                                                     error);
                if (values) {
                        gsize i;
                        for (i = 0; i < length; i++) {
                                gchar *value = g_strdup (values[i]);
                                retval = g_slist_prepend (retval, value);
                        }
                }

                g_strfreev (data);
                g_free (values);
        }

        return g_slist_reverse (retval);
}

static void
_set_list (TrackerConfiguration * configuration, const gchar * const key,
	   const GSList * const value, GType type)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	switch (type) {
	case G_TYPE_BOOLEAN:
		_set_boolean_list (configuration, key, value);
		break;
	case G_TYPE_DOUBLE:
		_set_double_list (configuration, key, value);
		break;
	case G_TYPE_INT:
		_set_int_list (configuration, key, value);
		break;
	case G_TYPE_STRING:
		_set_string_list (configuration, key, value);
		break;
	default:
		g_error ("Invalid list type\n");
		break;
	}

	priv->dirty = TRUE;
}

#define MAKE_SET_ATYPE_LIST_FCT(TypeName, Type, Type_g_key_file_fct)            \
  static void                                                                   \
  _set_##TypeName##_list (TrackerConfiguration * configuration,                 \
                          const gchar * const key,                              \
                          const GSList * const value)                           \
  {                                                                             \
        TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);     \
	TrackerConfigurationPrivate *priv =                                     \
		TRACKER_CONFIGURATION_GET_PRIVATE (self);                       \
                                                                                \
	gchar **data = g_strsplit (key, "/", 3);                                \
	guint length = g_slist_length ((GSList *) value);                       \
	Type *list = g_new0 (Type, length);                                     \
                                                                                \
	guint i;                                                                \
        const GSList *tmp;                                                      \
        for (i = 0, tmp = value; tmp; tmp = tmp->next, i++) {                   \
                if (tmp->data) {                                                \
                        Type *n = tmp->data;                                    \
                        list[i] = *n;                                           \
                }                                                               \
        }                                                                       \
                                                                                \
	g_key_file_set_##Type_g_key_file_fct##_list (priv->keyfile,             \
                                                     data[1], data[2],          \
                                                     list, length);             \
                                                                                \
	g_strfreev (data);                                                      \
	g_free (list);                                                          \
  }

MAKE_SET_ATYPE_LIST_FCT (boolean, gboolean, boolean)
MAKE_SET_ATYPE_LIST_FCT (double, gdouble, double)
MAKE_SET_ATYPE_LIST_FCT (int, gint, integer)

#undef MAKE_SET_ATYPE_LIST_FCT

static void
_set_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar **data = g_strsplit (key, "/", 3);
	guint length = g_slist_length ((GSList *) value);
        gchar **list = g_new0 (gchar *, length + 1);

        guint i;
        const GSList *tmp;
        for (i = 0, tmp = value; tmp; tmp = tmp->next, i++) {
                if (tmp->data) {
                        gchar *value = g_strdup (tmp->data);
                        list[i] = value;
                }
        }

        g_key_file_set_string_list (priv->keyfile, data[1], data[2],
                                    (const gchar **) list, length);

        g_strfreev (data);
        g_strfreev (list);
}

GType
tracker_configuration_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TrackerConfigurationClass),
			NULL,	/* bsse_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) tracker_configuration_class_init,	/* class_init */
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (TrackerConfiguration),
			0,	/* n_preallocs */
			tracker_configuration_init	/* instance_init */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "TrackerConfigurationType",
					       &info, 0);
	}

	return type;
}
