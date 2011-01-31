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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <math.h>

#include <glib/gi18n.h>

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-marshal.h"
#include "tracker-miner-object.h"
#include "tracker-miner-dbus.h"

/* Here we use ceil() to eliminate decimal points beyond what we're
 * interested in, which is 2 decimal places for the progress. The
 * ceil() call will also round up the last decimal place.
 *
 * The 0.49 value is used for rounding correctness, because ceil()
 * rounds up if the number is > 0.0.
 */
#define PROGRESS_ROUNDED(x) (ceil (((x) * 100) - 0.49) / 100)

#define TRACKER_SERVICE "org.freedesktop.Tracker1"

/**
 * SECTION:tracker-miner
 * @short_description: Abstract base class for data miners
 * @include: libtracker-miner/tracker-miner.h
 *
 * #TrackerMiner is an abstract base class to help developing data miners
 * for tracker-store, being an abstract class it doesn't do much by itself,
 * but provides the basic signaling and operation control so the miners
 * implementing this class are properly recognized by Tracker, and can be
 * controlled properly by external means such as #TrackerMinerManager.
 **/

#define TRACKER_MINER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER, TrackerMinerPrivate))

static GQuark miner_error_quark = 0;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.freedesktop.Tracker1.Miner'>"
  "    <method name='GetStatus'>"
  "      <arg type='s' name='status' direction='out' />"
  "    </method>"
  "    <method name='GetProgress'>"
  "      <arg type='d' name='progress' direction='out' />"
  "    </method>"
  "    <method name='GetPauseDetails'>"
  "      <arg type='as' name='pause_applications' direction='out' />"
  "      <arg type='as' name='pause_reasons' direction='out' />"
  "    </method>"
  "    <method name='Pause'>"
  "      <arg type='s' name='application' direction='in' />"
  "      <arg type='s' name='reason' direction='in' />"
  "      <arg type='i' name='cookie' direction='out' />"
  "    </method>"
  "    <method name='Resume'>"
  "      <arg type='i' name='cookie' direction='in' />"
  "    </method>"
  "    <method name='IgnoreNextUpdate'>"
  "      <arg type='as' name='urls' direction='in' />"
  "    </method>"
  "    <signal name='Started' />"
  "    <signal name='Stopped' />"
  "    <signal name='Paused' />"
  "    <signal name='Resumed' />"
  "    <signal name='Progress'>"
  "      <arg type='s' name='status' />"
  "      <arg type='d' name='progress' />"
  "    </signal>"
  "  </interface>"
  "</node>";

struct _TrackerMinerPrivate {
	TrackerSparqlConnection *connection;
	GHashTable *pauses;
	gboolean started;
	gchar *name;
	gchar *status;
	gdouble progress;
	gint availability_cookie;
	GDBusConnection *d_connection;
	GDBusNodeInfo *introspection_data;
	guint watch_name_id;
	guint registration_id;
	gchar *full_name;
	gchar *full_path;
};

typedef struct {
	gint cookie;
	gchar *application;
	gchar *reason;
} PauseData;

enum {
	PROP_0,
	PROP_NAME,
	PROP_STATUS,
	PROP_PROGRESS
};

