/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2017, Red Hat, Inc.
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

public class Tracker.Direct.Connection : Tracker.Sparql.Connection, AsyncInitable, Initable {
	File? database_loc;
	File? journal_loc;
	File? ontology_loc;
	Sparql.ConnectionFlags flags;

	Data.Manager data_manager;

	// Mutex to hold datamanager
	private Mutex mutex = Mutex ();
	Thread<void*> thread;

	// Initialization stuff, both sync and async
	private Mutex init_mutex = Mutex ();
	private Cond init_cond = Cond ();
	private bool initialized;
	private Error init_error;
	public SourceFunc init_callback;

	private AsyncQueue<Task> update_queue;
	private NamespaceManager namespace_manager;

	[CCode (cname = "SHAREDIR")]
	extern const string SHAREDIR;

	enum TaskType {
		QUERY,
		UPDATE,
		UPDATE_BLANK,
		TURTLE,
	}

	abstract class Task {
		public TaskType type;
		public int priority;
		public Cancellable? cancellable;
		public SourceFunc callback;
		public Error error;
	}

	private class UpdateTask : Task {
		public string sparql;
		public Variant blank_nodes;

		private void set (TaskType type, string sparql, int priority = Priority.DEFAULT, Cancellable? cancellable = null) {
			this.type = type;
			this.sparql = sparql;
			this.priority = priority;
			this.cancellable = cancellable;
		}

		public UpdateTask (string sparql, int priority = Priority.DEFAULT, Cancellable? cancellable) {
			this.set (TaskType.UPDATE, sparql, priority, cancellable);
		}

		public UpdateTask.blank (string sparql, int priority = Priority.DEFAULT, Cancellable? cancellable) {
			this.set (TaskType.UPDATE_BLANK, sparql, priority, cancellable);
		}
	}

	private class TurtleTask : Task {
		public File file;

		public TurtleTask (File file, Cancellable? cancellable) {
			this.type = TaskType.TURTLE;
			this.file = file;
			this.priority = Priority.DEFAULT;
			this.cancellable = cancellable;
		}
	}

	static void wal_checkpoint (DBInterface iface, bool blocking) {
		try {
			debug ("Checkpointing database...");
			iface.sqlite_wal_checkpoint (blocking);
			debug ("Checkpointing complete...");
		} catch (Error e) {
			warning (e.message);
		}
	}

	static void wal_checkpoint_on_thread (DBInterface iface) {
		new Thread<void*> ("wal-checkpoint", () => {
			wal_checkpoint (iface, false);
			return null;
		});
	}

	static void wal_hook (DBInterface iface, int n_pages) {
		var manager = (Data.Manager) iface.get_user_data ();
		var wal_iface = manager.get_wal_db_interface ();

		if (n_pages >= 10000) {
			// do immediate checkpointing (blocking updates)
			// to prevent excessive wal file growth
			wal_checkpoint (wal_iface, true);
		} else if (n_pages >= 1000) {
			wal_checkpoint_on_thread (wal_iface);
		}
	}

	private void* thread_func () {
		init_mutex.lock ();

		try {
			Locale.sanity_check ();
			DBManagerFlags db_flags = DBManagerFlags.ENABLE_MUTEXES;
			if ((flags & Sparql.ConnectionFlags.READONLY) != 0)
				db_flags |= DBManagerFlags.READONLY;

			data_manager = new Data.Manager (db_flags,
			                                 database_loc, journal_loc, ontology_loc,
			                                 false, false, 100, 100);
			data_manager.init ();

			var iface = data_manager.get_writable_db_interface ();
			iface.sqlite_wal_hook (wal_hook);
		} catch (Error e) {
			init_error = e;
		} finally {
			if (init_callback != null) {
				init_callback ();
			} else {
				initialized = true;
				init_cond.signal ();
				init_mutex.unlock ();
			}
		}

		while (true) {
			var task = update_queue.pop();

			try {
				switch (task.type) {
				case TaskType.UPDATE:
					UpdateTask update_task = (UpdateTask) task;
					update (update_task.sparql, update_task.priority, update_task.cancellable);
					break;
				case TaskType.UPDATE_BLANK:
					UpdateTask update_task = (UpdateTask) task;
					update_task.blank_nodes = update_blank (update_task.sparql, update_task.priority, update_task.cancellable);
					break;
				case TaskType.TURTLE:
					TurtleTask turtle_task = (TurtleTask) task;
					load (turtle_task.file, turtle_task.cancellable);
					break;
				default:
					break;
				}
			} catch (Error e) {
				task.error = e;
			}

			task.callback ();
		}
	}

	public async bool init_async (int io_priority, Cancellable? cancellable) throws Error {
		init_callback = init_async.callback;
		thread = new Thread<void*> ("database", thread_func);

		return initialized;
	}

	public bool init (Cancellable? cancellable) throws Error {
		try {
			thread = new Thread<void*> ("database", thread_func);

			init_mutex.lock ();
			while (!initialized)
				init_cond.wait(init_mutex);
			init_mutex.unlock ();

			if (init_error != null)
				throw init_error;
		} catch (Error e) {
			throw new Sparql.Error.INTERNAL (e.message);
		}

		return true;
	}

