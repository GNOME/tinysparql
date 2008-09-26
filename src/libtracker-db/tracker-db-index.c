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
#include <glib/gstdio.h>

#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-db/tracker-db-manager.h>

#include "tracker-db-index.h"

/* Size of free block pool of inverted index */
#define MAX_HIT_BUFFER 480000

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

	/* From the indexer */
	GHashTable *cache;
	gchar	   *filename;
	gint	    bucket_count;
};

static void tracker_db_index_class_init   (TrackerDBIndexClass *class);
static void tracker_db_index_init	  (TrackerDBIndex      *tree);
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
	PROP_READONLY
};

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

	g_type_class_add_private (object_class, sizeof (TrackerDBIndexPrivate));
}

static void
tracker_db_index_init (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	priv->reload = TRUE;

	priv->cache = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     (GDestroyNotify) g_free,
					     (GDestroyNotify) free_cache_values);
}

static void
tracker_db_index_finalize (GObject *object)
{
	TrackerDBIndex	      *index;
	TrackerDBIndexPrivate *priv;

	index = TRACKER_DB_INDEX (object);
	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	tracker_db_index_flush (index);
	tracker_db_index_close (index);

	g_hash_table_destroy (priv->cache);

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
	TrackerDBIndex *index;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (min_bucket > 0, NULL);
	g_return_val_if_fail (min_bucket < max_bucket, NULL);

	index = g_object_new (TRACKER_TYPE_DB_INDEX,
			      "filename", filename,
			      "min-bucket", min_bucket,
			      "max-bucket", max_bucket,
			      "readonly", readonly,
			      NULL);

	tracker_db_index_open (index);

	return index;
}

void
tracker_db_index_set_filename (TrackerDBIndex *index,
			       const gchar    *filename)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	if (priv->filename) {
		g_free (priv->filename);
	}

	priv->filename = g_strdup (filename);

	g_object_notify (G_OBJECT (index), "filename");
}

void
tracker_db_index_set_min_bucket (TrackerDBIndex *index,
				 gint		 min_bucket)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	priv->min_bucket = min_bucket;

	g_object_notify (G_OBJECT (index), "min-bucket");
}

void
tracker_db_index_set_max_bucket (TrackerDBIndex *index,
				 gint		 max_bucket)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	priv->max_bucket = max_bucket;

	g_object_notify (G_OBJECT (index), "max-bucket");
}

void
tracker_db_index_set_reload (TrackerDBIndex *index,
			     gboolean	     reload)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	priv->reload = reload;

	g_object_notify (G_OBJECT (index), "reload");
}

void
tracker_db_index_set_readonly (TrackerDBIndex *index,
			       gboolean        readonly)
{
	TrackerDBIndexPrivate *priv;

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	priv->readonly = readonly;

	g_object_notify (G_OBJECT (index), "readonly");
}

gboolean
tracker_db_index_get_reload (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	return priv->reload;
}

gboolean
tracker_db_index_get_readonly (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), TRUE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	return priv->readonly;
}

static inline gboolean
has_word (TrackerDBIndex *index,
	  const gchar	 *word)
{
	TrackerDBIndexPrivate *priv;
	gchar		       buffer[32];
	gint		       count;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	count = dpgetwb (priv->index, word, -1, 0, 32, buffer);

	return count > 7;
}