enum {
	STARTED,
	STOPPED,
	PAUSED,
	RESUMED,
	PROGRESS,
	IGNORE_NEXT_UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void       miner_set_property           (GObject                *object,
                                                guint                   param_id,
                                                const GValue           *value,
                                                GParamSpec             *pspec);
static void       miner_get_property           (GObject                *object,
                                                guint                   param_id,
                                                GValue                 *value,
                                                GParamSpec             *pspec);
static void       miner_finalize               (GObject                *object);
static void       miner_initable_iface_init    (GInitableIface         *iface);
static gboolean   miner_initable_init          (GInitable              *initable,
                                                GCancellable           *cancellable,
                                                GError                **error);
static void       pause_data_destroy           (gpointer                data);
static PauseData *pause_data_new               (const gchar            *application,
                                                const gchar            *reason);
static void       handle_method_call           (GDBusConnection        *connection,
                                                const gchar            *sender,
                                                const gchar            *object_path,
                                                const gchar            *interface_name,
                                                const gchar            *method_name,
                                                GVariant               *parameters,
                                                GDBusMethodInvocation  *invocation,
                                                gpointer                user_data);
static GVariant  *handle_get_property          (GDBusConnection        *connection,
                                                const gchar            *sender,
                                                const gchar            *object_path,
                                                const gchar            *interface_name,
                                                const gchar            *property_name,
                                                GError                **error,
                                                gpointer                user_data);
static gboolean   handle_set_property          (GDBusConnection        *connection,
                                                const gchar            *sender,
                                                const gchar            *object_path,
                                                const gchar            *interface_name,
                                                const gchar            *property_name,
                                                GVariant               *value,
                                                GError                **error,
                                                gpointer                user_data);
static void       on_tracker_store_appeared    (GDBusConnection        *connection,
                                                const gchar            *name,
                                                const gchar            *name_owner,
                                                gpointer                user_data);
static void       on_tracker_store_disappeared (GDBusConnection        *connection,
                                                const gchar            *name,
                                                gpointer                user_data);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TrackerMiner, tracker_miner, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         miner_initable_iface_init));

