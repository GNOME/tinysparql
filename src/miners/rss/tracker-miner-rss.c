/*
 * Copyright (C) 2009/2010, Roberto Guido <madbob@users.barberaware.org>
 *                          Michele Tameni <michele@amdplanet.it>
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

#include "config.h"

#include <stdio.h>

#include <libgrss.h>

#include <libtracker-common/tracker-ontologies.h>

#include <glib/gi18n.h>

#include "tracker-miner-rss.h"

#define TRACKER_MINER_RSS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_MINER_RSS, TrackerMinerRSSPrivate))

typedef struct _TrackerMinerRSSPrivate TrackerMinerRSSPrivate;

struct _TrackerMinerRSSPrivate {
	gboolean paused;
	gboolean stopped;
	gchar *last_status;

	FeedsPool *pool;
	gint now_fetching;
	GDBusConnection *connection;
	guint graph_updated_id;
};

static void         miner_started               (TrackerMiner    *miner);
static void         miner_stopped               (TrackerMiner    *miner);
static void         miner_paused                (TrackerMiner    *miner);
static void         miner_resumed               (TrackerMiner    *miner);
static void         retrieve_and_schedule_feeds (TrackerMinerRSS *miner);
static void         change_status               (FeedsPool       *pool,
                                                 FeedChannel     *feed,
                                                 gpointer         user_data);
static void         feed_fetched                (FeedsPool       *pool,
                                                 FeedChannel     *feed,
                                                 GList           *items,
                                                 gpointer         user_data);
static const gchar *get_message_url             (FeedItem        *item);

G_DEFINE_TYPE (TrackerMinerRSS, tracker_miner_rss, TRACKER_TYPE_MINER)

static void
tracker_miner_rss_finalize (GObject *object)
{
	TrackerMinerRSSPrivate *priv;

	priv = TRACKER_MINER_RSS_GET_PRIVATE (object);

	priv->stopped = TRUE;
	g_free (priv->last_status);
	g_object_unref (priv->pool);

	g_dbus_connection_signal_unsubscribe (priv->connection, priv->graph_updated_id);
	g_object_unref (priv->connection);

	G_OBJECT_CLASS (tracker_miner_rss_parent_class)->finalize (object);
}

static void
tracker_miner_rss_class_init (TrackerMinerRSSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	object_class->finalize = tracker_miner_rss_finalize;

	miner_class->started = miner_started;
	miner_class->stopped = miner_stopped;
	miner_class->paused  = miner_paused;
	miner_class->resumed = miner_resumed;

	g_type_class_add_private (object_class, sizeof (TrackerMinerRSSPrivate));
}

static gboolean
check_if_update_is_ours (TrackerSparqlConnection *con,
                         gint                     p)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *query;
	const gchar *p_str;
	gboolean is_ours = FALSE;

	/* We default by returning FALSE to avoid recursive updates */

	/* FIXME: We should really use a:
	 *          SELECT ... { FILTER(?id IN (1,2,3)) }
	 *
	 *        For efficiency when we add more updates.
	 */
	query = g_strdup_printf ("SELECT tracker:uri(%d) {}", p);
        cursor = tracker_sparql_connection_query (con, query, NULL, &error);
        g_free (query);

        if (error) {
	        g_critical ("Could not check if GraphUpdated was our last update or not: %s",
	                    error->message ? error->message : "no error given");
	        g_error_free (error);

	        return is_ours;
        }

        tracker_sparql_cursor_next (cursor, NULL, NULL);
        p_str = tracker_sparql_cursor_get_string (cursor, 0, NULL);

        /* Crude way to check */
        if (p_str) {
	        if (g_ascii_strcasecmp (p_str, "http://www.tracker-project.org/temp/mfo#updatedTime") == 0) {
		        is_ours = TRUE;
	        }

	        /* More checks for the future */
        }

        g_object_unref (cursor);

        return is_ours;
}

