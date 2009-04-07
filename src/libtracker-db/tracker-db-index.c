/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

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

#include <string.h>

#include <depot.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-db-index.h"

/* Size of free block pool of inverted index */
#define MAX_HIT_BUFFER 480000
#define MAX_CACHE_DEPTH 2
#define MAX_FLUSH_TIME 0.5 /* In fractions of a second */

#define TRACKER_DB_INDEX_ERROR_DOMAIN "TrackerDBIndex"

#define TRACKER_DB_INDEX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TRACKER_TYPE_DB_INDEX, TrackerDBIndexPrivate))

typedef struct TrackerDBIndexPrivate TrackerDBIndexPrivate;

struct TrackerDBIndexPrivate {
	/* From the daemon */
	DEPOT	   *index;

	guint	    min_bucket;
	guint	    max_bucket;

	guint	    reload : 1;
	guint	    readonly : 1;
	guint	    in_pause : 1;
	guint	    in_flush : 1;
	guint       overloaded : 1;

	/* Internal caches */
	guint       idle_flush_id;
	GList      *cache_layers;
	GHashTable *cur_cache;

	/* From the indexer */
	gchar	   *filename;
	gint	    bucket_count;
};

static void tracker_db_index_finalize	  (GObject	       *object);
static void tracker_db_index_set_property (GObject	       *object,
					   guint		prop_id,
					   const GValue        *value,
					   GParamSpec	       *pspec);
static void tracker_db_index_get_property (GObject	       *object,
					   guint		prop_id,
					   GValue	       *value,
					   GParamSpec	       *pspec);
static void free_cache_values		  (GArray	       *array);


enum {
	PROP_0,
	PROP_FILENAME,
	PROP_MIN_BUCKET,
	PROP_MAX_BUCKET,
	PROP_RELOAD,
	PROP_READONLY,
	PROP_FLUSHING,
	PROP_OVERLOADED
};