static void
tracker_miner_class_init (TrackerMinerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = miner_set_property;
	object_class->get_property = miner_get_property;
	object_class->finalize     = miner_finalize;

	/**
	 * TrackerMiner::started:
	 * @miner: the #TrackerMiner
	 *
	 * the ::started signal is emitted in the miner
	 * right after it has been started through
	 * tracker_miner_start().
	 **/
	signals[STARTED] =
		g_signal_new ("started",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, started),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	/**
	 * TrackerMiner::stopped:
	 * @miner: the #TrackerMiner
	 *
	 * the ::stopped signal is emitted in the miner
	 * right after it has been stopped through
	 * tracker_miner_stop().
	 **/
	signals[STOPPED] =
		g_signal_new ("stopped",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, stopped),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	/**
	 * TrackerMiner::paused:
	 * @miner: the #TrackerMiner
	 *
	 * the ::paused signal is emitted whenever
	 * there is any reason to pause, either
	 * internal (through tracker_miner_pause()) or
	 * external (through DBus, see #TrackerMinerManager).
	 **/
	signals[PAUSED] =
		g_signal_new ("paused",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, paused),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	/**
	 * TrackerMiner::resumed:
	 * @miner: the #TrackerMiner
	 *
	 * the ::resumed signal is emitted whenever
	 * all reasons to pause have disappeared, see
	 * tracker_miner_resume() and #TrackerMinerManager.
	 **/
	signals[RESUMED] =
		g_signal_new ("resumed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, resumed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	/**
	 * TrackerMiner::progress:
	 * @miner: the #TrackerMiner
	 * @status: miner status
	 * @progress: a #gdouble indicating miner progress, from 0 to 1.
	 *
	 * the ::progress signal will be emitted by TrackerMiner implementations
	 * to indicate progress about the data mining process. @status will
	 * contain a translated string with the current miner status and @progress
	 * will indicate how much has been processed so far.
	 **/
	signals[PROGRESS] =
		g_signal_new ("progress",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, progress),
		              NULL, NULL,
		              tracker_marshal_VOID__STRING_DOUBLE,
		              G_TYPE_NONE, 2,
		              G_TYPE_STRING,
		              G_TYPE_DOUBLE);

	/**
	 * TrackerMiner::ignore-next-update:
	 * @miner: the #TrackerMiner
	 * @urls: the urls to mark as ignore on next update
	 *
	 * the ::ignore-next-update signal is emitted in the miner
	 * right after it has been asked to mark @urls as to ignore on next update
	 * through tracker_miner_ignore_next_update().
	 **/
	signals[IGNORE_NEXT_UPDATE] =
		g_signal_new ("ignore-next-update",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (TrackerMinerClass, ignore_next_update),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOXED,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRV);

	g_object_class_install_property (object_class,
	                                 PROP_NAME,
	                                 g_param_spec_string ("name",
	                                                      "Miner name",
	                                                      "Miner name",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_STATUS,
	                                 g_param_spec_string ("status",
	                                                      "Status",
	                                                      "Translatable string with status description",
	                                                      NULL,
	                                                      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_PROGRESS,
	                                 g_param_spec_double ("progress",
	                                                      "Progress",
	                                                      "Miner progress",
	                                                      0.0,
	                                                      1.0,
	                                                      0.0,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (TrackerMinerPrivate));

	miner_error_quark = g_quark_from_static_string ("TrackerMiner");
}

static void
miner_initable_iface_init (GInitableIface *iface)
{
	iface->init = miner_initable_init;
}

static gboolean
miner_initable_init (GInitable     *initable,
                     GCancellable  *cancellable,
                     GError       **error)
{
	TrackerMiner *miner = TRACKER_MINER (initable);
	GError *inner_error = NULL;
	GVariant *reply;
	guint32 rval;
	GDBusInterfaceVTable interface_vtable = {
		handle_method_call,
		handle_get_property,
		handle_set_property
	};

	/* Try to get SPARQL connection... */
	miner->private->connection = tracker_sparql_connection_get (NULL, error);
	if (!miner->private->connection) {
		return FALSE;
	}

	/* Try to get DBus connection... */
	miner->private->d_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
	if (!miner->private->d_connection) {
		return FALSE;
	}

	/* Setup introspection data */
	miner->private->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	/* Check miner has a proper name */
	if (!miner->private->name) {
		g_set_error (error,
		             TRACKER_MINER_ERROR,
		             0,
		             "Miner '%s' should have been given a name, bailing out",
		             G_OBJECT_TYPE_NAME (miner));
		return FALSE;
	}

	/* Setup full name */
	miner->private->full_name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX,
	                                         miner->private->name,
	                                         NULL);

	/* Register the service name for the miner */
	miner->private->full_path = g_strconcat (TRACKER_MINER_DBUS_PATH_PREFIX,
	                                         miner->private->name,
	                                         NULL);

	g_message ("Registering D-Bus object...");
	g_message ("  Path:'%s'", miner->private->full_path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (miner));

	miner->private->registration_id =
		g_dbus_connection_register_object (miner->private->d_connection,
		                                   miner->private->full_path,
	                                       miner->private->introspection_data->interfaces[0],
	                                       &interface_vtable,
	                                       miner,
	                                       NULL,
		                                   &inner_error);
	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_prefix_error (error,
		                "Could not register the D-Bus object '%s'. ",
		                miner->private->full_path);
		return FALSE;
	}

	reply = g_dbus_connection_call_sync (miner->private->d_connection,
	                                     "org.freedesktop.DBus",
	                                     "/org/freedesktop/DBus",
	                                     "org.freedesktop.DBus",
	                                     "RequestName",
	                                     g_variant_new ("(su)",
	                                                    miner->private->full_name,
	                                                    0x4 /* DBUS_NAME_FLAG_DO_NOT_QUEUE */),
	                                     G_VARIANT_TYPE ("(u)"),
	                                     0, -1, NULL, &inner_error);
	if (inner_error) {
		g_propagate_error (error, inner_error);
		g_prefix_error (error,
		                "Could not acquire name:'%s'. ",
		                miner->private->full_name);
		return FALSE;
	}

	g_variant_get (reply, "(u)", &rval);
	g_variant_unref (reply);

	if (rval != 1 /* DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER */) {
		g_set_error (error,
		             TRACKER_MINER_ERROR,
		             0,
		             "D-Bus service name:'%s' is already taken, "
		             "perhaps the application is already running?",
		             miner->private->full_name);
		return FALSE;
	}

	miner->private->watch_name_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
	                                                  TRACKER_SERVICE,
	                                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
	                                                  on_tracker_store_appeared,
	                                                  on_tracker_store_disappeared,
	                                                  miner,
	                                                  NULL);

	return TRUE;
}