static inline gint
count_hit_size_for_word (TrackerDBIndex *index,
			 const gchar	*word)
{
	TrackerDBIndexPrivate *priv;
	gint		     tsiz;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

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
count_hits_for_word (TrackerDBIndex *index,
		     const gchar    *str)
{

	gint tsiz;
	gint hits = 0;

	tsiz = count_hit_size_for_word (index, str);

	if (tsiz == -1 ||
	    tsiz % sizeof (TrackerDBIndexItem) != 0) {
		return -1;
	}

	hits = tsiz / sizeof (TrackerDBIndexItem);

	return hits;
}

static gboolean
check_index_is_up_to_date (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	if (priv->reload) {
		g_message ("Reloading index:'%s'", priv->filename);
		tracker_db_index_close (index);
	}

	if (!priv->index) {
		tracker_db_index_open (index);
	}

	return !priv->reload;
}

/* Use for deletes or updates of multiple entities when they are not
 * new.
 */
static gboolean
indexer_update_word (DEPOT	 *index,
		     const gchar *word,
		     GArray	 *new_hits)
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

	g_return_val_if_fail (index != NULL, FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (new_hits != NULL, FALSE);

	write_back = FALSE;
	edited = FALSE;
	old_hit_count = 0;
	pending_hits = NULL;

	previous_hits = (TrackerDBIndexItem *) dpget (index,
						      word,
						      -1,
						      0,
						      MAX_HIT_BUFFER,
						      &tsiz);

	/* New word in the index */
	if (previous_hits == NULL) {
		result = dpput (index,
				word, -1,
				(char *) new_hits->data,
				new_hits->len * sizeof (TrackerDBIndexItem),
				DP_DOVER);

		if (!result) {
			g_warning ("Could not store word '%s': %s", word, dperrmsg (dpecode));
			return FALSE;
		}

		return TRUE;
	}

	/* Word already exists */
	old_hit_count = tsiz / sizeof (TrackerDBIndexItem);

	for (j = 0; j < new_hits->len; j++) {
		guint left, right, center;

		new_hit = &g_array_index (new_hits, TrackerDBIndexItem, j);
		edited = FALSE;

		left = 0;
		right = old_hit_count;
		center = (right - left) / 2;

		/* New items are going to have always a higher service ID,
		 * so the insertion is sorted, perform a binary search.
		 */

		do {
			center += left;

			if (new_hit->id > previous_hits[center].id) {
				left = center;
			} else if (new_hit->id < previous_hits[center].id) {
				right = center;
			} else if (new_hit->id == previous_hits[center].id) {
				write_back = TRUE;

				/* NB the paramter score can be negative */
				score =  tracker_db_index_item_get_score (&previous_hits[center]);
				score += tracker_db_index_item_get_score (new_hit);


				/* Check for deletion */
				if (score < 1) {
					/* Shift all subsequent records in array down one place */
					g_memmove (&previous_hits[center], &previous_hits[center + 1],
						   (old_hit_count - center) * sizeof (TrackerDBIndexItem));
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
		} while (center > 0);

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
			result = dpout (index, word, -1);
		} else {
			result = dpput (index,
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
		result = dpput (index,
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

static gboolean
cache_flush_item (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	GArray *array;
	DEPOT  *index;
	gchar  *word;

	word = (gchar *) key;
	array = (GArray *) value;
	index = (DEPOT *) user_data;

	/* Mark element for removal if succesfull insertion */

	/**
	 * FIXME:
	 *
	 * Not removing the word from the memory-queue is not a good solution.
	 * That's because the only thing we'll achieve is letting this queue
	 * grow until it starts succeeding again. Which might end up being
	 * never. Making tracker-indexer both becoming increasingly slow and
	 * start consuming increasing amounts of memory.
	 **/

	return indexer_update_word (index, word, array);
}

gboolean
tracker_db_index_open (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;
	gint		       flags;
	gint		       bucket_count;
	gint		       rec_count;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

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
	} else {
		priv->reload = TRUE;
	}

	return !priv->reload;
}

gboolean
tracker_db_index_close (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;
	gboolean	       retval;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), FALSE);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

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
tracker_db_index_set_paused (TrackerDBIndex *index,
			     gboolean	     paused)
{
	TrackerDBIndexPrivate *priv;

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	if (!priv->in_pause && paused) {
		priv->in_pause = paused;
		tracker_db_index_close (index);
	} else if (priv->in_pause && !paused) {
		priv->in_pause = paused;
		tracker_db_index_open (index);
	}
}

guint
tracker_db_index_flush (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;
	guint		       size, removed_items;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), 0);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	if (priv->in_flush) {
		g_debug ("Index was already in the middle of a flush");
		return 0;
	}

	if (!priv->index) {
		g_debug ("Index was not open for flush, waiting...");
		return 0;
	}

	priv->in_flush = TRUE;
	size = g_hash_table_size (priv->cache);
	removed_items = 0;

	if (size > 0) {
		GList *keys, *k;
		gpointer value;

		g_debug ("Flushing index with %d items in cache", size);

		keys = g_hash_table_get_keys (priv->cache);

		for (k = keys; k; k = k->next) {
			value = g_hash_table_lookup (priv->cache, k->data);

			if (cache_flush_item (k->data, value, priv->index)) {
				g_hash_table_remove (priv->cache, k->data);
				removed_items++;
			}

			g_main_context_iteration (NULL, FALSE);
		}

		g_list_free (keys);
	}

	priv->in_flush = FALSE;

	return removed_items;
}

guint32
tracker_db_index_get_size (TrackerDBIndex *index)
{
	TrackerDBIndexPrivate *priv;
	guint32		       size;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), 0);

	if (!check_index_is_up_to_date (index)) {
		return 0;
	}

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	size = dpfsiz (priv->index);

	return size;
}

gchar *
tracker_db_index_get_suggestion (TrackerDBIndex *index,
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

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), NULL);
	g_return_val_if_fail (term != NULL, NULL);
	g_return_val_if_fail (maxdist >= 0, NULL);

	if (!check_index_is_up_to_date (index)) {
		return NULL;
	}

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

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
			hits = count_hits_for_word (index, str);

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
tracker_db_index_get_word_hits (TrackerDBIndex *index,
				const gchar    *word,
				guint	       *count)
{
	TrackerDBIndexPrivate *priv;
	TrackerDBIndexItem    *details;
	gint		       tsiz;
	gchar		      *tmp;

	g_return_val_if_fail (TRACKER_IS_DB_INDEX (index), NULL);
	g_return_val_if_fail (word != NULL, NULL);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	if (!check_index_is_up_to_date (index)) {
		return NULL;
	}


	details = NULL;

	if (count) {
		*count = 0;
	}

	if ((tmp = dpget (priv->index, word, -1, 0, MAX_HIT_BUFFER, &tsiz)) != NULL) {
		if (tsiz >= (gint) sizeof (TrackerDBIndexItem)) {
			details = (TrackerDBIndexItem *) tmp;

			if (count) {
				*count = tsiz / sizeof (TrackerDBIndexItem);
			}
		}
	}


	return details;
}