	public Connection (Sparql.ConnectionFlags connection_flags, File loc, File? journal, File? ontology) throws Sparql.Error, IOError, DBusError {
		database_loc = loc;
		journal_loc = journal;
		ontology_loc = ontology;
		flags = connection_flags;

		if (journal_loc == null)
			journal_loc = database_loc;
		if (ontology_loc == null)
			ontology_loc = File.new_for_path (Path.build_filename (SHAREDIR, "tracker", "ontologies", "nepomuk"));

		update_queue = new AsyncQueue<Task> ();
	}

	public override void dispose () {
		data_manager.shutdown ();
		base.dispose ();
        }

	Sparql.Cursor query_unlocked (string sparql) throws Sparql.Error, DBusError {
		try {
			var query_object = new Sparql.Query (data_manager, sparql);
			var cursor = query_object.execute_cursor ();
			cursor.connection = this;
			return cursor;
		} catch (DBInterfaceError e) {
			throw new Sparql.Error.INTERNAL (e.message);
		} catch (DateError e) {
			throw new Sparql.Error.PARSE (e.message);
		}
	}

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws Sparql.Error, IOError, DBusError {
		// Check here for early cancellation, just in case
		// the operation can be entirely avoided
		if (cancellable != null && cancellable.is_cancelled ()) {
			throw new IOError.CANCELLED ("Operation was cancelled");
		}

		mutex.lock ();
		try {
			return query_unlocked (sparql);
		} finally {
			mutex.unlock ();
		}
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable) throws Sparql.Error, IOError, DBusError {
		// run in a separate thread
		Sparql.Error sparql_error = null;
		IOError io_error = null;
		DBusError dbus_error = null;
		GLib.Error error = null;
		Sparql.Cursor result = null;
		var context = MainContext.get_thread_default ();

		IOSchedulerJob.push ((job, cancellable) => {
			try {
				result = query (sparql, cancellable);
			} catch (IOError e_io) {
				io_error = e_io;
			} catch (Sparql.Error e_spql) {
				sparql_error = e_spql;
			} catch (DBusError e_dbus) {
				dbus_error = e_dbus;
			} catch (GLib.Error e) {
				error = e;
			}

			context.invoke (() => {
				query_async.callback ();
				return false;
			});

			return false;
		}, Priority.DEFAULT, cancellable);

		yield;

		if (cancellable != null && cancellable.is_cancelled ()) {
			throw new IOError.CANCELLED ("Operation was cancelled");
		} else if (sparql_error != null) {
			throw sparql_error;
		} else if (io_error != null) {
			throw io_error;
		} else if (dbus_error != null) {
			throw dbus_error;
		} else {
			return result;
		}
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, GLib.Error {
		mutex.lock ();
		try {
			var data = data_manager.get_data ();
			data.update_sparql (sparql);
		} finally {
			mutex.unlock ();
		}
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, GLib.Error {
		var task = new UpdateTask (sparql, priority, cancellable);
		task.callback = update_async.callback;
		update_queue.push (task);
		yield;

		if (task.error != null)
			throw task.error;
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, GLib.Error {
		GLib.Variant? blank_nodes = null;
		mutex.lock ();
		try {
			var data = data_manager.get_data ();
			blank_nodes = data.update_sparql_blank (sparql);
		} finally {
			mutex.unlock ();
		}

		return blank_nodes;
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError, GLib.Error {
		var task = new UpdateTask.blank (sparql, priority, cancellable);
		task.callback = update_blank_async.callback;
		update_queue.push (task);
		yield;

		if (task.error != null)
			throw task.error;

		return task.blank_nodes;
	}

	public async override GenericArray<Sparql.Error?>? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, GLib.Error, GLib.IOError, DBusError {
		var combined_query = new StringBuilder ();
		var n_updates = sparql.length;
		int i;

		for (i = 0; i < n_updates; i++)
			combined_query.append (sparql[i]);

		var task = new UpdateTask (combined_query.str, priority, cancellable);
		task.callback = update_array_async.callback;
		update_queue.push (task);
		yield;

		var errors = new GenericArray<Sparql.Error?> (n_updates);

		if (task.error == null) {
			for (i = 0; i < n_updates; i++)
				errors.add (null);
		} else {
			// combined query was not successful, try queries one by one
			for (i = 0; i < n_updates; i++) {
				try {
					yield update_async (sparql[i], priority, cancellable);
					errors.add (null);
				} catch (Sparql.Error e) {
					errors.add (e);
				}
			}
		}

		return errors;
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		mutex.lock ();
		try {
			var data = data_manager.get_data ();
			data.load_turtle_file (file);
		} finally {
			mutex.unlock ();
		}
	}

	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		var task = new TurtleTask (file, cancellable);
		task.callback = load_async.callback;
		update_queue.push (task);
		yield;

		if (task.error != null)
			throw new Sparql.Error.INTERNAL (task.error.message);
	}

	public override NamespaceManager? get_namespace_manager () {
		if (namespace_manager == null && data_manager != null) {
			var ht = data_manager.get_namespaces ();
			namespace_manager = new NamespaceManager ();

			foreach (var prefix in ht.get_keys ()) {
				namespace_manager.add_prefix (prefix, ht.lookup (prefix));
			}
		}

		return namespace_manager;
	}
}