static void
tracker_miner_init (TrackerMiner *miner)
{
	TrackerMinerPrivate *priv;
	miner->private = priv = TRACKER_MINER_GET_PRIVATE (miner);

	priv->pauses = g_hash_table_new_full (g_direct_hash,
	                                      g_direct_equal,
	                                      NULL,
	                                      pause_data_destroy);
}

static void
miner_update_progress (TrackerMiner *miner)
{
	g_signal_emit (miner, signals[PROGRESS], 0,
	               miner->private->status,
	               miner->private->progress);

	if (miner->private->d_connection) {
		g_dbus_connection_emit_signal (miner->private->d_connection,
		                               NULL,
		                               miner->private->full_path,
		                               TRACKER_MINER_DBUS_INTERFACE,
		                               "Progress",
		                               g_variant_new ("(sd)",
		                                              miner->private->status,
		                                              miner->private->progress),
		                               NULL);
	}
}

static void
miner_set_property (GObject      *object,
                    guint         prop_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	switch (prop_id) {
	case PROP_NAME:
		g_free (miner->private->name);
		miner->private->name = g_value_dup_string (value);
		break;
	case PROP_STATUS: {
		const gchar *new_status;

		new_status = g_value_get_string (value);
		if (miner->private->status && new_status &&
		    strcmp (miner->private->status, new_status) == 0) {
			/* Same, do nothing */
			break;
		}

		g_free (miner->private->status);
		miner->private->status = g_strdup (new_status);
		miner_update_progress (miner);
		break;
	}
	case PROP_PROGRESS: {
		gdouble new_progress;

		new_progress = PROGRESS_ROUNDED (g_value_get_double (value));

		/* NOTE: We don't round the current progress before
		 * comparison because we use the rounded value when
		 * we set it last.
		 *
		 * Only notify 1% changes
		 */
		if (new_progress == miner->private->progress) {
			/* Same, do nothing */
			break;
		}

		miner->private->progress = new_progress;
		miner_update_progress (miner);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
miner_get_property (GObject    *object,
                    guint       prop_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, miner->private->name);
		break;
	case PROP_STATUS:
		g_value_set_string (value, miner->private->status);
		break;
	case PROP_PROGRESS:
		g_value_set_double (value, miner->private->progress);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static PauseData *
pause_data_new (const gchar *application,
                const gchar *reason)
{
	PauseData *data;
	static gint cookie = 1;

	data = g_slice_new0 (PauseData);

	data->cookie = cookie++;
	data->application = g_strdup (application);
	data->reason = g_strdup (reason);

	return data;
}

static void
pause_data_destroy (gpointer data)
{
	PauseData *pd;

	pd = data;

	g_free (pd->reason);
	g_free (pd->application);

	g_slice_free (PauseData, pd);
}

/**
 * tracker_miner_error_quark:
 *
 * Returns the #GQuark used to identify miner errors in GError structures.
 *
 * Returns: the error #GQuark
 **/
GQuark
tracker_miner_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_MINER_ERROR_DOMAIN);
}

/**
 * tracker_miner_start:
 * @miner: a #TrackerMiner
 *
 * Tells the miner to start processing data.
 **/
void
tracker_miner_start (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (miner->private->started == FALSE);

	miner->private->started = TRUE;

	g_signal_emit (miner, signals[STARTED], 0);

	if (miner->private->d_connection) {
		g_dbus_connection_emit_signal (miner->private->d_connection,
		                               NULL,
		                               miner->private->full_path,
		                               TRACKER_MINER_DBUS_INTERFACE,
		                               "Started",
		                               NULL,
		                               NULL);
	}
}

/**
 * tracker_miner_stop:
 * @miner: a #TrackerMiner
 *
 * Tells the miner to stop processing data.
 **/
void
tracker_miner_stop (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (miner->private->started == TRUE);

	miner->private->started = FALSE;

	g_signal_emit (miner, signals[STOPPED], 0);

	if (miner->private->d_connection) {
		g_dbus_connection_emit_signal (miner->private->d_connection,
		                               NULL,
		                               miner->private->full_path,
		                               TRACKER_MINER_DBUS_INTERFACE,
		                               "Stopped",
		                               NULL,
		                               NULL);
	}
}

/**
 * tracker_miner_ignore_next_update:
 * @miner: a #TrackerMiner
 * @urls: the urls to mark as to ignore on next update
 *
 * Tells the miner to mark @urls are to ignore on next update.
 **/
void
tracker_miner_ignore_next_update (TrackerMiner *miner,
                                  const GStrv   urls)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));

	g_signal_emit (miner, signals[IGNORE_NEXT_UPDATE], 0, urls);
}