enum {
	ERROR_RECEIVED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (TrackerDBIndex, tracker_db_index, G_TYPE_OBJECT)

static void
tracker_db_index_class_init (TrackerDBIndexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_db_index_finalize;
	object_class->set_property = tracker_db_index_set_property;
	object_class->get_property = tracker_db_index_get_property;

	g_object_class_install_property (object_class,
					 PROP_FILENAME,
					 g_param_spec_string ("filename",
							      "Filename",
							      "Filename",
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_MIN_BUCKET,
					 g_param_spec_int ("min-bucket",
							   "Minimum bucket",
							   "Minimum bucket",
							   0,
							   1000000, /* FIXME MAX_GUINT ?? */
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MAX_BUCKET,
					 g_param_spec_int ("max-bucket",
							   "Maximum bucket",
							   "Maximum bucket",
							   0,
							   1000000, /* FIXME MAX_GUINT ?? */
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_RELOAD,
					 g_param_spec_boolean ("reload",
							       "Reload the index file before read",
							       "Reload the index file before read",
							       TRUE,
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_READONLY,
					 g_param_spec_boolean ("readonly",
							       "Open the index for readonly purposes",
							       "Open the index for readonly purposes",
							       TRUE,
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_FLUSHING,
					 g_param_spec_boolean ("flushing",
							       "Whether the index is currently being flushed",
							       "Whether the index is currently being flushed",
							       FALSE,
							       G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_OVERLOADED,
					 g_param_spec_boolean ("overloaded",
							       "Whether the index cache is overloaded",
							       "Whether the index cache is overloaded",
							       FALSE,
							       G_PARAM_READABLE));
	signals[ERROR_RECEIVED] =
		g_signal_new ("error-received",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TrackerDBIndexClass, error_received),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_type_class_add_private (object_class, sizeof (TrackerDBIndexPrivate));
}

static GHashTable *
index_cache_new (void)
{
	return g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      (GDestroyNotify) g_free,
				      (GDestroyNotify) free_cache_values);
}

static void
tracker_db_index_init (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	priv->reload = TRUE;
}

static void
tracker_db_index_finalize (GObject *object)
{
	TrackerDBIndex	      *indez;
	TrackerDBIndexPrivate *priv;

	indez = TRACKER_DB_INDEX (object);
	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (!priv->readonly) {
		tracker_db_index_open (indez);
		tracker_db_index_flush_sync (indez);
		tracker_db_index_close (indez);
	}

	if (priv->idle_flush_id) {
		g_source_remove (priv->idle_flush_id);
		priv->idle_flush_id = 0;
	}

	g_list_foreach (priv->cache_layers, (GFunc) g_hash_table_destroy, NULL);
	g_list_free (priv->cache_layers);

	if (priv->cur_cache) {
		g_hash_table_destroy (priv->cur_cache);
	}

	g_free (priv->filename);

	G_OBJECT_CLASS (tracker_db_index_parent_class)->finalize (object);
}

static void
tracker_db_index_set_property (GObject	    *object,
			       guint	     prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_FILENAME:
		tracker_db_index_set_filename (TRACKER_DB_INDEX (object),
					    g_value_get_string (value));
		break;
	case PROP_MIN_BUCKET:
		tracker_db_index_set_min_bucket (TRACKER_DB_INDEX (object),
					      g_value_get_int (value));
		break;
	case PROP_MAX_BUCKET:
		tracker_db_index_set_max_bucket (TRACKER_DB_INDEX (object),
					      g_value_get_int (value));
		break;
	case PROP_RELOAD:
		tracker_db_index_set_reload (TRACKER_DB_INDEX (object),
					  g_value_get_boolean (value));
		break;
	case PROP_READONLY:
		tracker_db_index_set_readonly (TRACKER_DB_INDEX (object),
					       g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_db_index_get_property (GObject	  *object,
			       guint	   prop_id,
			       GValue	  *value,
			       GParamSpec *pspec)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_MIN_BUCKET:
		g_value_set_int (value, priv->min_bucket);
		break;
	case PROP_MAX_BUCKET:
		g_value_set_int (value, priv->max_bucket);
		break;
	case PROP_RELOAD:
		g_value_set_boolean (value, priv->reload);
		break;
	case PROP_READONLY:
		g_value_set_boolean (value, priv->readonly);
		break;
	case PROP_FLUSHING:
		g_value_set_boolean (value, priv->in_flush);
		break;
	case PROP_OVERLOADED:
		g_value_set_boolean (value, priv->overloaded);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
free_cache_values (GArray *array)
{
	g_array_free (array, TRUE);
}

TrackerDBIndex *
tracker_db_index_new (const gchar *filename,
		      gint	   min_bucket,
		      gint	   max_bucket,
		      gboolean	   readonly)
{
	TrackerDBIndex *indez;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (min_bucket > 0, NULL);
	g_return_val_if_fail (min_bucket < max_bucket, NULL);

	indez = g_object_new (TRACKER_TYPE_DB_INDEX,
			      "filename", filename,
			      "min-bucket", min_bucket,
			      "max-bucket", max_bucket,
			      "readonly", readonly,
			      NULL);

	tracker_db_index_open (indez);

	return indez;
}

void
tracker_db_index_set_filename (TrackerDBIndex *indez,
			       const gchar    *filename)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (priv->filename) {
		g_free (priv->filename);
	}

	priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (indez), "filename");
}

void
tracker_db_index_set_min_bucket (TrackerDBIndex *indez,
				 gint		 min_bucket)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	priv->min_bucket = min_bucket;

	g_object_notify (G_OBJECT (indez), "min-bucket");
}

void
tracker_db_index_set_max_bucket (TrackerDBIndex *indez,
				 gint		 max_bucket)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	priv->max_bucket = max_bucket;

	g_object_notify (G_OBJECT (indez), "max-bucket");
}

void
tracker_db_index_set_reload (TrackerDBIndex *indez,
			     gboolean	     reload)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	priv->reload = reload;

	g_object_notify (G_OBJECT (indez), "reload");
}

void
tracker_db_index_set_readonly (TrackerDBIndex *indez,
			       gboolean        readonly)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	priv->readonly = readonly;

	g_object_notify (G_OBJECT (indez), "readonly");
}

gboolean
tracker_db_index_get_reload (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	return priv->reload;
}

gboolean
tracker_db_index_get_readonly (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), TRUE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	return priv->readonly;
}

static inline gboolean
has_word (TrackerDBIndex *indez,
	  const gchar	 *word)
{
	TrackerDBIndexPrivate *priv;
	gchar		       buffer[32];
	gint		       count;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	count = dpgetwb (priv->index, word, -1, 0, 32, buffer);

	return count > 7;
}

static inline gint
count_hit_size_for_word (TrackerDBIndex *indez,
			 const gchar	*word)
{
	TrackerDBIndexPrivate *priv;
	gint		     tsiz;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	tsiz = dpvsiz (priv->index, word, -1);

	return tsiz;
}

/* int levenshtein ()
 * Original license: GNU Lesser Public License
 * from the Dixit project, (http://dixit.sourceforge.net/)
 * Author: Octavian Procopiuc <oprocopiuc@gmail.com>
 * Created: July 25, 2004
 * Copied into tracker, by Edward Duffy
 */
static gint
levenshtein (const gchar *source,
	     gchar	 *target,
	     gint	  maxdist)
{
	gchar n, m;
	gint  l;
	gchar mincolval;
	gchar matrix[51][51];
	gchar j;
	gchar i;
	gchar cell;

	l = strlen (source);
	if (l > 50)
		return -1;
	n = l;

	l = strlen (target);
	if (l > 50)
		return -1;
	m = l;

	if (maxdist == 0)
		maxdist = MAX(m, n);
	if (n == 0)
		return MIN(m, maxdist);
	if (m == 0)
		return MIN(n, maxdist);

	/* Store the min. value on each column, so that, if it
	 * reaches. maxdist, we break early.
	 */
	for (j = 0; j <= m; j++)
		matrix[0][(gint)j] = j;

	for (i = 1; i <= n; i++) {
		gchar s_i;

		mincolval = MAX(m, i);
		matrix[(gint)i][0] = i;

		s_i = source[i-1];

		for (j = 1; j <= m; j++) {
			gchar t_j = target[j-1];
			gchar cost = (s_i == t_j ? 0 : 1);
			gchar above = matrix[i-1][(gint)j];
			gchar left = matrix[(gint)i][j-1];
			gchar diag = matrix[i-1][j-1];

			cell = MIN(above + 1, MIN(left + 1, diag + cost));

			/* Cover transposition, in addition to deletion,
			 * insertion and substitution. This step is taken from:
			 * Berghel, Hal ; Roach, David : "An Extension of Ukkonen's
			 * Enhanced Dynamic Programming ASM Algorithm"
			 * (http://www.acm.org/~hlb/publications/asm/asm.html)
			 */
			if (i > 2 && j > 2) {
				gchar trans = matrix[i-2][j-2] + 1;

				if (source[i-2] != t_j)
					trans++;
				if (s_i != target[j-2])
					trans++;
				if (cell > trans)
					cell = trans;
			}

			mincolval = MIN(mincolval, cell);
			matrix[(gint)i][(gint)j] = cell;
		}

		if (mincolval >= maxdist)
			break;
	}

	if (i == n + 1) {
		return (gint) matrix[(gint)n][(gint)m];
	} else {
		return maxdist;
	}
}

static gint
count_hits_for_word (TrackerDBIndex *indez,
		     const gchar    *str)
{

	gint tsiz;
	gint hits = 0;

	tsiz = count_hit_size_for_word (indez, str);

	if (tsiz == -1 ||
	    tsiz % sizeof (TrackerDBIndexItem) != 0) {
		return -1;
	}

	hits = tsiz / sizeof (TrackerDBIndexItem);

	return hits;
}

static gboolean
check_index_is_up_to_date (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (priv->reload) {
		g_message ("Reloading index:'%s'", priv->filename);
		tracker_db_index_close (indez);
	}

	if (!priv->index) {
		tracker_db_index_open (indez);
	}

	return !priv->reload;
}

static void
update_overloaded_status (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;
	gboolean overloaded;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	overloaded = g_list_length (priv->cache_layers) > MAX_CACHE_DEPTH;

	if (priv->overloaded != overloaded) {
		priv->overloaded = overloaded;
		g_object_notify (G_OBJECT (indez), "overloaded");
	}
}

static void
emit_error_received (TrackerDBIndex *indez,
		     const gchar    *error_str)
{
	GQuark domain;
	GError *error;

	domain = g_quark_from_static_string (TRACKER_DB_INDEX_ERROR_DOMAIN);

	error = g_error_new_literal (domain, 0, error_str);
	g_signal_emit (indez, signals[ERROR_RECEIVED], 0, error);
	g_error_free (error);
}

/* Use for deletes or updates of multiple entities when they are not
 * new.
 */
static gboolean
indexer_update_word (const gchar *word,
		     GArray	 *new_hits,
		     DEPOT	 *indez)
{
	TrackerDBIndexItem *new_hit;
	TrackerDBIndexItem *previous_hits;
	GArray		   *pending_hits;
	gboolean	    write_back;
	gboolean	    edited;
	gboolean	    result;
	gint		    old_hit_count;
	gint		    score;
	gint		    tsiz;
	guint		    j;

	g_return_val_if_fail (indez != NULL, FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (new_hits != NULL, FALSE);

	write_back = FALSE;
	edited = FALSE;
	old_hit_count = 0;
	pending_hits = NULL;

	previous_hits = (TrackerDBIndexItem *) dpget (indez,
						      word,
						      -1,
						      0,
						      MAX_HIT_BUFFER,
						      &tsiz);

	/* New word in the index */
	if (previous_hits == NULL) {
		pending_hits = g_array_new (FALSE, TRUE, sizeof (TrackerDBIndexItem));
		result = TRUE;

		/* Ensure weights are correct before inserting */
		for (j = 0; j < new_hits->len; j++) {
			new_hit = &g_array_index (new_hits, TrackerDBIndexItem, j);
			score = tracker_db_index_item_get_score (new_hit);

			if (score > 0) {
				g_array_append_val (pending_hits, *new_hit);
			}
		}

		if (pending_hits->len > 0) {
			result = dpput (indez,
					word, -1,
					(char *) pending_hits->data,
					pending_hits->len * sizeof (TrackerDBIndexItem),
					DP_DOVER);

			if (!result) {
				g_warning ("Could not store word '%s': %s", word, dperrmsg (dpecode));
			}
		}

		g_array_free (pending_hits, TRUE);

		return result;
	}

	/* Word already exists */
	old_hit_count = tsiz / sizeof (TrackerDBIndexItem);

	for (j = 0; j < new_hits->len; j++) {
		gint left, right, center;

		new_hit = &g_array_index (new_hits, TrackerDBIndexItem, j);
		edited = FALSE;

		left = 0;
		right = old_hit_count - 1;
		center = (right - left) / 2;

		/* New items are going to have always a higher service ID,
		 * so the insertion is sorted, perform a binary search.
		 */

		do {
			center += left;

			if (new_hit->id > previous_hits[center].id) {
				left = center + 1;
			} else if (new_hit->id < previous_hits[center].id) {
				right = center - 1;
			} else if (new_hit->id == previous_hits[center].id) {
				write_back = TRUE;

				/* NB the paramter score can be negative */
				score =  tracker_db_index_item_get_score (&previous_hits[center]);
				score += tracker_db_index_item_get_score (new_hit);


				/* Check for deletion */
				if (score < 1) {
					/* Shift all subsequent records in array down one place */
					g_memmove (&previous_hits[center], &previous_hits[center + 1],
						   (old_hit_count - center - 1) * sizeof (TrackerDBIndexItem));
					old_hit_count--;
				} else {
					guint32 service_type;

					service_type =
						tracker_db_index_item_get_service_type (&previous_hits[center]);
					previous_hits[center].amalgamated =
						tracker_db_index_item_calc_amalgamated (service_type,
											score);
				}

				edited = TRUE;
				break;
			}

			center = (right - left) / 2;
		} while (left <= right);

		/* Add hits that could not be updated directly here so
		 * they can be appended later
		 */
		if (!edited) {
			if (!pending_hits) {
				pending_hits = g_array_new (FALSE,
							    TRUE,
							    sizeof (TrackerDBIndexItem));
			}

			g_array_append_val (pending_hits, *new_hit);
		}
	}

	/* Write back if we have modded anything */
	if (write_back) {
		/* If the word has no hits, remove it! Otherwise
		 * overwrite the value with the new hits array
		 */
		if (old_hit_count < 1) {
			result = dpout (indez, word, -1);
		} else {
			result = dpput (indez,
					word, -1,
					(char *) previous_hits,
					old_hit_count * sizeof (TrackerDBIndexItem),
					DP_DOVER);
		}

		if (!result) {
			g_warning ("Could not modify word '%s': %s", word, dperrmsg (dpecode));
		}
	}

	/*  Append new occurences */
	if (pending_hits) {
		result = dpput (indez,
				word, -1,
				(char*) pending_hits->data,
				pending_hits->len * sizeof (TrackerDBIndexItem),
				DP_DCAT);
		g_array_free (pending_hits, TRUE);

		if (!result) {
			g_warning ("Could not insert pending word '%s': %s", word, dperrmsg (dpecode));
		}
	}

	g_free (previous_hits);

	return TRUE;
}

static void
set_in_flush (TrackerDBIndex *indez,
	      gboolean        in_flush)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (in_flush != priv->in_flush) {
		priv->in_flush = in_flush;
		g_object_notify (G_OBJECT (indez), "flushing");
	}
}

static gboolean
index_flush_item (gpointer user_data)
{
	TrackerDBIndex *indez;
	TrackerDBIndexPrivate *priv;
	GHashTableIter iter;
	gpointer key, value;

	indez = TRACKER_DB_INDEX (user_data);
	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (priv->in_pause || !priv->index) {
		g_debug ("Flushing was paused or index was closed, waiting...");
		priv->idle_flush_id = 0;
		return FALSE;
	}

	if (priv->cache_layers && g_hash_table_size (priv->cache_layers->data) > 0) {
		GTimer *timer;

		timer = g_timer_new ();
		g_hash_table_iter_init (&iter, (GHashTable *) priv->cache_layers->data);

		while (g_hash_table_iter_next (&iter, &key, &value)) {
			/* Process words from cache */
			if (indexer_update_word (key, value, priv->index)) {
				g_hash_table_iter_remove (&iter);
			} else {
				emit_error_received (indez, _("Index corrupted"));
				break;
			}

			if (g_timer_elapsed (timer, NULL) > MAX_FLUSH_TIME) {
				break;
			}
		}

		g_timer_destroy (timer);

		return TRUE;
	} else {
		GList *link;

		if (priv->cache_layers) {
			/* Current cache being flushed is already empty, proceed with the next one */
			link = priv->cache_layers;
			priv->cache_layers = g_list_remove_link (priv->cache_layers, link);
			g_hash_table_destroy (link->data);
			g_list_free_1 (link);

			update_overloaded_status (indez);
		}

		if (priv->cache_layers) {
			g_debug ("Flushing next batch (%d words) to index...",
				 g_hash_table_size (priv->cache_layers->data));
			return TRUE;
		} else {
			g_debug ("Finished flushing elements to index");

			set_in_flush (indez, FALSE);
			priv->idle_flush_id = 0;

			return FALSE;
		}
	}

	return TRUE;
}

static void
init_flush (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (priv->in_pause) {
		g_debug ("Index was paused, waiting for it being resumed...");
		return;
	}

	if (!priv->index) {
		g_debug ("Index was not open for flush, waiting...");
		return;
	}

	if (priv->idle_flush_id == 0) {
		priv->idle_flush_id = g_idle_add (index_flush_item, indez);
	}
}

gboolean
tracker_db_index_open (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;
	gint		       flags;
	gint		       bucket_count;
	gint		       rec_count;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	g_return_val_if_fail (priv->filename != NULL, FALSE);

	if (priv->index) {
		return TRUE;
	}

	g_debug ("Opening index:'%s' (%s)",
		 priv->filename,
		 priv->readonly ? "readonly" : "read/write");

	if (priv->readonly) {
		flags = DP_OREADER | DP_ONOLCK;
	} else {
		flags = DP_OWRITER | DP_OCREAT | DP_ONOLCK;
	}

	priv->index = dpopen (priv->filename,
			      flags,
			      priv->max_bucket);

	if (!priv->index) {
		if (!g_file_test (priv->filename, G_FILE_TEST_EXISTS)) {
			g_debug ("Index doesnt exists yet:'%s'",
				 priv->filename);
		} else {
			g_debug ("Index was not closed properly:'%s', %s",
				 priv->filename,
				 dperrmsg (dpecode));

			if (dprepair (priv->filename)) {
				priv->index = dpopen (priv->filename,
						      flags,
						      priv->max_bucket);
			} else {
				g_critical ("Corrupted index file %s.",
					    priv->filename);
			}
		}
	}

	if (priv->index) {
		dpsetalign (priv->index, 8);

		/* Reoptimize database if bucket count < rec count */
		bucket_count = dpbnum (priv->index);
		rec_count = dprnum (priv->index);

		g_debug ("Bucket count (max is %d) is %d and record count is %d",
			 priv->max_bucket,
			 bucket_count,
			 rec_count);

		priv->reload = FALSE;

		if (priv->in_flush) {
			g_debug ("Resuming flushing...");
			init_flush (indez);
		}
	} else {
		priv->reload = TRUE;
	}

	return !priv->reload;
}

gboolean
tracker_db_index_close (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;
	gboolean	       retval;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	retval = TRUE;

	if (priv->index) {
		g_debug ("Closing index:'%s'", priv->filename);

		if (!dpclose (priv->index)) {
			g_message ("Could not close index, %s",
				   dperrmsg (dpecode));
			retval = FALSE;
		}

		priv->index = NULL;
	}

	return retval;
}

void
tracker_db_index_set_paused (TrackerDBIndex *indez,
			     gboolean	     paused)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (!priv->in_pause && paused) {
		priv->in_pause = paused;
		tracker_db_index_close (indez);
	} else if (priv->in_pause && !paused) {
		priv->in_pause = paused;
		tracker_db_index_open (indez);
	}
}

