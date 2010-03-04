/*
 * Copyright (C) 2009/2010, Roberto Guido <madbob@users.barberaware.org>
 *                          Michele Tameni <michele@amdplanet.it>
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

#include "tracker-miner-rss.h"

#include <stdio.h>
#include <dbus/dbus-glib.h>
#include <libtracker-miner/tracker-miner.h>
#include <libtracker-common/tracker-sparql-builder.h>
#include <libgrss.h>

#define TRACKER_MINER_RSS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_MINER_RSS, TrackerMinerRSSPrivate))

struct TrackerMinerRSSPrivate {
	gboolean	paused;
	gboolean	stopped;

	FeedsPool	*pool;
	int		now_fetching;
};

static void tracker_miner_rss_started (TrackerMiner *miner);
static void tracker_miner_rss_stopped (TrackerMiner *miner);
static void tracker_miner_rss_paused (TrackerMiner *miner);
static void tracker_miner_rss_resumed (TrackerMiner *miner);
static void retrieve_and_schedule_feeds (TrackerMinerRSS *miner);
static void update_updated_interval (TrackerMinerRSS *miner, gchar *uri, time_t *now);
static void change_status (FeedsPool *pool, FeedChannel *feed, gpointer user_data);
static void feed_fetched (FeedsPool *pool, FeedChannel *feed, GList *items, gpointer user_data);
static gchar* get_message_subject (FeedItem *item);
static const gchar* get_message_url (FeedItem *item);

G_DEFINE_TYPE (TrackerMinerRSS, tracker_miner_rss, TRACKER_TYPE_MINER)

static void
tracker_miner_rss_finalize (GObject *object)
{
	TrackerMinerRSS *rss;

	rss = TRACKER_MINER_RSS (object);

	rss->priv->stopped = TRUE;
	g_object_unref (rss->priv->pool);

	G_OBJECT_CLASS (tracker_miner_rss_parent_class)->finalize (object);
}

static void
tracker_miner_rss_class_init (TrackerMinerRSSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TrackerMinerClass *miner_class = TRACKER_MINER_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (TrackerMinerRSSPrivate));
	object_class->finalize = tracker_miner_rss_finalize;

	miner_class->started = tracker_miner_rss_started;
	miner_class->stopped = tracker_miner_rss_stopped;
	miner_class->paused  = tracker_miner_rss_paused;
	miner_class->resumed = tracker_miner_rss_resumed;
}

static void
subjects_added_cb (DBusGProxy *proxy, gchar **subjects, gpointer user_data)
{
	TrackerMinerRSS *miner;

	miner = TRACKER_MINER_RSS (user_data);

	/*
		TODO	Add only the channels added?
	*/
	retrieve_and_schedule_feeds (miner);
}

static void
subjects_removed_cb (DBusGProxy *proxy, gchar **subjects, gpointer user_data)
{
	TrackerMinerRSS *miner;

	miner = TRACKER_MINER_RSS (user_data);

	/*
		TODO	Remove only the channels removed?
	*/
	retrieve_and_schedule_feeds (miner);
}