static void
graph_updated_cb (GDBusConnection *connection,
                  const gchar     *sender_name,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *signal_name,
                  GVariant        *parameters,
                  gpointer         user_data)
{
	TrackerMinerRSS *rss;
	TrackerSparqlConnection *con;
	GVariantIter *deletes, *inserts;
	const gchar *c;
	gint g, s, p, o;
	gboolean update_is_ours = FALSE;

	rss = TRACKER_MINER_RSS (user_data);
	con = tracker_miner_get_connection (TRACKER_MINER (rss));

	g_message ("%s", signal_name);
	g_message ("  parameters:'%s'", g_variant_print (parameters, FALSE));

	g_message ("  ");

	g_variant_get (parameters, "(&sa(iiii)a(iiii))", &c, &deletes, &inserts);
	g_message ("  Class:'%s'", c);

	g_message ("  Deletes:");

	while (g_variant_iter_loop (deletes, "(iiii)", &g, &s, &p, &o)) {
		g_message ("    g:%d, s:%d, p:%d, o:%d", g, s, p, o);
		update_is_ours |= check_if_update_is_ours (con, p);
	}

	g_message ("  Inserts:");

	while (g_variant_iter_loop (inserts, "(iiii)", &g, &s, &p, &o)) {
		g_message ("    g:%d, s:%d, p:%d, o:%d", g, s, p, o);
		update_is_ours |= check_if_update_is_ours (con, p);
	}

	g_variant_iter_free (deletes);
	g_variant_iter_free (inserts);
	g_variant_unref (parameters);

	/* Check if it is our update or not */
	if (!update_is_ours) {
		retrieve_and_schedule_feeds (rss);
	} else {
		g_message ("  Signal was for our update, doing nothing");
	}
}

static void
tracker_miner_rss_init (TrackerMinerRSS *object)
{
	GError *error = NULL;
	TrackerMinerRSSPrivate *priv;

	g_message ("Initializing...");

	priv = TRACKER_MINER_RSS_GET_PRIVATE (object);

	priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (!priv->connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
			    error ? error->message : "no error given.");
		g_error_free (error);
		return;
	}

	priv->pool = feeds_pool_new ();
	g_signal_connect (priv->pool, "feed-fetching", G_CALLBACK (change_status), object);
	g_signal_connect (priv->pool, "feed-ready", G_CALLBACK (feed_fetched), object);
	priv->now_fetching = 0;

	g_message ("Listening for GraphUpdated changes on D-Bus interface...");
	g_message ("  arg0:'%s'", TRACKER_MFO_PREFIX "FeedChannel");

	priv->graph_updated_id =
		g_dbus_connection_signal_subscribe  (priv->connection,
		                                     "org.freedesktop.Tracker1",
		                                     "org.freedesktop.Tracker1.Resources",
		                                     "GraphUpdated",
		                                     "/org/freedesktop/Tracker1/Resources",
		                                     TRACKER_MFO_PREFIX "FeedChannel",
		                                     G_DBUS_SIGNAL_FLAGS_NONE,
		                                     graph_updated_cb,
		                                     object,
		                                     NULL);
}

static void
verify_channel_update (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	GError *error;

	error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (source), result, &error);
	if (error != NULL) {
		g_critical ("Could not update channel information, %s", error->message);
		g_error_free (error);
	}
}

static void
update_updated_interval (TrackerMinerRSS *miner,
                         gchar           *uri,
                         time_t          *now)
{
	TrackerSparqlBuilder *sparql;

	g_message ("Updating mfo:updatedTime for channel '%s'", uri);

	/* I hope there will be soon a SPARQL command to just update a
	 * value instead to delete and re-insert it
	 */

	sparql = tracker_sparql_builder_new_update ();
	tracker_sparql_builder_delete_open (sparql, NULL);
	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "mfo:updatedTime");
	tracker_sparql_builder_object_variable (sparql, "unknown");
	tracker_sparql_builder_delete_close (sparql);
	tracker_sparql_builder_where_open (sparql);
	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "mfo:updatedTime");
	tracker_sparql_builder_object_variable (sparql, "unknown");
	tracker_sparql_builder_where_close (sparql);

	tracker_sparql_builder_insert_open (sparql, NULL);
	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "mfo:updatedTime");
	tracker_sparql_builder_object_date (sparql, now);
	tracker_sparql_builder_insert_close (sparql);

	tracker_sparql_connection_update_async (tracker_miner_get_connection (TRACKER_MINER (miner)),
	                                        tracker_sparql_builder_get_result (sparql),
	                                        G_PRIORITY_DEFAULT,
	                                        NULL,
	                                        verify_channel_update,
	                                        NULL);
	g_object_unref (sparql);
}