void
tracker_db_index_flush (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (!priv->in_flush) {
		set_in_flush (indez, TRUE);
	}

	if (priv->cur_cache && g_hash_table_size (priv->cur_cache) > 0) {
		g_debug ("Pushing a new batch (%d words) to be flushed to index...",
			 g_hash_table_size (priv->cur_cache));

		/* Put current cache into the queue and create a
		 * new one for keeping appending words
		 */
		priv->cache_layers = g_list_append (priv->cache_layers, priv->cur_cache);
		priv->cur_cache = index_cache_new ();

		update_overloaded_status (indez);
	}

	init_flush (indez);
}

void
tracker_db_index_flush_sync (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;
	GList *cache;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (priv->idle_flush_id) {
		g_source_remove (priv->idle_flush_id);
		priv->idle_flush_id = 0;
	}

	set_in_flush (indez, TRUE);

	if (priv->cur_cache && g_hash_table_size (priv->cur_cache) > 0) {
		priv->cache_layers = g_list_append (priv->cache_layers, priv->cur_cache);
		priv->cur_cache = NULL;
	}

	for (cache = priv->cache_layers; cache; cache = cache->next) {
		g_hash_table_foreach_remove (cache->data,
					     (GHRFunc) indexer_update_word,
					     priv->index);
	}

	g_list_foreach (priv->cache_layers, (GFunc) g_hash_table_destroy, NULL);
	g_list_free (priv->cache_layers);
	priv->cache_layers = NULL;

	set_in_flush (indez, FALSE);
	update_overloaded_status (indez);
}