static void
tracker_miner_rss_init (TrackerMinerRSS *object)
{
	DBusGProxy *wrap;

	object->priv = TRACKER_MINER_RSS_GET_PRIVATE (object);

	object->priv->pool = feeds_pool_new ();
	g_signal_connect (object->priv->pool, "feed-fetching", G_CALLBACK (change_status), object);
	g_signal_connect (object->priv->pool, "feed-ready", G_CALLBACK (feed_fetched), object);
	object->priv->now_fetching = 0;

	g_object_set (object, "progress", 0.0, "status", "Initializing", NULL);

	wrap = dbus_g_proxy_new_for_name (dbus_g_bus_get (DBUS_BUS_SESSION, NULL),
	                                  "org.freedesktop.Tracker1",
					  "/org/freedesktop/Tracker1/Resources/Classes/mfo/FeedChannel",
	                                  "org.freedesktop.Tracker1.Resources.Class");

	if (wrap == NULL) {
		g_warning ("Unable to listen for added and removed channels");
		return;
	}

	dbus_g_proxy_add_signal (wrap, "SubjectsAdded", G_TYPE_STRV, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (wrap, "SubjectsAdded", G_CALLBACK (subjects_added_cb), object, NULL);

	dbus_g_proxy_add_signal (wrap, "SubjectsRemoved", G_TYPE_STRV, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (wrap, "SubjectsRemoved", G_CALLBACK (subjects_removed_cb), object, NULL);
}

static void
update_updated_interval (TrackerMinerRSS *miner, gchar *uri, time_t *now)
{
	TrackerSparqlBuilder *sparql;

	/*
		I hope there will be soon a SPARQL command to just update a
		value instead to delete and re-insert it
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

	tracker_sparql_builder_insert_open (sparql, uri);
	tracker_sparql_builder_subject_iri (sparql, uri);
	tracker_sparql_builder_predicate (sparql, "mfo:updatedTime");
	tracker_sparql_builder_object_date (sparql, now);
	tracker_sparql_builder_insert_close (sparql);

	tracker_miner_execute_update (TRACKER_MINER (miner), tracker_sparql_builder_get_result (sparql), NULL, NULL, NULL);
	g_object_unref (sparql);
}

static void
change_status (FeedsPool *pool, FeedChannel *feed, gpointer user_data)
{
	int avail;
	double prog;
	TrackerMinerRSS *miner;

	miner = (TrackerMinerRSS*) user_data;
	avail = feeds_pool_get_listened_num (miner->priv->pool);

	miner->priv->now_fetching++;
	if (miner->priv->now_fetching > avail)
		miner->priv->now_fetching = avail;

	prog = (double) miner->priv->now_fetching / (double) avail;
	g_object_set (miner, "progress", prog, "status", "Fetching...", NULL);
}

static void
item_verify_reply_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	time_t t;
	gchar *uri;
	gchar *subject;
	gchar **values;
	gdouble latitude;
	gdouble longitude;
	const gchar *tmp_string;
	const GPtrArray *response;
	GError *error;
	TrackerSparqlBuilder *sparql;
	FeedItem *item;
	FeedChannel *feed;
	TrackerMinerRSS *miner;
	gboolean has_geopoint;

	miner = TRACKER_MINER_RSS (source_object);
	response = tracker_miner_execute_sparql_finish (TRACKER_MINER (source_object), res, &error);

	if (response == NULL) {
		g_warning ("Unable to verify item: %s\n", error->message);
		g_error_free (error);
		return;
	}

	values = (gchar**) g_ptr_array_index (response, 0);
	if (strcmp (values [0], "1") == 0)
		return;

	item = data;

	subject = get_message_subject (item);

	sparql = tracker_sparql_builder_new_update ();

	has_geopoint = feed_item_get_geo_point (item, &latitude, &longitude);
	tracker_sparql_builder_insert_open (sparql, subject);

	if (has_geopoint) {
		tracker_sparql_builder_subject (sparql, "_:location");
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "mlo:GeoLocation");
		tracker_sparql_builder_predicate (sparql, "mlo:asGeoPoint");
		tracker_sparql_builder_object_blank_open (sparql);
		tracker_sparql_builder_predicate (sparql, "a");
		tracker_sparql_builder_object (sparql, "mlo:GeoPoint");
		tracker_sparql_builder_predicate (sparql, "mlo:latitude");
		tracker_sparql_builder_object_double (sparql, latitude);
		tracker_sparql_builder_predicate (sparql, "mlo:longitude");
		tracker_sparql_builder_object_double (sparql, longitude);
		tracker_sparql_builder_object_blank_close (sparql);
	}

	tracker_sparql_builder_subject (sparql, "_:message");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "mfo:FeedMessage");
	tracker_sparql_builder_predicate (sparql, "a");
	tracker_sparql_builder_object (sparql, "nfo:RemoteDataObject");

	if (has_geopoint == TRUE) {
		tracker_sparql_builder_predicate (sparql, "mlo:location");
		tracker_sparql_builder_object (sparql, "_:location");
	}

	tmp_string = feed_item_get_title (item);
	if (tmp_string != NULL) {
		tracker_sparql_builder_predicate (sparql, "nie:title");
		tracker_sparql_builder_object_string (sparql, tmp_string);
	}

	tmp_string = feed_item_get_description (item);
	if (tmp_string != NULL) {
		tracker_sparql_builder_predicate (sparql, "nmo:plainTextMessageContent");
		tracker_sparql_builder_object_string (sparql, tmp_string);
	}

	tmp_string = get_message_url (item);
	if (tmp_string != NULL) {
		tracker_sparql_builder_predicate (sparql, "nie:url");
		tracker_sparql_builder_object_string (sparql, tmp_string);
	}

	/*
		nmo:receivedDate and mfo:downloadedTime are the same? Ask for the MFO maintainer
	*/

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
	uri = (gchar*) g_object_get_data (G_OBJECT (feed), "subject");
	tracker_sparql_builder_predicate (sparql, "nmo:communicationChannel");
	tracker_sparql_builder_object_iri (sparql, uri);

	tracker_sparql_builder_insert_close (sparql);

	tracker_miner_execute_update (TRACKER_MINER (miner), tracker_sparql_builder_get_result (sparql), NULL, NULL, NULL);

	g_object_unref (sparql);
	g_free (subject);
}

static void
check_if_save (TrackerMinerRSS *miner, FeedItem *item)
{
	FeedChannel *feed;
	gchar *query;
	gchar *communication_channel;
	const gchar *url;

	url = get_message_url (item);
	feed = feed_item_get_parent (item);
	communication_channel = (gchar*) g_object_get_data (G_OBJECT (feed), "subject");

	query = g_strdup_printf ("ASK { ?message a mfo:FeedMessage; nie:url \"%s\"; nmo:communicationChannel <%s> }",
	                        url, communication_channel);
	tracker_miner_execute_sparql (TRACKER_MINER (miner), query, NULL, item_verify_reply_cb, item);
	g_free (query);
}

static void
feed_fetched (FeedsPool *pool, FeedChannel *feed, GList *items, gpointer user_data)
{
	gchar *uri;
	time_t now;
	GList *iter;
	FeedItem *item;
	TrackerMinerRSS *miner;

	miner = (TrackerMinerRSS*) user_data;

	miner->priv->now_fetching--;
	if (miner->priv->now_fetching <= 0) {
		miner->priv->now_fetching = 0;
		g_object_set (miner, "progress", 1.0, "status", "Idle", NULL);
	}

	if (items == NULL)
		return;

	now = time (NULL);
	uri = (gchar*) g_object_get_data (G_OBJECT (feed), "subject");
	update_updated_interval (miner, uri, &now);

	for (iter = items; iter; iter = g_list_next (iter)) {
		item = (FeedItem*) iter->data;
		check_if_save (miner, item);
	}
}

static void
feeds_retrieve_cb (GObject *source_object, GAsyncResult *res, gpointer data)
{
	int interval;
	register int i;
	gchar **values;
	GList *channels;
	const GPtrArray *response;
	GError *error;
	TrackerMinerRSS *miner;
	FeedChannel *chan;

	miner = TRACKER_MINER_RSS (source_object);
	response = tracker_miner_execute_sparql_finish (TRACKER_MINER (source_object), res, &error);

	if (response == NULL) {
		g_warning ("Unable to retrieve list of feeds: %s\n", error->message);
		g_error_free (error);
		return;
	}

	channels = NULL;

	for (i = 0; i < response->len; i++) {
		values = (gchar**) g_ptr_array_index (response, i);

		chan = feed_channel_new ();
		g_object_set_data_full (G_OBJECT (chan), "subject", g_strdup (values [2]), g_free);
		feed_channel_set_source (chan, values [0]);

		/*
			How to manage feeds with an update mfo:updateInterval == 0 ?
			Here the interval is forced to be at least 1 minute, but perhaps those
			elements are to be considered "disabled"
		*/
		interval = strtoull (values [1], NULL, 10);
		if (interval <= 0)
			interval = 1;
		feed_channel_set_update_interval (chan, interval);

		channels = g_list_prepend (channels, chan);
	}

	feeds_pool_listen (miner->priv->pool, channels);
}

static void
retrieve_and_schedule_feeds (TrackerMinerRSS *miner)
{
	gchar *sparql;

	sparql = g_strdup_printf ("SELECT ?chanUrl ?interval ?chanUrn WHERE		\
	                           { ?chanUrn a mfo:FeedChannel .			\
	                             ?chanUrn mfo:feedSettings ?settings .		\
	                             ?chanUrn nie:url ?chanUrl .			\
	                             ?settings mfo:updateInterval ?interval }");

	tracker_miner_execute_sparql (TRACKER_MINER (miner), sparql, NULL, feeds_retrieve_cb, NULL);
	g_free (sparql);
}

static gchar*
get_message_subject (FeedItem *item)
{
	return g_strdup_printf ("rss://%s", feed_item_get_id (item));
}

static const gchar*
get_message_url (FeedItem *item)
{
	const gchar *url;

	feed_item_get_real_source (item, &url, NULL);
	if (url == NULL)
		url = feed_item_get_source (item);
	return url;
}

static void
tracker_miner_rss_started (TrackerMiner *miner)
{
	TrackerMinerRSS *rss;

	g_object_set (miner, "progress", 0.0, "status", "Initializing", NULL);

	rss = TRACKER_MINER_RSS (miner);
	retrieve_and_schedule_feeds (rss);
	feeds_pool_switch (rss->priv->pool, TRUE);

	g_object_set (miner, "status", "Idle", NULL);
}

static void
tracker_miner_rss_stopped (TrackerMiner *miner)
{
	TrackerMinerRSS *rss;

	rss = TRACKER_MINER_RSS (miner);
	feeds_pool_switch (rss->priv->pool, FALSE);
	g_object_set (miner, "progress", 1.0, "status", "Idle", NULL);
}

static void
tracker_miner_rss_paused (TrackerMiner *miner)
{
	TrackerMinerRSS *rss;

	rss = TRACKER_MINER_RSS (miner);
	feeds_pool_switch (rss->priv->pool, FALSE);
	g_object_set (miner, "progress", 1.0, "status", "Paused", NULL);
}

static void
tracker_miner_rss_resumed (TrackerMiner *miner)
{
	TrackerMinerRSS *rss;

	rss = TRACKER_MINER_RSS (miner);
	feeds_pool_switch (rss->priv->pool, TRUE);
	g_object_set (miner, "progress", 1.0, "status", "Idle", NULL);
}
