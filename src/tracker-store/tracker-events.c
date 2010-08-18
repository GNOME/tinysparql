/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <libtracker-data/tracker-data.h>

#include "tracker-events.h"

typedef struct {
	GHashTable *allowances_id;
	GHashTable *allowances;
	struct {
		//GArray *sub_pred_ids;
		GArray *subject_ids;
		GArray *pred_ids;
		GArray *object_ids;
		GArray *class_ids;
	} deletes;
	struct {
		//GArray *sub_pred_ids;
		GArray *subject_ids;
		GArray *pred_ids;
		GArray *object_ids;
		GArray *class_ids;
	} inserts;
	gboolean frozen;
} EventsPrivate;

static EventsPrivate *private;

/*
void showbits64(gint64 a)
{
  int i  , k , mask;

  for( i =63 ; i >= 0 ; i--)
  {
     mask = 1 << i;
     k = a & mask;
     if( k == 0)
        printf("0 ");
     else
        printf ("1 ");
  }
	printf ("\n");
}

void showbits(gint a)
{
  int i  , k , mask;

  for( i =31 ; i >= 0 ; i--)
  {
     mask = 1 << i;
     k = a & mask;
     if( k == 0)
        printf("0 ");
     else
        printf ("1 ");
  }
	printf ("\n");
}
*/

static void
get_from_array (gint    class_id,
                GArray *class_ids,
                //GArray *sub_pred_ids,
                GArray *subject_ids_in,
                GArray *pred_ids_in,
                GArray *object_ids_in,
                GArray *subject_ids,
                GArray *pred_ids,
                GArray *object_ids)
{
	guint i;

	g_array_set_size (subject_ids, 0);
	g_array_set_size (pred_ids, 0);
	g_array_set_size (object_ids, 0);

	for (i = 0; i < class_ids->len; i++) {
		gint class_id_v;

		class_id_v = g_array_index (class_ids, gint, i);

		if (class_id_v == class_id) {
			gint subject_id, pred_id, object_id;
			/*gint64 sub_pred_id;

			sub_pred_id = g_array_index (sub_pred_ids, gint64, i);

			printf ("get_array\n");

			printf ("sub_pred_id: ");
			showbits64 (sub_pred_id);
			printf ("\n");

			subject_id = sub_pred_id >> 32;
			pred_id = sub_pred_id << 32;

			printf ("subject_id: ");
			showbits (subject_id);
			printf("\n");

			printf ("pred_id: ");
			showbits (pred_id);
			printf("\n");*/

			subject_id = g_array_index (subject_ids_in, gint, i);
			pred_id = g_array_index (pred_ids_in, gint, i);
			object_id = g_array_index (object_ids_in, gint, i);

			g_array_append_val (subject_ids, subject_id);
			g_array_append_val (pred_ids, pred_id);
			g_array_append_val (object_ids, object_id);
		}
	}
}

void
tracker_events_get_inserts (gint    class_id,
                            GArray *subject_ids,
                            GArray *pred_ids,
                            GArray *object_ids)
{
	g_return_if_fail (private != NULL);
	g_return_if_fail (subject_ids != NULL);
	g_return_if_fail (pred_ids != NULL);
	g_return_if_fail (object_ids != NULL);

	get_from_array (class_id,
	                private->inserts.class_ids,
	                //private->inserts.sub_pred_ids,
	                private->inserts.subject_ids,
	                private->inserts.pred_ids,
	                private->inserts.object_ids,
	                subject_ids,
	                pred_ids,
	                object_ids);
}

void
tracker_events_get_deletes (gint    class_id,
                            GArray *subject_ids,
                            GArray *pred_ids,
                            GArray *object_ids)
{
	g_return_if_fail (private != NULL);
	g_return_if_fail (subject_ids != NULL);
	g_return_if_fail (pred_ids != NULL);
	g_return_if_fail (object_ids != NULL);

	get_from_array (class_id,
	                private->deletes.class_ids,
	                //private->deletes.sub_pred_ids,
	                private->inserts.subject_ids,
	                private->inserts.pred_ids,
	                private->deletes.object_ids,
	                subject_ids,
	                pred_ids,
	                object_ids);
}

static gboolean
is_allowed (EventsPrivate *private, TrackerClass *rdf_class, gint class_id)
{
	gboolean ret;

	if (rdf_class != NULL) {
		ret = (g_hash_table_lookup (private->allowances, rdf_class) != NULL) ? TRUE : FALSE;
	} else {
		ret = (g_hash_table_lookup (private->allowances_id, GINT_TO_POINTER (class_id)) != NULL) ? TRUE : FALSE;
	}
	return ret;
}