guint32
tracker_db_index_get_size (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;
	guint32		       size;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), 0);

	if (!check_index_is_up_to_date (indez)) {
		return 0;
	}

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	size = dpfsiz (priv->index);

	return size;
}

gchar *
tracker_db_index_get_suggestion (TrackerDBIndex *indez,
				 const gchar	*term,
				 gint		 maxdist)
{
	TrackerDBIndexPrivate *priv;
	gchar		    *str;
	gint		     dist;
	gchar		    *winner_str;
	gint		     winner_dist;
	gint		     hits;
	GTimeVal	     start, current;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), NULL);
	g_return_val_if_fail (term != NULL, NULL);
	g_return_val_if_fail (maxdist >= 0, NULL);

	if (!check_index_is_up_to_date (indez)) {
		return NULL;
	}

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	winner_str = g_strdup (term);
	winner_dist = G_MAXINT;  /* Initialize to the worst case */

	dpiterinit (priv->index);

	g_get_current_time (&start);

	str = dpiternext (priv->index, NULL);

	while (str != NULL) {
		dist = levenshtein (term, str, 0);

		if (dist != -1 &&
		    dist < maxdist &&
		    dist < winner_dist) {
			hits = count_hits_for_word (indez, str);

			if (hits < 0) {
				g_free (winner_str);
				g_free (str);

				return NULL;
			} else if (hits > 0) {
				g_free (winner_str);
				winner_str = g_strdup (str);
				winner_dist = dist;
			} else {
				g_message ("No hits for:'%s'!", str);
			}
		}

		g_free (str);

		g_get_current_time (&current);

		/* 2 second time out */
		if (current.tv_sec - start.tv_sec >= 2) {
			g_message ("Timed out in %s, not collecting more suggestions.",
				   __FUNCTION__);
			break;
		}

		str = dpiternext (priv->index, NULL);
	}

	return winner_str;
}

