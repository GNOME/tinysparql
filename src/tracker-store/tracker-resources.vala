/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2011, Nokia <ivan.frade@nokia.com>
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

[DBus (name = "org.freedesktop.Tracker1.Resources")]
public class Tracker.Resources : Object {
	public const string PATH = "/org/freedesktop/Tracker1/Resources";

	const int GRAPH_UPDATED_IMMEDIATE_EMIT_AT = 50000;
	const int SIGNALS_SECONDS_PER_EMIT = 1;

	/* I *know* that this is some arbitrary number that doesn't seem to
	 * resemble anything. In fact it's what I experimentally measured to
	 * be a good value on a default Debian testing which has
	 * max_message_size set to 1 000 000 000 in session.conf. I didn't have
	 * the feeling that this value was very much respected, as the size
	 * of the DBusMessage when libdbus decided to exit() the process was
	 * around 160 MB, and not ~ 1000 MB. So if you take 160 MB and you
	 * devide it by 1000000 you have an average string size of ~ 160
	 * bytes plus DBusMessage's overhead. If that makes this number less
	 * arbitrary for you, then fine.
	 *
	 * I really hope that the libdbus people get to their senses and
	 * either stop doing their exit() nonsense in a library, and instead
	 * return a clean DBusError or something, or create crystal clear
	 * clarity about the maximum size of a message. And make it both so
	 * that I can get this length at runtime (without having to parse
	 * libdbus's own configuration files) and my DBusMessage's current
	 * total length. As far as I know are both not possible. So that for
	 * me means that libdbus's exit() is unacceptable.
	 *
	 * Note for the debugger of the future, the "Disconnected" signal gets
	 * sent to us by the bus, which in turn makes libdbus-glib perform exit(). */

	const int DBUS_ARBITRARY_MAX_MSG_SIZE = 10000000;

	DBusConnection connection;
	uint signal_timeout;

	public signal void writeback ([DBus (signature = "a{iai}")] Variant subjects);
	public signal void graph_updated (string classname, [DBus (signature = "a(iiii)")] Variant deletes, [DBus (signature = "a(iiii)")] Variant inserts);

	public Resources (DBusConnection connection) {
		this.connection = connection;
	}

	public async void load (BusName sender, string uri) throws Error {
		var request = DBusRequest.begin (sender, "Resources.Load (uri: '%s')", uri);
		try {
			var file = File.new_for_uri (uri);

			yield Tracker.Store.queue_turtle_import (file, sender);

			request.end ();
		} catch (DBInterfaceError.NO_SPACE ie) {
			throw new Sparql.Error.NO_SPACE (ie.message);
		} catch (Error e) {
			request.end (e);
			if (e is Sparql.Error) {
				throw e;
			} else {
				throw new Sparql.Error.INTERNAL (e.message);
			}
		}
	}

	[DBus (signature = "aas")]
	public async Variant sparql_query (BusName sender, string query) throws Error {
		var request = DBusRequest.begin (sender, "Resources.SparqlQuery");
		request.debug ("query: %s", query);
		try {
			var builder = new VariantBuilder ((VariantType) "aas");

			yield Tracker.Store.sparql_query (query, Tracker.Store.Priority.HIGH, cursor => {
				while (cursor.next ()) {
					builder.open ((VariantType) "as");

					for (int i = 0; i < cursor.n_columns; i++) {
						unowned string str = cursor.get_string (i);

						if (str == null) {
							str = "";
						}

						builder.add ("s", str);
					}

					builder.close ();
				}
			}, sender);

			var result = builder.end ();
			if (result.get_size () > DBUS_ARBITRARY_MAX_MSG_SIZE) {
				throw new DBusError.FAILED ("result set of the query is too large");
			}

			request.end ();

			return result;
		} catch (Error e) {
			request.end (e);
			if (e is Sparql.Error) {
				throw e;
			} else {
				throw new Sparql.Error.INTERNAL (e.message);
			}
		}
	}

	public async void sparql_update (BusName sender, string update) throws Error {
		var request = DBusRequest.begin (sender, "Resources.SparqlUpdate");
		request.debug ("query: %s", update);
		try {
			yield Tracker.Store.sparql_update (update, Tracker.Store.Priority.HIGH, sender);

			request.end ();
		} catch (DBInterfaceError.NO_SPACE ie) {
			throw new Sparql.Error.NO_SPACE (ie.message);
		} catch (Error e) {
			request.end (e);
			if (e is Sparql.Error) {
				throw e;
			} else {
				throw new Sparql.Error.INTERNAL (e.message);
			}
		}
	}

	[DBus (signature = "aaa{ss}")]
	public async Variant sparql_update_blank (BusName sender, string update) throws Error {
		var request = DBusRequest.begin (sender, "Resources.SparqlUpdateBlank");
		request.debug ("query: %s", update);
		try {
			return yield Tracker.Store.sparql_update_blank (update, Tracker.Store.Priority.HIGH, sender);
		} catch (DBInterfaceError.NO_SPACE ie) {
			throw new Sparql.Error.NO_SPACE (ie.message);
		} catch (Error e) {
			request.end (e);
			if (e is Sparql.Error) {
				throw e;
			} else {
				throw new Sparql.Error.INTERNAL (e.message);
			}
		}
	}

	public void sync (BusName sender) {
		var request = DBusRequest.begin (sender, "Resources.Sync");

		Data.sync ();

		request.end ();
	}