static void
insert_vals_into_arrays (GArray *class_ids,
                         //GArray *sub_pred_ids,
                         GArray *subject_ids,
                         GArray *pred_ids,
                         GArray *object_ids,
                         gint    class_id,
                         gint    subject_id,
                         gint    pred_id,
                         gint    object_id)
{
//	guint i;
//	gboolean inserted = FALSE;
	/*gint64 sub_pred_id = (gint64) subject_id;

	sub_pred_id = sub_pred_id << 32 | (gint64) pred_id;

	printf ("insert_vals\n");
	printf ("subject_id: ");
	showbits (subject_id);
	printf("\n");

	printf ("pred_id: ");
	showbits (subject_id);
	printf("\n");

	printf ("sub_pred_id: ");
	showbits64 (sub_pred_id);
	printf("\n");*/

	//for (i = 0; i < sub_pred_ids->len; i++) {
//	for (i = 0; i < subject_ids->len; i++) {
//		 if (sub_pred_id < g_array_index (sub_pred_ids, gint64, i)) {
//			g_array_insert_val (class_ids, i, class_id);
//			//g_array_insert_val (sub_pred_ids, i, sub_pred_id);
//			g_array_insert_val (subject_ids, i, subject_id);
//			g_array_insert_val (pred_ids, i, pred_id);
//			g_array_insert_val (object_ids, i, object_id);
//			inserted = TRUE;
//			break;
//		}
//	}

//	if (!inserted) {
		g_array_append_val (class_ids, class_id);
		//g_array_append_val (sub_pred_ids, sub_pred_ids);
		g_array_append_val (subject_ids, subject_id);
		g_array_append_val (pred_ids, pred_id);
		g_array_append_val (object_ids, object_id);
//	}
}

void
tracker_events_add_insert (gint         graph_id,
                           gint         subject_id,
                           const gchar *subject,
                           gint         pred_id,
                           gint         object_id,
                           const gchar *object,
                           GPtrArray   *rdf_types)
{
	TrackerProperty *rdf_type;

	g_return_if_fail (rdf_types != NULL);
	g_return_if_fail (private != NULL);

	if (private->frozen) {
		return;
	}

	rdf_type = tracker_ontologies_get_rdf_type ();

	if (object_id != 0 && pred_id == tracker_property_get_id (rdf_type)) {
		/* Resource create
		 * In case of create, object is the rdf:type */
		if (is_allowed (private, NULL, object_id)) {
			insert_vals_into_arrays (private->inserts.class_ids,
			                         //private->inserts.sub_pred_ids,
			                         private->inserts.subject_ids,
			                         private->inserts.pred_ids,
			                         private->inserts.object_ids,
			                         object_id,
			                         subject_id, pred_id, object_id);
		}
	} else {
		guint i;

		for (i = 0; i < rdf_types->len; i++) {
			if (is_allowed (private, rdf_types->pdata[i], 0)) {
				insert_vals_into_arrays (private->inserts.class_ids,
				                         //private->inserts.sub_pred_ids,
				                         private->inserts.subject_ids,
				                         private->inserts.pred_ids,
				                         private->inserts.object_ids,
				                         tracker_class_get_id (rdf_types->pdata[i]),
				                         subject_id, pred_id, object_id);
			}
		}
	}
}

void
tracker_events_add_delete (gint         graph_id,
                           gint         subject_id,
                           const gchar *subject,
                           gint         pred_id,
                           gint         object_id,
                           const gchar *object,
                           GPtrArray   *rdf_types)
{
	TrackerProperty *rdf_type;

	g_return_if_fail (rdf_types != NULL);
	g_return_if_fail (private != NULL);

	if (private->frozen) {
		return;
	}

	rdf_type = tracker_ontologies_get_rdf_type ();

	if (object_id != 0 && pred_id == tracker_property_get_id (rdf_type)) {
		/* Resource delete
		 * In case of delete, object is the rdf:type */
		if (is_allowed (private, NULL, object_id)) {
			insert_vals_into_arrays (private->deletes.class_ids,
			                         //private->deletes.sub_pred_ids,
			                         private->deletes.subject_ids,
			                         private->deletes.pred_ids,
			                         private->deletes.object_ids,
			                         object_id,
			                         subject_id, pred_id, object_id);
		}
	} else {
		guint i;

		for (i = 0; i < rdf_types->len; i++) {
			if (is_allowed (private, rdf_types->pdata[i], 0)) {
				insert_vals_into_arrays (private->deletes.class_ids,
				                         //private->deletes.sub_pred_ids,
				                         private->deletes.subject_ids,
				                         private->deletes.pred_ids,
				                         private->deletes.object_ids,
				                         tracker_class_get_id (rdf_types->pdata[i]),
				                         subject_id, pred_id, object_id);
			}
		}
	}

}

