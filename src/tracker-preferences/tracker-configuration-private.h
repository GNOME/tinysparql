#ifndef __TRACKER_CONFIGURATION_PRIVATE_H__
#define __TRACKER_CONFIGURATION_PRIVATE_H__

#define TRACKER_CONFIGURATION_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_CONFIGURATION, TrackerConfigurationPrivate))

typedef struct _TrackerConfigurationPrivate {
	gboolean dirty;
	gchar *filename;
	GKeyFile *keyfile;
} TrackerConfigurationPrivate;

static void
tracker_configuration_class_init (TrackerConfigurationClass * klass);

static void
tracker_configuration_init (GTypeInstance * instance, gpointer data);

static void
tracker_configuration_finalize (GObject * object);

static void
_write (TrackerConfiguration * configuration);

static gboolean
_get_bool (TrackerConfiguration * configuration, const gchar * const key,
	   GError ** error);

static void
_set_bool (TrackerConfiguration * configuration, const gchar * const key,
	   const gboolean value);

static gint
_get_int (TrackerConfiguration * configuration, const gchar * const key,
	  GError ** error);

static void
_set_int (TrackerConfiguration * configuration, const gchar * const key,
	  const gint value);

static gchar *
_get_string (TrackerConfiguration * configuration,
	     const gchar * const key, GError ** error);

static void
_set_string (TrackerConfiguration * configuration, const gchar * const key,
	     const gchar * const value);

static GSList *
_get_list (TrackerConfiguration * configuration,
	   const gchar * const key, GType type,
	   GError ** error);

static void
_set_list (TrackerConfiguration * configuration, const gchar * const key,
	   const GSList * const value, GType type);

static GSList *
_get_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, GError ** error);

static GSList *
_get_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error);

static GSList *
_get_int_list (TrackerConfiguration * configuration,
	       const gchar * const key, GError ** error);

static GSList *
_get_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error);

static void
_set_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, const GSList * const value);

static void
_set_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value);

static void
_set_int_list (TrackerConfiguration * configuration, const gchar * const key,
	       const GSList * const value);

static void
_set_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value);

#endif
