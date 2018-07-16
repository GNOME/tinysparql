/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

namespace Tracker {
	[CCode (cheader_filename = "tracker-store/tracker-events.h")]
	namespace Events {
		public void init ();
		public void shutdown ();
		public void add_insert (int graph_id, int subject_id, string subject, int pred_id, int object_id, string object, GLib.PtrArray rdf_types);
		public void add_delete (int graph_id, int subject_id, string subject, int pred_id, int object_id, string object, GLib.PtrArray rdf_types);
		public uint get_total ();
		public void reset_pending ();

		public void transact ();
		public GLib.HashTable<Tracker.Class, Batch> get_pending ();

		[CCode (lower_case_cprefix="tracker_event_batch_", cname = "TrackerEventBatch")]
		public class Batch {
			public delegate void EventsForeach (int graph_id, int subject_id, int pred_id, int object_id);
			public void foreach_delete_event (EventsForeach func);
			public void foreach_insert_event (EventsForeach func);
                }
	}
}
