/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009, Nokia (urho.konttori@nokia.com)
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

#include <stdlib.h>

#include <libtracker-common/tracker-dbus.h>

#include "tracker-marshal.h"
#include "tracker-miner.h"
#include "tracker-miner-dbus.h"
#include "tracker-miner-glue.h"

#define TRACKER_MINER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER, TrackerMinerPrivate))

struct TrackerMinerPrivate {
	TrackerClient *client;
	
	GHashTable *pauses;

	gboolean started;
	
	gchar *name;
	gchar *description;
	gchar *status;
	gdouble progress;
};

typedef struct {
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GHashTable *name_monitors;
} DBusData;

typedef struct {
	gint cookie;
	gchar *application;
	gchar *reason;	
} PauseData;

enum {
	PROP_0,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_STATUS,
	PROP_PROGRESS
};

enum {
	STARTED,
	STOPPED,
	PAUSED,
	RESUMED,
	TERMINATED,
	PROGRESS,
	ERROR,
	LAST_SIGNAL
};

static GQuark dbus_data = 0;

static guint signals[LAST_SIGNAL] = { 0 };

static void       miner_set_property (GObject      *object,
				      guint         param_id,
				      const GValue *value,
				      GParamSpec   *pspec);
static void       miner_get_property (GObject      *object,
				      guint         param_id,
				      GValue       *value,
				      GParamSpec   *pspec);
static void       miner_finalize     (GObject      *object);
static void       miner_constructed  (GObject      *object);
static void       dbus_data_destroy  (gpointer      data);
static DBusData * dbus_data_create   (TrackerMiner *miner,
				      const gchar  *name);
static void       pause_data_destroy (gpointer      data);
static PauseData *pause_data_new     (const gchar  *application,
				      const gchar  *reason);

G_DEFINE_ABSTRACT_TYPE (TrackerMiner, tracker_miner, G_TYPE_OBJECT)