/**
 * tracker_miner_is_started:
 * @miner: a #TrackerMiner
 *
 * Returns #TRUE if the miner has been started.
 *
 * Returns: #TRUE if the miner is already started.
 **/
gboolean
tracker_miner_is_started (TrackerMiner *miner)
{
	g_return_val_if_fail (TRACKER_IS_MINER (miner), TRUE);

	return miner->private->started;
}

static gint
tracker_miner_pause_internal (TrackerMiner  *miner,
                              const gchar   *application,
                              const gchar   *reason,
                              GError       **error)
{
	PauseData *pd;
	GHashTableIter iter;
	gpointer key, value;

	/* Check this is not a duplicate pause */
	g_hash_table_iter_init (&iter, miner->private->pauses);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PauseData *pd = value;

		if (g_strcmp0 (application, pd->application) == 0 &&
		    g_strcmp0 (reason, pd->reason) == 0) {
			/* Can't use duplicate pauses */
			g_set_error_literal (error, TRACKER_MINER_ERROR, 0,
			                     _("Pause application and reason match an already existing pause request"));
			return -1;
		}
	}

	pd = pause_data_new (application, reason);

	g_hash_table_insert (miner->private->pauses,
	                     GINT_TO_POINTER (pd->cookie),
	                     pd);

	if (g_hash_table_size (miner->private->pauses) == 1) {
		/* Pause */
		g_message ("Miner:'%s' is pausing", miner->private->name);
		g_signal_emit (miner, signals[PAUSED], 0);

		if (miner->private->d_connection) {
			g_dbus_connection_emit_signal (miner->private->d_connection,
			                               NULL,
			                               miner->private->full_path,
			                               TRACKER_MINER_DBUS_INTERFACE,
			                               "Paused",
			                               NULL,
			                               NULL);
		}
	}

	return pd->cookie;
}

/**
 * tracker_miner_pause:
 * @miner: a #TrackerMiner
 * @reason: reason to pause
 * @error: return location for errors
 *
 * Asks @miner to pause. On success the cookie ID is returned,
 * this is what must be used in tracker_miner_resume() to resume
 * operations. On failure @error will be set and -1 will be returned.
 *
 * Returns: The pause cookie ID.
 **/
gint
tracker_miner_pause (TrackerMiner  *miner,
                     const gchar   *reason,
                     GError       **error)
{
	const gchar *application;

	g_return_val_if_fail (TRACKER_IS_MINER (miner), -1);
	g_return_val_if_fail (reason != NULL, -1);

	application = g_get_application_name ();

	if (!application) {
		application = miner->private->name;
	}

	return tracker_miner_pause_internal (miner, application, reason, error);
}

/**
 * tracker_miner_resume:
 * @miner: a #TrackerMiner
 * @cookie: pause cookie
 * @error: return location for errors
 *
 * Asks the miner to resume processing. The cookie must be something
 * returned by tracker_miner_pause(). The miner won't actually resume
 * operations until all pause requests have been resumed.
 *
 * Returns: #TRUE if the cookie was valid.
 **/