void
tracker_db_index_add_word (TrackerDBIndex *index,
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

	g_return_if_fail (TRACKER_IS_DB_INDEX (index));
	g_return_if_fail (word != NULL);

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	g_return_if_fail (priv->in_flush == FALSE);

	elem.id = service_id;
	elem.amalgamated = tracker_db_index_item_calc_amalgamated (service_type, weight);

	array = g_hash_table_lookup (priv->cache, word);

	if (!array) {
		/* Create the array if it didn't exist (first time we
		 * find the word)
		 */
		array = g_array_new (FALSE, TRUE, sizeof (TrackerDBIndexItem));
		g_hash_table_insert (priv->cache, g_strdup (word), array);
		g_array_append_val (array, elem);

		return;
	}

	/* It is not the first time we find the word */
	for (i = 0; i < array->len; i++) {
		current = &g_array_index (array, TrackerDBIndexItem, i);

		if (current->id == service_id) {
			/* The word was already found in the same
			 * service_id (file), increase score
			 */
			new_score = tracker_db_index_item_get_score (current) + weight;
			if (new_score < 1) {
				array = g_array_remove_index (array, i);
				if (array->len == 0) {
					g_hash_table_remove (priv->cache, word);
				}
			} else {
				guint32 service_type;

				service_type =
					tracker_db_index_item_get_service_type (current);
				current->amalgamated =
					tracker_db_index_item_calc_amalgamated (service_type,
										new_score);
			}


			return;
		}
	}

	/* First time in the file */
	g_array_append_val (array, elem);

}

/*
 * UNUSED
 *
 *  Use to delete dud hits for a word - dud_list is a list of
 * TrackerSearchHit structs.
 */
gboolean
tracker_db_index_remove_dud_hits (TrackerDBIndex *index,
				  const gchar	 *word,
				  GSList	 *dud_list)
{
	TrackerDBIndexPrivate *priv;
	gchar		      *tmp;
	gint		       tsiz;
	gboolean	       retval = FALSE;

	g_return_val_if_fail (index, FALSE);
	g_return_val_if_fail (word, FALSE);
	g_return_val_if_fail (dud_list, FALSE);

	if (!check_index_is_up_to_date (index)) {
		return TRUE;
	}

	priv = TRACKER_DB_INDEX_GET_PRIVATE (index);

	g_return_val_if_fail (priv->index, FALSE);


	/* Check if existing record is there  */
	tmp = dpget (priv->index,
		     word,
		     -1,
		     0,
		     MAX_HIT_BUFFER,
		     &tsiz);

	if (!tmp) {
		return FALSE;
	}

	if (tsiz >= (int) sizeof (TrackerDBIndexItem)) {
		TrackerDBIndexItem *details;
		gint		    wi, i, pnum;

		details = (TrackerDBIndexItem *) tmp;
		pnum = tsiz / sizeof (TrackerDBIndexItem);
		wi = 0;

		for (i = 0; i < pnum; i++) {
			GSList *lst;

			for (lst = dud_list; lst; lst = lst->next) {
				TrackerDBIndexItemRank *rank = lst->data;

				if (!rank) {
					continue;
				}

				if (details[i].id == rank->service_id) {
					gint k;

					/* Shift all subsequent
					 * records in array down one
					 * place.
					 */
					for (k = i + 1; k < pnum; k++) {
						details[k - 1] = details[k];
					}

					/* Make size of array one size
					 * smaller.
					 */
					tsiz -= sizeof (TrackerDBIndexItem);
					pnum--;

					break;
				}
			}
		}

		dpput (priv->index, word, -1, (gchar *) details, tsiz, DP_DOVER);

		retval = TRUE;
	}

	g_free (tmp);


	return retval;
}