static void
change_status (FeedsPool   *pool,
               FeedChannel *feed,
               gpointer     user_data)
{
	gint avail;
	gdouble prog;
	TrackerMinerRSS *miner;
	TrackerMinerRSSPrivate *priv;

	miner = TRACKER_MINER_RSS (user_data);
	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);
	avail = feeds_pool_get_listened_num (priv->pool);

	priv->now_fetching++;

	if (priv->now_fetching > avail)
		priv->now_fetching = avail;

	g_message ("Fetching channel '%s' (in progress: %d/%d)",
	           feed_channel_get_source (feed),
	           priv->now_fetching,
	           avail);

	prog = ((gdouble) priv->now_fetching) / ((gdouble) avail);
	g_object_set (miner, "progress", prog, "status", "Fetching…", NULL);
}

static void
verify_item_insertion (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	GError *error;
	gchar *title;

	title = user_data;
	error = NULL;

	tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (source), result, &error);
	if (error != NULL) {
		g_critical ("Could not insert feed information for message titled:'%s', %s",
		            title,
		            error->message);
		g_error_free (error);
	}

	g_free (title);
}

static void
item_verify_reply_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
	TrackerSparqlConnection *connection;
	time_t t;
	gchar *uri;
	gchar *title = NULL;
	const gchar *str;
	const gchar *url;
	gdouble latitude;
	gdouble longitude;
	const gchar *tmp_string;
	TrackerSparqlCursor *cursor;
	GError *error;
	TrackerSparqlBuilder *sparql;
	FeedItem *item;
	FeedChannel *feed;
	gboolean has_geolocation;

	connection = TRACKER_SPARQL_CONNECTION (source_object);
	error = NULL;
	cursor = tracker_sparql_connection_query_finish (connection, res, &error);

	if (error != NULL) {
		g_message ("Could not verify feed existance, %s", error->message);
		g_error_free (error);
		if (cursor) {
			g_object_unref (cursor);
		}
		return;
	}

	if (!tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		g_message ("No data in query response??");
		g_object_unref (cursor);
		return;
	}

	str = tracker_sparql_cursor_get_string (cursor, 0, NULL);
	if (g_strcmp0 (str, "1") == 0) {
		g_object_unref (cursor);
		return;
	}

	item = user_data;

	url = get_message_url (item);

	g_message ("Updating feed information for '%s'", url);

	sparql = tracker_sparql_builder_new_update ();

	has_geolocation = feed_item_get_geo_point (item, &latitude, &longitude);
	tracker_sparql_builder_insert_open (sparql, NULL);

	if (has_geolocation) {
		g_message ("  Geolocation, using longitude:%f, latitude:%f",
		           longitude, latitude);

		tracker_sparql_builder_subject (sparql, "_:location");
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "slo:GeoLocation");

		tracker_sparql_builder_predicate (sparql, "slo:latitude");
		tracker_sparql_builder_object_double (sparql, latitude);
		tracker_sparql_builder_predicate (sparql, "slo:longitude");
		tracker_sparql_builder_object_double (sparql, longitude);
	}

	tracker_sparql_builder_subject (sparql, "_:message");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "mfo:FeedMessage");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:RemoteDataObject");

	if (has_geolocation == TRUE) {
		tracker_sparql_builder_predicate (sparql, "slo:location");
		tracker_sparql_builder_object (sparql, "_:location");
	}

	tmp_string = feed_item_get_title (item);
	if (tmp_string != NULL) {
		g_message ("  Title:'%s'", tmp_string);

		tracker_sparql_builder_predicate (sparql, "nie:title");
		tracker_sparql_builder_object_unvalidated (sparql, tmp_string);

		title = g_strdup (tmp_string);
	}

	tmp_string = feed_item_get_description (item);
	if (tmp_string != NULL) {
		tracker_sparql_builder_predicate (sparql, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (sparql, tmp_string);
	}

	if (url != NULL) {
		tracker_sparql_builder_predicate (sparql, "nie:url");
		tracker_sparql_builder_object_unvalidated (sparql, url);
	}

	/* TODO nmo:receivedDate and mfo:downloadedTime are the same?
	 *      Ask for the MFO maintainer */

	t = time (NULL);

	tracker_sparql_builder_predicate (sparql, "nmo:receivedDate");
	tracker_sparql_builder_object_date (sparql, &t);

	tracker_sparql_builder_predicate (sparql, "mfo:downloadedTime");
	tracker_sparql_builder_object_date (sparql, &t);

	t = feed_item_get_publish_time (item);
	tracker_sparql_builder_predicate (sparql, "nie:contentCreated");
	tracker_sparql_builder_object_date (sparql, &t);

	tracker_sparql_builder_predicate (sparql, "nmo:isRead");
	tracker_sparql_builder_object_boolean (sparql, FALSE);

	feed = feed_item_get_parent (item);
	uri = g_object_get_data (G_OBJECT (feed), "subject");
	tracker_sparql_builder_predicate (sparql, "nmo:communicationChannel");
	tracker_sparql_builder_object_iri (sparql, uri);

	tracker_sparql_builder_insert_close (sparql);

	tracker_sparql_connection_update_async (connection,
	                                        tracker_sparql_builder_get_result (sparql),
	                                        G_PRIORITY_DEFAULT,
	                                        NULL,
	                                        verify_item_insertion,
	                                        title);

	g_object_unref (cursor);
	g_object_unref (sparql);
}

