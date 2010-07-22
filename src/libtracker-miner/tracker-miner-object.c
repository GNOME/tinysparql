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

#include <libtracker-common/tracker-dbus.h>
#include <libtracker-common/tracker-type-utils.h>

#include "tracker-marshal.h"
#include "tracker-miner-object.h"
#include "tracker-miner-dbus.h"
#include "tracker-miner-glue.h"
#include "tracker-dbus.h"

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

struct TrackerMinerPrivate {
	TrackerSparqlConnection *connection;

	GHashTable *pauses;

	gboolean started;

	gchar *name;
	gchar *status;
	gdouble progress;

	gint availability_cookie;
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

static void       miner_set_property           (GObject       *object,
                                                guint          param_id,
                                                const GValue  *value,
                                                GParamSpec    *pspec);
static void       miner_get_property           (GObject       *object,
                                                guint          param_id,
                                                GValue        *value,
                                                GParamSpec    *pspec);
static void       miner_finalize               (GObject       *object);
static void       miner_constructed            (GObject       *object);
static void       pause_data_destroy           (gpointer       data);
static PauseData *pause_data_new               (const gchar   *application,
                                                const gchar   *reason);
static void       store_name_monitor_cb (TrackerMiner *miner,
                                         const gchar  *name,
                                         gboolean      available);

G_DEFINE_ABSTRACT_TYPE (TrackerMiner, tracker_miner, G_TYPE_OBJECT)

static void
tracker_miner_class_init (TrackerMinerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = miner_set_property;
	object_class->get_property = miner_get_property;
	object_class->finalize     = miner_finalize;
	object_class->constructed  = miner_constructed;

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
tracker_miner_init (TrackerMiner *miner)
{
	TrackerMinerPrivate *priv;
	GError *error = NULL;

	miner->private = priv = TRACKER_MINER_GET_PRIVATE (miner);

	priv->connection = tracker_sparql_connection_get (&error);
	g_assert_no_error (error);

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

		new_progress = g_value_get_double (value);

		/* Only notify 1% changes */
		if ((gint) (miner->private->progress * 100) == (gint) (new_progress * 100)) {
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

static void
miner_finalize (GObject *object)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	g_free (miner->private->status);
	g_free (miner->private->name);

	if (miner->private->connection) {
		g_object_unref (miner->private->connection);
	}

	g_hash_table_unref (miner->private->pauses);

	_tracker_miner_dbus_shutdown (miner);

	G_OBJECT_CLASS (tracker_miner_parent_class)->finalize (object);
}

static void
miner_constructed (GObject *object)
{
	TrackerMiner *miner = TRACKER_MINER (object);

	_tracker_miner_dbus_init (miner, &dbus_glib__tracker_miner_object_info);
	_tracker_miner_dbus_add_name_watch (miner, "org.freedesktop.Tracker1",
                                            store_name_monitor_cb);
}

static void
store_name_monitor_cb (TrackerMiner *miner,
                       const gchar  *name,
                       gboolean      available)
{
	GError *error = NULL;

	g_debug ("Store availability has changed to %s",
		 available ? "AVAILABLE" : "UNAVAILABLE");

	if (available && miner->private->availability_cookie != 0) {
		tracker_miner_resume (miner,
		                      miner->private->availability_cookie,
		                      &error);

		if (error) {
			g_warning ("Error happened resuming miner, %s", error->message);
			g_error_free (error);
		}

		miner->private->availability_cookie = 0;
	} else if (!available && miner->private->availability_cookie == 0) {
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
		g_message ("Miner is pausing");
		g_signal_emit (miner, signals[PAUSED], 0);
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
		g_message ("Miner is resuming");
		g_signal_emit (miner, signals[RESUMED], 0);
	}

	return TRUE;
}

TrackerSparqlConnection *
tracker_miner_get_connection (TrackerMiner *miner)
{
	return miner->private->connection;
}

/* DBus methods */
void
_tracker_miner_dbus_get_status (TrackerMiner           *miner,
                                DBusGMethodInvocation  *context,
                                GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, context, "%s()", __PRETTY_FUNCTION__);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, miner->private->status);
}

void
_tracker_miner_dbus_get_progress (TrackerMiner           *miner,
                                  DBusGMethodInvocation  *context,
                                  GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, context, "%s()", __PRETTY_FUNCTION__);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, miner->private->progress);
}

void
_tracker_miner_dbus_get_pause_details (TrackerMiner           *miner,
                                       DBusGMethodInvocation  *context,
                                       GError                **error)
{
	GSList *applications, *reasons;
	GStrv applications_strv, reasons_strv;
	GHashTableIter iter;
	gpointer key, value;
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, context, "%s()", __PRETTY_FUNCTION__);

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

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, applications_strv, reasons_strv);

	g_strfreev (applications_strv);
	g_strfreev (reasons_strv);

	g_slist_free (applications);
	g_slist_free (reasons);
}

void
_tracker_miner_dbus_pause (TrackerMiner           *miner,
                           const gchar            *application,
                           const gchar            *reason,
                           DBusGMethodInvocation  *context,
                           GError                **error)
{
	GError *local_error = NULL;
	guint request_id;
	gint cookie;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);
	tracker_dbus_async_return_if_fail (application != NULL, context);
	tracker_dbus_async_return_if_fail (reason != NULL, context);

	tracker_dbus_request_new (request_id, context,
	                          "%s(application:'%s', reason:'%s')",
	                          __PRETTY_FUNCTION__,
	                          application,
	                          reason);

	cookie = tracker_miner_pause_internal (miner, application, reason, &local_error);
	if (cookie == -1) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             context,
		                             &actual_error,
		                             local_error ? local_error->message : NULL);
		dbus_g_method_return_error (context, actual_error);

		g_error_free (actual_error);
		g_error_free (local_error);

		return;
	}

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context, cookie);
}

void
_tracker_miner_dbus_resume (TrackerMiner           *miner,
                            gint                    cookie,
                            DBusGMethodInvocation  *context,
                            GError                **error)
{
	GError *local_error = NULL;
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id,
	                          context,
	                          "%s(cookie:%d)",
	                          __PRETTY_FUNCTION__,
	                          cookie);

	if (!tracker_miner_resume (miner, cookie, &local_error)) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             context,
		                             &actual_error,
		                             local_error ? local_error->message : NULL);
		dbus_g_method_return_error (context, actual_error);

		g_error_free (actual_error);
		g_error_free (local_error);

		return;
	}

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context);
}

void
_tracker_miner_dbus_ignore_next_update (TrackerMiner           *miner,
                                        const GStrv             urls,
                                        DBusGMethodInvocation  *context,
                                        GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, context, "%s()", __PRETTY_FUNCTION__);

	tracker_miner_ignore_next_update (miner, urls);

	tracker_dbus_request_success (request_id, context);
	dbus_g_method_return (context);
}