	public async void batch_sparql_update (BusName sender, string update) throws Error {
		var request = DBusRequest.begin (sender, "Resources.BatchSparqlUpdate");
		request.debug ("query: %s", update);
		try {
			yield Tracker.Store.sparql_update (update, Tracker.Store.Priority.LOW, sender);

			request.end ();
		} catch (DBInterfaceError.NO_SPACE ie) {
			throw new Sparql.Error.NO_SPACE (ie.message);
		} catch (Error e) {
			request.end (e);
			if (e is Sparql.Error) {
				throw e;
			} else {
				throw new Sparql.Error.INTERNAL (e.message);
			}
		}
	}

	public void batch_commit () {
		/* no longer needed, just return */
	}

	bool emit_graph_updated (Class cl) {
		if (cl.has_insert_events () || cl.has_delete_events ()) {
			var builder = new VariantBuilder ((VariantType) "a(iiii)");
			cl.foreach_delete_event ((graph_id, subject_id, pred_id, object_id) => {
				builder.add ("(iiii)", graph_id, subject_id, pred_id, object_id);
			});
			var deletes = builder.end ();

			builder = new VariantBuilder ((VariantType) "a(iiii)");
			cl.foreach_insert_event ((graph_id, subject_id, pred_id, object_id) => {
				builder.add ("(iiii)", graph_id, subject_id, pred_id, object_id);
			});
			var inserts = builder.end ();

			graph_updated (cl.uri, deletes, inserts);

			cl.reset_ready_events ();

			return true;
		}
		return false;
	}

	bool on_emit_signals () {
		foreach (var cl in Tracker.Events.get_classes ()) {
			emit_graph_updated (cl);
		}

		/* Reset counter */
		Tracker.Events.get_total (true);

		/* Writeback feature */
		var writebacks = Tracker.Writeback.get_ready ();

		if (writebacks != null) {
			var builder = new VariantBuilder ((VariantType) "a{iai}");

			var wb_iter = HashTableIter<int, GLib.Array<int>> (writebacks);

			int subject_id;
			unowned Array<int> types;
			while (wb_iter.next (out subject_id, out types)) {
				builder.open ((VariantType) "{iai}");

				builder.add ("i", subject_id);

				builder.open ((VariantType) "ai");
				for (int i = 0; i < types.length; i++) {
					builder.add ("i", types.index (i));
				}
				builder.close ();

				builder.close ();
			}

			writeback (builder.end ());
		}

		Tracker.Writeback.reset_ready ();

		signal_timeout = 0;
		return false;
	}

	void on_statements_committed (bool start_timer) {
		/* Class signal feature */

		foreach (var cl in Tracker.Events.get_classes ()) {
			cl.transact_events ();
		}

		if (start_timer && signal_timeout == 0) {
			signal_timeout = Timeout.add_seconds (SIGNALS_SECONDS_PER_EMIT, on_emit_signals);
		}

		/* Writeback feature */
		Tracker.Writeback.transact ();
	}

	void on_statements_rolled_back (bool start_timer) {
		Tracker.Events.reset_pending ();
		Tracker.Writeback.reset_pending ();
	}

	void check_graph_updated_signal () {
		/* Check for whether we need an immediate emit */
		if (Tracker.Events.get_total (false) > GRAPH_UPDATED_IMMEDIATE_EMIT_AT) {

			foreach (var cl in Tracker.Events.get_classes ()) {
				emit_graph_updated (cl);
			}

			/* Reset counter */
			Tracker.Events.get_total (true);
		}
	}

	void on_statement_inserted (int graph_id, string? graph, int subject_id, string subject, int pred_id, int object_id, string? object, PtrArray rdf_types) {
		Tracker.Events.add_insert (graph_id, subject_id, subject, pred_id, object_id, object, rdf_types);
		Tracker.Writeback.check (graph_id, graph, subject_id, subject, pred_id, object_id, object, rdf_types);
		check_graph_updated_signal ();
	}

	void on_statement_deleted (int graph_id, string? graph, int subject_id, string subject, int pred_id, int object_id, string? object, PtrArray rdf_types) {
		Tracker.Events.add_delete (graph_id, subject_id, subject, pred_id, object_id, object, rdf_types);
		Tracker.Writeback.check (graph_id, graph, subject_id, subject, pred_id, object_id, object, rdf_types);
		check_graph_updated_signal ();
	}

	[DBus (visible = false)]
	public void enable_signals () {
		Tracker.Data.add_insert_statement_callback (on_statement_inserted);
		Tracker.Data.add_delete_statement_callback (on_statement_deleted);
		Tracker.Data.add_commit_statement_callback (on_statements_committed);
		Tracker.Data.add_rollback_statement_callback (on_statements_rolled_back);
	}

	[DBus (visible = false)]
	public void disable_signals () {
		Tracker.Data.remove_insert_statement_callback (on_statement_inserted);
		Tracker.Data.remove_delete_statement_callback (on_statement_deleted);
		Tracker.Data.remove_commit_statement_callback (on_statements_committed);
		Tracker.Data.remove_rollback_statement_callback (on_statements_rolled_back);

		if (signal_timeout != 0) {
			Source.remove (signal_timeout);
		}
	}

	~Resources () {
		this.disable_signals ();
	}

	[DBus (visible = false)]
	public void unreg_batches (string old_owner) {
		Tracker.Store.unreg_batches (old_owner);
	}
}