static void
check_if_save (TrackerMinerRSS *miner,
               FeedItem        *item)
{
	FeedChannel *feed;
	gchar *query;
	gchar *communication_channel;
	const gchar *url;

	url = get_message_url (item);
	feed = feed_item_get_parent (item);
	communication_channel = g_object_get_data (G_OBJECT (feed), "subject");

	g_debug ("Verifying feed '%s' is stored", url);

	query = g_strdup_printf ("ASK { ?message a mfo:FeedMessage; "
	                         "nie:url \"%s\"; nmo:communicationChannel <%s> }",
	                         url,
	                         communication_channel);

	tracker_sparql_connection_query_async (tracker_miner_get_connection (TRACKER_MINER (miner)),
	                                       query,
	                                       NULL,
	                                       item_verify_reply_cb,
	                                       item);
	g_free (query);
}

static void
feed_fetched (FeedsPool   *pool,
              FeedChannel *feed,
              GList       *items,
              gpointer     user_data)
{
	gchar *uri;
	time_t now;
	GList *iter;
	FeedItem *item;
	TrackerMinerRSS *miner;
	TrackerMinerRSSPrivate *priv;

	miner = TRACKER_MINER_RSS (user_data);
	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);

	priv->now_fetching--;

	g_debug ("Feed fetched, %d remaining", priv->now_fetching);

	if (priv->now_fetching <= 0) {
		priv->now_fetching = 0;
		g_object_set (miner, "progress", 1.0, "status", "Idle", NULL);
	}

	if (items == NULL)
		return;

	now = time (NULL);
	uri = g_object_get_data (G_OBJECT (feed), "subject");
	update_updated_interval (miner, uri, &now);

	for (iter = items; iter; iter = iter->next) {
		item = iter->data;
		check_if_save (miner, item);
	}
}

