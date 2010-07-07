/*
 * Copyright (C) 2010, Adrien Bustany <abustany@gnome.org>
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

[CCode (cprefix = "Tracker", lower_case_cprefix = "tracker_")]
namespace Tracker {
	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public class Client : GLib.Object {
		[CCode (has_construct_function = false)]
		public Client (Tracker.ClientFlags flags, int timeout);

		[CCode (cname = "tracker_cancel_call")]
		public bool cancel_call (uint call_id);
		[CCode (cname = "tracker_cancel_last_call")]
		public bool cancel_last_call ();
		[CCode (cname = "tracker_resources_batch_commit")]
		public void batch_commit () throws GLib.Error;
		[CCode (cname = "tracker_resources_batch_commit_async")]
		public void batch_commit_async (Tracker.ReplyVoid callback);
		[CCode (cname = "tracker_resources_batch_sparql_update")]
		public void batch_sparql_update (string query) throws GLib.Error;
		[CCode (cname = "tracker_resources_batch_sparql_update_async")]
		public void batch_sparql_update_async (string query, Tracker.ReplyVoid callback);
		[CCode (cname = "tracker_resources_load")]
		public void load (string uri) throws GLib.Error;
		[CCode (cname = "tracker_resources_load_async")]
		public void load_async (string uri, Tracker.ReplyVoid callback);
		[CCode (cname = "tracker_resources_sparql_query")]
		public GLib.PtrArray sparql_query (string query) throws GLib.Error;
		[CCode (cname = "tracker_resources_sparql_query_iterate")]
		public Tracker.ResultIterator sparql_query_iterate (string query) throws GLib.Error;
		[CCode (cname = "tracker_resources_sparql_query_async")]
		public void sparql_query_async (string query, Tracker.ReplyGPtrArray callback);
		[CCode (cname = "tracker_resources_sparql_query_iterate_async")]
		public void sparql_query_iterate_async (string query, Tracker.ReplyIterator callback);
		[CCode (cname = "tracker_resources_sparql_update")]
		public void sparql_update (string query) throws GLib.Error;
		[CCode (cname = "tracker_resources_sparql_update_async")]
		public void sparql_update_async (string query, Tracker.ReplyVoid callback);
		[CCode (cname = "tracker_resources_sparql_update_blank")]
		public GLib.PtrArray sparql_update_blank (string query) throws GLib.Error;
		[CCode (cname = "tracker_resources_sparql_update_blank_async")]
		public void sparql_update_blank_async (string query, Tracker.ReplyGPtrArray callback);
		[CCode (cname = "tracker_statistics_get")]
		public GLib.PtrArray statistics_get () throws GLib.Error;
		[CCode (cname = "tracker_statistics_get_async")]
		public void statistics_get_async (Tracker.ReplyGPtrArray callback);
		[CCode (cname = "tracker_resources_writeback_connect")]
		public void writeback_connect (Tracker.WritebackCallback callback);
		[CCode (cname = "tracker_resources_writeback_disconnect")]
		public void writeback_disconnect ();
	}
	[Compact]
	[CCode (free_function = "g_object_unref", cheader_filename = "libtracker-client/tracker-sparql-builder.h")]
	public class SparqlBuilder {
		[CCode (has_construct_function = false)]
		public SparqlBuilder ();
		public void append (string raw);
		public static unowned Tracker.SparqlBuilder @construct (GLib.Type object_type);
		public static unowned Tracker.SparqlBuilder construct_embedded_insert (GLib.Type object_type);
		public static unowned Tracker.SparqlBuilder construct_update (GLib.Type object_type);
		public void delete_close ();
		public void delete_open (string? graph);
		public void drop_graph (string iri);
		[CCode (has_construct_function = false)]
		public SparqlBuilder.embedded_insert ();
		public int get_length ();
		public unowned string get_result ();
		public void insert_close ();
		public void insert_open (string? graph);
		public void object (string s);
		public void object_blank_close ();
		public void object_blank_open ();
		public void object_boolean (bool literal);
		public void object_date (ref time_t literal);
		public void object_double (double literal);
		public void object_int64 (int64 literal);
		public void object_iri (string iri);
		public void object_string (string literal);
		public void object_unvalidated (string value);
		public void object_variable (string var_name);
		public void predicate (string s);
		public void predicate_iri (string iri);
		public void prepend (string raw);
		public void subject (string s);
		public void subject_iri (string iri);
		public void subject_variable (string var_name);
		[CCode (has_construct_function = false)]
		public SparqlBuilder.update ();
		public void where_close ();
		public void where_open ();
	}
	[CCode (cprefix = "TRACKER_CLIENT_ENABLE_", has_type_id = false, cheader_filename = "libtracker-client/tracker-client.h")]
	public enum ClientFlags {
		WARNINGS
	}

	[Compact]
	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public class ResultIterator {
		public int n_columns ();
		public bool next ();
		public unowned string value (uint column);
	}

	[CCode (cheader_filename = "libtracker-client/tracker-client.h", instance_pos = -2)]
	public delegate void ReplyArray (string result, GLib.Error error);
	[CCode (cheader_filename = "libtracker-client/tracker-client.h", instance_pos = -2)]
	public delegate void ReplyGPtrArray (GLib.PtrArray result, GLib.Error error);
	[CCode (cheader_filename = "libtracker-client/tracker-client.h", instance_pos = -2)]
	public delegate void ReplyIterator (Tracker.ResultIterator iterator, GLib.Error error);
	[CCode (cheader_filename = "libtracker-client/tracker-client.h", instance_pos = -2)]
	public delegate void ReplyVoid (GLib.Error error);
	[CCode (cheader_filename = "libtracker-client/tracker.h", instance_pos = -2)]
	public delegate void WritebackCallback (GLib.HashTable resources);

	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public const string DBUS_INTERFACE_RESOURCES;
	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public const string DBUS_INTERFACE_STATISTICS;
	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public const string DBUS_OBJECT;
	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public const string DBUS_SERVICE;

	[CCode (cheader_filename = "libtracker-client/tracker-client.h")]
	public static string sparql_escape (string str);
}