gboolean
tracker_miner_resume (TrackerMiner  *miner,
                      gint           cookie,
                      GError       **error)
{
	g_return_val_if_fail (TRACKER_IS_MINER (miner), FALSE);

	if (!g_hash_table_remove (miner->private->pauses, GINT_TO_POINTER (cookie))) {
		g_set_error_literal (error, TRACKER_MINER_ERROR, 0,
		                     _("Cookie not recognized to resume paused miner"));
		return FALSE;
	}

	if (g_hash_table_size (miner->private->pauses) == 0) {
		/* Resume */
		g_message ("Miner:'%s' is resuming", miner->private->name);
		g_signal_emit (miner, signals[RESUMED], 0);

		if (miner->private->d_connection) {
			g_dbus_connection_emit_signal (miner->private->d_connection,
			                               NULL,
			                               miner->private->full_path,
			                               TRACKER_MINER_DBUS_INTERFACE,
			                               "Resumed",
			                               NULL,
			                               NULL);
		}
	}

	return TRUE;
}

TrackerSparqlConnection *
tracker_miner_get_connection (TrackerMiner *miner)
{
	return miner->private->connection;
}

static void
miner_finalize (GObject *object)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	if (miner->private->watch_name_id != 0) {
		g_bus_unwatch_name (miner->private->watch_name_id);
	}

	if (miner->private->registration_id != 0) {
		g_dbus_connection_unregister_object (miner->private->d_connection,
		                                     miner->private->registration_id);
	}

	if (miner->private->introspection_data) {
		g_dbus_node_info_unref (miner->private->introspection_data);
	}

	if (miner->private->d_connection) {
		g_object_unref (miner->private->d_connection);
	}

	g_free (miner->private->status);
	g_free (miner->private->name);
	g_free (miner->private->full_name);
	g_free (miner->private->full_path);

	if (miner->private->connection) {
		g_object_unref (miner->private->connection);
	}

	g_hash_table_unref (miner->private->pauses);

	G_OBJECT_CLASS (tracker_miner_parent_class)->finalize (object);
}

static void
handle_method_call_ignore_next_update (TrackerMiner          *miner,
                                       GDBusMethodInvocation *invocation,
                                       GVariant              *parameters)
{
	GStrv urls = NULL;
	TrackerDBusRequest *request;

	g_variant_get (parameters, "(^a&s)", &urls);

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s", __PRETTY_FUNCTION__);

	tracker_miner_ignore_next_update (miner, urls);

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation, NULL);
	g_free (urls);
}

static void
handle_method_call_resume (TrackerMiner          *miner,
                           GDBusMethodInvocation *invocation,
                           GVariant              *parameters)
{
	GError *local_error = NULL;
	gint cookie;
	TrackerDBusRequest *request;

	g_variant_get (parameters, "(i)", &cookie);

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s(cookie:%d)",
	                                        __PRETTY_FUNCTION__,
	                                        cookie);

	if (!tracker_miner_resume (miner, cookie, &local_error)) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);
		return;
	}

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
handle_method_call_pause (TrackerMiner          *miner,
                          GDBusMethodInvocation *invocation,
                          GVariant              *parameters)
{
	GError *local_error = NULL;
	gint cookie;
	const gchar *application = NULL, *reason = NULL;
	TrackerDBusRequest *request;

	g_variant_get (parameters, "(&s&s)", &application, &reason);

	tracker_gdbus_async_return_if_fail (application != NULL, invocation);
	tracker_gdbus_async_return_if_fail (reason != NULL, invocation);

	request = tracker_g_dbus_request_begin (invocation,
	                                        "%s(application:'%s', reason:'%s')",
	                                        __PRETTY_FUNCTION__,
	                                        application,
	                                        reason);

	cookie = tracker_miner_pause_internal (miner, application, reason, &local_error);
	if (cookie == -1) {
		tracker_dbus_request_end (request, local_error);

		g_dbus_method_invocation_return_gerror (invocation, local_error);

		g_error_free (local_error);

		return;
	}

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(i)", cookie));
}