TrackerDBIndexItem *
tracker_db_index_get_word_hits (TrackerDBIndex *indez,
				const gchar    *word,
				guint	       *count)
{
	TrackerDBIndexPrivate *priv;
	TrackerDBIndexItem    *details;
	gint		       tsiz;
	gchar		      *tmp;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), NULL);
	g_return_val_if_fail (word != NULL, NULL);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	if (!check_index_is_up_to_date (indez)) {
		return NULL;
	}

	details = NULL;

	if (count) {
		*count = 0;
	}

	if ((tmp = dpget (priv->index, word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {
		if (tsiz >= sizeof (TrackerDBIndexItem)) {
			details = (TrackerDBIndexItem *) tmp;

			if (count) {
				*count = tsiz / sizeof (TrackerDBIndexItem);
			}
		}
	}

	return details;
}

void
tracker_db_index_add_word (TrackerDBIndex *indez,
			   const gchar	  *word,
			   guint32	   service_id,
			   gint		   service_type,
			   gint		   weight)
{
	TrackerDBIndexPrivate *priv;
	TrackerDBIndexItem     elem;
	TrackerDBIndexItem    *current;
	GArray		      *array;
	guint		       i, new_score;

	g_return_if_fail (TRACKER_IS_DB_INDEX (indez));
	g_return_if_fail (word != NULL);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	g_return_if_fail (priv->readonly == FALSE);

	if (G_UNLIKELY (!priv->cur_cache)) {
		priv->cur_cache = index_cache_new ();
	}

	elem.id = service_id;
	elem.amalgamated = tracker_db_index_item_calc_amalgamated (service_type, weight);

	array = g_hash_table_lookup (priv->cur_cache, word);

	if (!array) {
		/* Create the array if it didn't exist (first time we
		 * find the word)
		 */
		array = g_array_new (FALSE, TRUE, sizeof (TrackerDBIndexItem));
		g_hash_table_insert (priv->cur_cache, g_strdup (word), array);
		g_array_append_val (array, elem);

		return;
	}

	/* It is not the first time we find the word */
	for (i = 0; i < array->len; i++) {
		current = &g_array_index (array, TrackerDBIndexItem, i);

		if (current->id == service_id) {
			guint32 serv_type;

			/* The word was already found in the same
			 * service_id (file), modify score
			 */
			new_score = tracker_db_index_item_get_score (current) + weight;

			serv_type = tracker_db_index_item_get_service_type (current);
			current->amalgamated = tracker_db_index_item_calc_amalgamated (serv_type, new_score);

			return;
		}
	}

	/* First time in the file */
	g_array_append_val (array, elem);
}

gboolean
tracker_db_index_get_flushing (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	return priv->in_flush;
}

gboolean
tracker_db_index_get_overloaded (TrackerDBIndex *indez)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (indez), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (indez);

	return priv->overloaded;
}
