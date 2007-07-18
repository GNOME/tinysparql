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

	if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS))
		g_error ("tracker_configuration_init: implement file defaults\n");

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

gboolean
tracker_configuration_get_bool (TrackerConfiguration * configuration,
				const gchar * const key, GError ** error)
{
	return TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		get_bool (configuration, key, error);
}

void
tracker_configuration_set_bool (TrackerConfiguration * configuration,
				const gchar * const key, const gboolean value)
{
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		set_bool (configuration, key, value);
}

gint
tracker_configuration_get_int (TrackerConfiguration * configuration,
			       const gchar * const key, GError ** error)
{
	return TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		get_int (configuration, key, error);
}

void
tracker_configuration_set_int (TrackerConfiguration * configuration,
			       const gchar * const key, const gint value)
{
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		set_int (configuration, key, value);
}

gchar *
tracker_configuration_get_string (TrackerConfiguration * configuration,
				  const gchar * const key, GError ** error)
{
	return TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		get_string (configuration, key, error);
}

void
tracker_configuration_set_string (TrackerConfiguration * configuration,
				  const gchar * const key,
				  const gchar * const value)
{
	TRACKER_CONFIGURATION_GET_CLASS (configuration)->
		set_string (configuration, key, value);
}

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

static void
_write (TrackerConfiguration * configuration)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	if (!priv->dirty)
		return;

	gsize length = 0;
	GError *error = NULL;
	gchar *contents = NULL;

	contents = g_key_file_to_data (priv->keyfile, &length, &error);

	if (error)
		g_error ("failed: g_key_file_to_data(): %s\n",
			 error->message);

	g_file_set_contents (priv->filename, contents, length, NULL);

	g_free (contents);
	priv->dirty = FALSE;
}

static gboolean
_get_bool (TrackerConfiguration * configuration, const gchar * const key,
	   GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gboolean value = FALSE;
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		value = g_key_file_get_boolean (priv->keyfile, data[1],
						data[2], error);

	g_strfreev (data);
	return value;
}

static void
_set_bool (TrackerConfiguration * configuration, const gchar * const key,
	   const gboolean value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar **data = g_strsplit (key, "/", 3);
	g_key_file_set_boolean (priv->keyfile, data[1], data[2], value);
	g_strfreev (data);

	priv->dirty = TRUE;
}

static gint
_get_int (TrackerConfiguration * configuration, const gchar * const key,
	  GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gint value = 0;
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		value = g_key_file_get_integer (priv->keyfile, data[1],
						data[2], error);

	g_strfreev (data);
	return value;
}

static void
_set_int (TrackerConfiguration * configuration, const gchar * const key,
	  const gint value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar **data = g_strsplit (key, "/", 3);
	g_key_file_set_integer(priv->keyfile, data[1], data[2], value);
	g_strfreev (data);

	priv->dirty = TRUE;
}

static gchar *
_get_string (TrackerConfiguration * configuration, const gchar * const key,
	     GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar *value = NULL;
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		value = g_key_file_get_string (priv->keyfile, data[1],
					       data[2], error);

	g_strfreev (data);
	return value;
}

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

static GSList *
_get_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	gsize length = 0;
	gboolean *values = NULL;
	GSList *retval = g_slist_alloc ();
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		values = g_key_file_get_boolean_list (priv->keyfile, data[1],
						      data[2], &length,
						      error);

	if (values)
		for (; i < length; ++i) {
			gboolean *value = g_new0 (gboolean, 1);
			*value = values[i];

			retval = g_slist_insert (retval, value, -1);
		}

	g_strfreev (data);
	g_free (values);

	return retval;
}

static GSList *
_get_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	gsize length = 0;
	gdouble *values = NULL;
	GSList *retval = g_slist_alloc ();
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		values = g_key_file_get_double_list (priv->keyfile, data[1],
						     data[2], &length, error);

	if (values)
		for (; i < length; ++i) {
			gdouble *value = g_new0 (gdouble, 1);
			*value = values[i];

			retval = g_slist_insert (retval, value, -1);
		}

	g_strfreev (data);
	g_free (values);

	return retval;
}

static GSList *
_get_int_list (TrackerConfiguration * configuration, const gchar * const key,
	       GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	gsize length = 0;
	gint *values = NULL;
	GSList *retval = g_slist_alloc ();
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		values = g_key_file_get_integer_list (priv->keyfile, data[1],
						      data[2], &length,
						      error);

	if (values)
		for (; i < length; ++i) {
			gint *value = g_new0 (gint, 1);
			*value = values[i];

			retval = g_slist_insert (retval, value, -1);
		}

	g_strfreev (data);
	g_free (values);

	return retval;
}

static GSList *
_get_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	gsize length = 0;
	gchar **values = NULL;
	GSList *retval = g_slist_alloc ();
	gchar **data = g_strsplit (key, "/", 3);

	if (g_key_file_has_key (priv->keyfile, data[1], data[2], error))
		values = g_key_file_get_string_list (priv->keyfile, data[1],
						     data[2], &length, error);

	if (values)
		for (; i < length; ++i)
			retval = g_slist_insert (retval, g_strdup (values[i]),
						 -1);

	g_strfreev (data);
	g_strfreev (values);

	return retval;
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

static void
_set_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, const GSList * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	guint length = g_slist_length ((GSList *) value);
	gchar **data = g_strsplit (key, "/", 3);
	gboolean *list = g_new0 (gboolean, length);

	for (; i < length; ++i)
		list[i] =
			*(gboolean
			  *) (g_slist_nth_data ((GSList *) value, i));

	g_key_file_set_boolean_list (priv->keyfile, data[1], data[2], list,
				     length);

	g_strfreev (data);
	g_free (list);
}

static void
_set_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	guint length = g_slist_length ((GSList *) value);
	gchar **data = g_strsplit (key, "/", 3);
	gdouble *list = g_new0 (gdouble, length);

	for (; i < length; ++i)
		list[i] =
			*(gdouble *) (g_slist_nth_data ((GSList *) value, i));

	g_key_file_set_double_list (priv->keyfile, data[1], data[2], list,
				    length);

	g_strfreev (data);
	g_free (list);
}

static void
_set_int_list (TrackerConfiguration * configuration, const gchar * const key,
	       const GSList * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gsize i = 0;
	guint length = g_slist_length ((GSList *) value);
	gchar **data = g_strsplit (key, "/", 3);
	gint *list = g_new0 (gint, length);

	for (; i < length; ++i)
		list[i] = *(gint *) (g_slist_nth_data ((GSList *) value, i));

	g_key_file_set_integer_list (priv->keyfile, data[1], data[2], list,
				     length);

	g_strfreev (data);
	g_free (list);
}

static void
_set_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value)
{
	TrackerConfiguration *self = TRACKER_CONFIGURATION (configuration);
	TrackerConfigurationPrivate *priv =
		TRACKER_CONFIGURATION_GET_PRIVATE (self);

	gchar **data = g_strsplit (key, "/", 3);

	guint i = 0;
	guint length = g_slist_length ((GSList *) value);
	gchar **list = g_new0 (gchar *, length + 1);

	for (; i < length; i++)
		list[i] = g_strdup (g_slist_nth_data ((GSList *) value, i));

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