static void
handle_method_call_get_pause_details (TrackerMiner          *miner,
                                      GDBusMethodInvocation *invocation,
                                      GVariant              *parameters)
{
	GSList *applications, *reasons;
	GStrv applications_strv, reasons_strv;
	GHashTableIter iter;
	gpointer key, value;
	TrackerDBusRequest *request;

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	applications = NULL;
	reasons = NULL;
	g_hash_table_iter_init (&iter, miner->private->pauses);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PauseData *pd = value;

		applications = g_slist_prepend (applications, pd->application);
		reasons = g_slist_prepend (reasons, pd->reason);
	}
	applications = g_slist_reverse (applications);
	reasons = g_slist_reverse (reasons);
	applications_strv = tracker_gslist_to_string_list (applications);
	reasons_strv = tracker_gslist_to_string_list (reasons);

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(^as^as)",
	                                                      applications_strv,
	                                                      reasons_strv));

	g_strfreev (applications_strv);
	g_strfreev (reasons_strv);
	g_slist_free (applications);
	g_slist_free (reasons);
}

static void
handle_method_call_get_progress (TrackerMiner          *miner,
                                 GDBusMethodInvocation *invocation,
                                 GVariant              *parameters)
{
	TrackerDBusRequest *request;

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(d)", miner->private->progress));
}

static void
handle_method_call_get_status (TrackerMiner          *miner,
                               GDBusMethodInvocation *invocation,
                               GVariant              *parameters)
{
	TrackerDBusRequest *request;

	request = tracker_g_dbus_request_begin (invocation, "%s()", __PRETTY_FUNCTION__);

	tracker_dbus_request_end (request, NULL);
	g_dbus_method_invocation_return_value (invocation,
	                                       g_variant_new ("(s)", miner->private->status ? miner->private->status : ""));

}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	TrackerMiner *miner = user_data;

	tracker_gdbus_async_return_if_fail (miner != NULL, invocation);

	if (g_strcmp0 (method_name, "IgnoreNextUpdate") == 0) {
		handle_method_call_ignore_next_update (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "Resume") == 0) {
		handle_method_call_resume (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "Pause") == 0) {
		handle_method_call_pause (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetPauseDetails") == 0) {
		handle_method_call_get_pause_details (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetProgress") == 0) {
		handle_method_call_get_progress (miner, invocation, parameters);
	} else
	if (g_strcmp0 (method_name, "GetStatus") == 0) {
		handle_method_call_get_status (miner, invocation, parameters);
	} else {
		g_assert_not_reached ();
	}
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return NULL;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	g_assert_not_reached ();
	return TRUE;
}

static void
on_tracker_store_appeared (GDBusConnection *connection,
                           const gchar     *name,
                           const gchar     *name_owner,
                           gpointer         user_data)

{
	TrackerMiner *miner = user_data;

	g_debug ("Miner:'%s' noticed store availability has changed to AVAILABLE",
	         miner->private->name);

	if (miner->private->availability_cookie != 0) {
		GError *error = NULL;

		tracker_miner_resume (miner,
		                      miner->private->availability_cookie,
		                      &error);

		if (error) {
			g_warning ("Error happened resuming miner, %s", error->message);
			g_error_free (error);
		}

		miner->private->availability_cookie = 0;
	}
}

static void
on_tracker_store_disappeared (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         user_data)
{
	TrackerMiner *miner = user_data;

	g_debug ("Miner:'%s' noticed store availability has changed to UNAVAILABLE",
	         miner->private->name);

	if (miner->private->availability_cookie == 0) {
		GError *error = NULL;
		gint cookie_id;

		cookie_id = tracker_miner_pause (miner,
		                                 _("Data store is not available"),
		                                 &error);

		if (error) {
			g_warning ("Could not pause, %s", error->message);
			g_error_free (error);
		} else {
			miner->private->availability_cookie = cookie_id;
		}
	}
}