static void
feeds_retrieve_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
	GList *channels;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	TrackerMinerRSSPrivate *priv;
	FeedChannel *chan;
	gint count;

	cursor = tracker_sparql_connection_query_finish (TRACKER_SPARQL_CONNECTION (source_object),
	                                                 res,
	                                                 &error);

	if (error != NULL) {
		g_message ("Could not retrieve feeds, %s", error->message);
		g_error_free (error);
		if (cursor) {
			g_object_unref (cursor);
		}
		return;
	}

	channels = NULL;
	count = 0;

	while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
		const gchar *source;
		const gchar *interval;
		const gchar *subject;
		gint mins;

		if (count == 0) {
			g_message ("Found feeds");
		}

		count++;

		source = tracker_sparql_cursor_get_string (cursor, 0, NULL);
		interval = tracker_sparql_cursor_get_string (cursor, 1, NULL);
		subject = tracker_sparql_cursor_get_string (cursor, 2, NULL);

		chan = feed_channel_new ();
		g_object_set_data_full (G_OBJECT (chan),
		                        "subject",
		                        g_strdup (subject),
		                        g_free);
		feed_channel_set_source (chan, g_strdup (source));

		/* TODO How to manage feeds with an update mfo:updateInterval == 0 ?
		 * Here the interval is forced to be at least 1 minute, but perhaps those
		 * elements are to be considered "disabled"
		 */
		mins = strtoull (interval, NULL, 10);
		if (mins <= 0)
			mins = 1;
		feed_channel_set_update_interval (chan, mins);
		g_message ("  Feed:'%s' with subject:'%s' has interval of %s minutes", source, subject, interval);

		channels = g_list_prepend (channels, chan);
	}

	if (count == 0) {
		g_message ("No feeds set up, nothing more to do");
	}

	priv = TRACKER_MINER_RSS_GET_PRIVATE (user_data);
	feeds_pool_listen (priv->pool, channels);

	g_object_unref (cursor);

	if (count == 0) {
		g_object_set (user_data, "progress", 1.0, "status", "Idle", NULL);
	}
}

static void
retrieve_and_schedule_feeds (TrackerMinerRSS *miner)
{
	const gchar *sparql;

	g_message ("Retrieving and scheduling feeds...");

	sparql =
		"SELECT ?chanUrl ?interval ?chanUrn "
		"WHERE {"
		"  ?chanUrn a mfo:FeedChannel ; "
		"             mfo:feedSettings ?settings ; "
		"             nie:url ?chanUrl . "
		"  ?settings mfo:updateInterval ?interval "
		"}";

	tracker_sparql_connection_query_async (tracker_miner_get_connection (TRACKER_MINER (miner)),
	                                       sparql,
	                                       NULL,
	                                       feeds_retrieve_cb,
	                                       miner);
}

static const gchar *
get_message_url (FeedItem *item)
{
	const gchar *url;

	feed_item_get_real_source (item, &url, NULL);
	if (url == NULL)
		url = feed_item_get_source (item);
	return url;
}

static void
miner_started (TrackerMiner *miner)
{
	TrackerMinerRSSPrivate *priv;

	g_object_set (miner, "progress", 0.0, "status", "Initializing", NULL);

	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);
	retrieve_and_schedule_feeds (TRACKER_MINER_RSS (miner));
	feeds_pool_switch (priv->pool, TRUE);
}

static void
miner_stopped (TrackerMiner *miner)
{
	TrackerMinerRSSPrivate *priv;

	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);
	feeds_pool_switch (priv->pool, FALSE);
	g_object_set (miner, "progress", 1.0, "status", "Idle", NULL);
}

static void
miner_paused (TrackerMiner *miner)
{
	TrackerMinerRSSPrivate *priv;

	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);
	feeds_pool_switch (priv->pool, FALSE);

	/* Save last status */
	g_free (priv->last_status);
	g_object_get (miner, "status", &priv->last_status, NULL);

	/* Set paused */
	g_object_set (miner, "status", "Paused", NULL);
}

static void
miner_resumed (TrackerMiner *miner)
{
	TrackerMinerRSSPrivate *priv;

	priv = TRACKER_MINER_RSS_GET_PRIVATE (miner);
	feeds_pool_switch (priv->pool, TRUE);

	/* Resume */
	g_object_set (miner, "status", priv->last_status ? priv->last_status : "Idle", NULL);
}

TrackerMinerRSS *
tracker_miner_rss_new (GError **error)
{
	return g_initable_new (TRACKER_TYPE_MINER_RSS,
	                       NULL,
	                       error,
	                       "name", "RSS",
	                       NULL);
}