static void
tracker_miner_class_init (TrackerMinerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = miner_set_property;
	object_class->get_property = miner_get_property;
	object_class->finalize     = miner_finalize;
	object_class->constructed  = miner_constructed;

	signals[STARTED] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[STOPPED] =
		g_signal_new ("stopped",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, stopped),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[PAUSED] =
		g_signal_new ("paused",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[RESUMED] =
		g_signal_new ("resumed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, resumed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[TERMINATED] =
		g_signal_new ("terminated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, terminated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
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
	signals[ERROR] =
		g_signal_new ("error",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerMinerClass, error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Name",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "Description",
							      "Description",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Status",
							      "Status (unique to each miner)",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PROGRESS,
					 g_param_spec_double ("progress",
							      "Progress",
							      "Progress (between 0 and 1)",
							      0.0, 
							      1.0,
							      0.0,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (TrackerMinerPrivate));
}

static void
tracker_miner_init (TrackerMiner *miner)
{
	TrackerMinerPrivate *priv;

	miner->private = priv = TRACKER_MINER_GET_PRIVATE (miner);

	priv->client = tracker_connect (TRUE, -1);

	priv->pauses = g_hash_table_new_full (g_direct_hash,
					      g_direct_equal,
					      NULL,
					      pause_data_destroy);
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
	case PROP_DESCRIPTION:
		g_free (miner->private->description);
		miner->private->description = g_value_dup_string (value);
		break;
	case PROP_STATUS:
		g_free (miner->private->status);
		miner->private->status = g_value_dup_string (value);
		break;
	case PROP_PROGRESS:
		miner->private->progress = g_value_get_double (value);
		/* Do we signal here? */
		break;
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
	case PROP_DESCRIPTION:
		g_value_set_string (value, miner->private->description);
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
	g_free (miner->private->description);
	g_free (miner->private->name);

	if (miner->private->client) {
		tracker_disconnect (miner->private->client);
	}

	if (dbus_data != 0) {
		g_object_set_qdata (G_OBJECT (miner), dbus_data, NULL);
	}

	G_OBJECT_CLASS (tracker_miner_parent_class)->finalize (object);
}

static void
miner_constructed (GObject *object)
{
	TrackerMiner *miner;
	DBusData *data;

	miner = TRACKER_MINER (object);

	if (!miner->private->name) {
		g_critical ("Miner should have been given a name, bailing out");
		g_assert_not_reached ();
	}

	if (G_UNLIKELY (dbus_data == 0)) {
		dbus_data = g_quark_from_static_string ("tracker-miner-dbus-data");
	}

	data = g_object_get_qdata (G_OBJECT (miner), dbus_data);

	if (G_LIKELY (!data)) {
		data = dbus_data_create (miner, miner->private->name);
	}

	if (G_UNLIKELY (!data)) {
		g_critical ("Miner could not register object on DBus session");
		exit (EXIT_FAILURE);
		return;
	}

	g_object_set_qdata_full (G_OBJECT (miner), 
				 dbus_data, 
				 data,
				 dbus_data_destroy);
}

GQuark
tracker_miner_error_quark (void)
{
	return g_quark_from_static_string (TRACKER_MINER_ERROR_DOMAIN);
}

static gboolean
dbus_register_service (DBusGProxy  *proxy,
		       const gchar *name)
{
	GError *error = NULL;
	guint	result;

	g_message ("Registering DBus service...\n"
		   "  Name:'%s'",
		   name);

	if (!org_freedesktop_DBus_request_name (proxy,
						name,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &error)) {
		g_critical ("Could not acquire name:'%s', %s",
			    name,
			    error ? error->message : "no error given");
		g_error_free (error);

		return FALSE;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_critical ("DBus service name:'%s' is already taken, "
			    "perhaps the application is already running?",
			    name);
		return FALSE;
	}

	return TRUE;
}

static gboolean
dbus_register_object (GObject		    *object,
		      DBusGConnection	    *connection,
		      DBusGProxy	    *proxy,
		      const DBusGObjectInfo *info,
		      const gchar	    *path)
{
	g_message ("Registering DBus object...");
	g_message ("  Path:'%s'", path);
	g_message ("  Object Type:'%s'", G_OBJECT_TYPE_NAME (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), info);
	dbus_g_connection_register_g_object (connection, path, object);

	return TRUE;
}

static void
dbus_data_destroy (gpointer data)
{
	DBusData *dd;

	dd = data;

	if (dd->gproxy) {
		g_object_unref (dd->gproxy);
	}

	if (dd->connection) {
		dbus_g_connection_unref (dd->connection);
	}

	if (dd->name_monitors) {
		g_hash_table_destroy (dd->name_monitors);
	}

	g_slice_free (DBusData, dd);
}

static DBusData *
dbus_data_create (TrackerMiner *miner,
		  const gchar  *name)
{
	DBusData *data;
	DBusGConnection *connection;
	DBusGProxy *gproxy;
	GError *error = NULL;
	gchar *full_name, *full_path;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection) {
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
		g_error_free (error);
		return NULL;
	}

	gproxy = dbus_g_proxy_new_for_name (connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);

	/* Register the service name for the miner */
	full_name = g_strconcat (TRACKER_MINER_DBUS_NAME_PREFIX, name, NULL);

	if (!dbus_register_service (gproxy, full_name)) {
		g_object_unref (gproxy);
		g_free (full_name);
		return NULL;
	}

	g_free (full_name);

	full_path = g_strconcat (TRACKER_MINER_DBUS_PATH_PREFIX, name, NULL);

	if (!dbus_register_object (G_OBJECT (miner),
				   connection, gproxy,
				   &dbus_glib_tracker_miner_object_info,
				   full_path)) {
		g_object_unref (gproxy);
		g_free (full_path);
		return NULL;
	}

	g_free (full_path);

	/* Now we're successfully connected and registered, create the data */
	data = g_slice_new0 (DBusData);
	data->connection = dbus_g_connection_ref (connection);
	data->gproxy = g_object_ref (gproxy);

	return data;
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

void
tracker_miner_start (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (miner->private->started == FALSE);

	miner->private->started = TRUE;

	g_signal_emit (miner, signals[STARTED], 0);
}

void
tracker_miner_stop (TrackerMiner *miner)
{
	g_return_if_fail (TRACKER_IS_MINER (miner));
	g_return_if_fail (miner->private->started == TRUE);

	miner->private->started = FALSE;

	g_signal_emit (miner, signals[STOPPED], 0);
}

TrackerClient *
tracker_miner_get_client (TrackerMiner *miner)
{
	g_return_val_if_fail (TRACKER_IS_MINER (miner), NULL);

	return miner->private->client;
}

gboolean
tracker_miner_execute_sparql (TrackerMiner  *miner,
			      const gchar   *sparql,
			      GError       **error)
{
	GError *internal_error = NULL;

	g_return_val_if_fail (TRACKER_IS_MINER (miner), FALSE);

	tracker_resources_batch_sparql_update (miner->private->client,
					       sparql, 
					       &internal_error);

	/* FIXME: should not commit after each update */
	tracker_resources_batch_commit (miner->private->client, NULL);

	if (!internal_error) {
		return TRUE;
	}

	if (error) {
		g_propagate_error (error, internal_error);
	} else {
		g_warning ("Error running sparql queries: %s", internal_error->message);
		g_error_free (internal_error);
	}

	return FALSE;
}

gint
tracker_miner_pause (TrackerMiner  *miner,
		     const gchar   *application,
		     const gchar   *reason,
		     GError       **error)
{
	PauseData *pd;

	g_return_val_if_fail (TRACKER_IS_MINER (miner), -1);
	g_return_val_if_fail (application != NULL, -1);
	g_return_val_if_fail (reason != NULL, -1);

	pd = pause_data_new (application, reason);

	g_hash_table_insert (miner->private->pauses, 
			     GINT_TO_POINTER (pd->cookie),
			     pd);

	if (g_hash_table_size (miner->private->pauses) == 1) {
		/* Pause */
		g_message ("Miner is pausing");
	}

	return pd->cookie;
}

gboolean 
tracker_miner_resume (TrackerMiner  *miner,
		      gint           cookie,
		      GError       **error)
{
	g_return_val_if_fail (TRACKER_IS_MINER (miner), FALSE);

	if (!g_hash_table_remove (miner->private->pauses, GINT_TO_POINTER (cookie))) {
		g_set_error (error, TRACKER_MINER_ERROR, 0, 
			     "%s",
			     _("Cookie not recognised to resume paused miner"));
		return FALSE;
	} 

	if (g_hash_table_size (miner->private->pauses) == 0) {
		/* Resume */
		g_message ("Miner is resuming");
	}

	return TRUE;
}


/* DBus methods */
void
tracker_miner_dbus_get_name (TrackerMiner           *miner,
			     DBusGMethodInvocation  *context,
			     GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s()", __PRETTY_FUNCTION__);

	dbus_g_method_return (context, miner->private->name);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_get_description (TrackerMiner           *miner,
				    DBusGMethodInvocation  *context,
				    GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s()", __PRETTY_FUNCTION__);

	dbus_g_method_return (context, miner->private->description);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_get_status (TrackerMiner           *miner,
			       DBusGMethodInvocation  *context,
			       GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s()", __PRETTY_FUNCTION__);

	dbus_g_method_return (context, miner->private->status);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_get_progress (TrackerMiner           *miner,
				 DBusGMethodInvocation  *context,
				 GError                **error)
{
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s()", __PRETTY_FUNCTION__);

	dbus_g_method_return (context, miner->private->progress);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_get_is_paused (TrackerMiner           *miner,
				  DBusGMethodInvocation  *context,
				  GError                **error)
{
	guint request_id;
	gboolean is_paused;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s()", __PRETTY_FUNCTION__);

	is_paused = g_hash_table_size (miner->private->pauses) > 0;
	dbus_g_method_return (context, is_paused);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_pause (TrackerMiner           *miner,
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

	tracker_dbus_request_new (request_id, "%s(application:'%s', reason:'%s')",
				  __PRETTY_FUNCTION__,
				  application,
				  reason);

	cookie = tracker_miner_pause (miner, application, reason, &local_error);
	if (cookie == -1) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             &actual_error,
		                             local_error ? local_error->message : NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		g_error_free (local_error);

		return;
	}

	dbus_g_method_return (context, cookie);

	tracker_dbus_request_success (request_id);
}

void
tracker_miner_dbus_resume (TrackerMiner           *miner,
			   gint                    cookie,
			   DBusGMethodInvocation  *context,
			   GError                **error)
{
	GError *local_error = NULL;
	guint request_id;

	request_id = tracker_dbus_get_next_request_id ();

	tracker_dbus_async_return_if_fail (miner != NULL, context);

	tracker_dbus_request_new (request_id, "%s(cookie:%d)", 
				  __PRETTY_FUNCTION__,
				  cookie);

	if (!tracker_miner_resume (miner, cookie, &local_error)) {
		GError *actual_error = NULL;

		tracker_dbus_request_failed (request_id,
		                             &actual_error,
		                             local_error ? local_error->message : NULL);
		dbus_g_method_return_error (context, actual_error);
		g_error_free (actual_error);
		g_error_free (local_error);

		return;
	}

	dbus_g_method_return (context);

	tracker_dbus_request_success (request_id);
}