void
tracker_events_reset (void)
{
	g_return_if_fail (private != NULL);

	g_array_set_size (private->deletes.class_ids, 0);
	//g_array_set_size (private->deletes.sub_pred_ids, 0);
	g_array_set_size (private->deletes.subject_ids, 0);
	g_array_set_size (private->deletes.pred_ids, 0);
	g_array_set_size (private->deletes.object_ids, 0);

	g_array_set_size (private->inserts.class_ids, 0);
	//g_array_set_size (private->inserts.sub_pred_ids, 0);
	g_array_set_size (private->inserts.subject_ids, 0);
	g_array_set_size (private->inserts.pred_ids, 0);
	g_array_set_size (private->inserts.object_ids, 0);

	private->frozen = FALSE;
}

void
tracker_events_freeze (void)
{
	g_return_if_fail (private != NULL);

	private->frozen = TRUE;
}

static void
free_private (EventsPrivate *private)
{
	g_hash_table_unref (private->allowances);
	g_hash_table_unref (private->allowances_id);

	g_array_free (private->deletes.class_ids, TRUE);
	//g_array_free (private->deletes.sub_pred_ids, TRUE);
	g_array_free (private->deletes.subject_ids, TRUE);
	g_array_free (private->deletes.pred_ids, TRUE);
	g_array_free (private->deletes.object_ids, TRUE);

	g_array_free (private->inserts.class_ids, TRUE);
	//g_array_free (private->inserts.sub_pred_ids, TRUE);
	g_array_free (private->inserts.subject_ids, TRUE);
	g_array_free (private->inserts.pred_ids, TRUE);
	g_array_free (private->inserts.object_ids, TRUE);

	g_free (private);
}

void
tracker_events_init (TrackerNotifyClassGetter callback)
{
	GStrv classes_to_signal;
	gint  i, count;

	if (!callback) {
		return;
	}

	private = g_new0 (EventsPrivate, 1);

	private->allowances = g_hash_table_new (g_direct_hash, g_direct_equal);
	private->allowances_id = g_hash_table_new (g_direct_hash, g_direct_equal);

	private->deletes.class_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	//private->deletes.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	private->deletes.subject_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	private->deletes.pred_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	private->deletes.object_ids = g_array_new (FALSE, FALSE, sizeof (gint));

	private->inserts.class_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	//private->inserts.sub_pred_ids = g_array_new (FALSE, FALSE, sizeof (gint64));
	private->inserts.subject_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	private->inserts.pred_ids = g_array_new (FALSE, FALSE, sizeof (gint));
	private->inserts.object_ids = g_array_new (FALSE, FALSE, sizeof (gint));

	classes_to_signal = (*callback)();

	if (!classes_to_signal)
		return;

	count = g_strv_length (classes_to_signal);
	for (i = 0; i < count; i++) {
		TrackerClass *class = tracker_ontologies_get_class_by_uri (classes_to_signal[i]);
		if (class != NULL) {
			g_hash_table_insert (private->allowances,
			                     class,
			                     GINT_TO_POINTER (TRUE));
			g_hash_table_insert (private->allowances_id,
			                     GINT_TO_POINTER (tracker_class_get_id (class)),
			                     GINT_TO_POINTER (TRUE));
			g_debug ("ClassSignal allowance: %s has ID %d",
			         tracker_class_get_name (class),
			         tracker_class_get_id (class));
		}
	}
	g_strfreev (classes_to_signal);
}

void
tracker_events_classes_iter (GHashTableIter *iter)
{
	g_return_if_fail (private != NULL);

	g_hash_table_iter_init (iter, private->allowances);
}

void
tracker_events_shutdown (void)
{
	if (private != NULL) {
		free_private (private);
		private = NULL;
	} else {
		g_warning ("tracker_events already shutdown");
	}
}
