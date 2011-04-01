/*
 * Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
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

[DBus (name = "org.freedesktop.Tracker1.Resources")]
private interface Tracker.Bus.Resources : DBusProxy {
	public abstract void load (string uri, Cancellable? cancellable) throws Sparql.Error, DBusError;
	[DBus (name = "Load")]
	public abstract async void load_async (string uri, Cancellable? cancellable) throws Sparql.Error, DBusError;
}

[DBus (name = "org.freedesktop.Tracker1.Steroids")]
private interface Tracker.Bus.Steroids : DBusProxy {
	public abstract async string[] query (string query, UnixOutputStream result_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;
	public abstract async void update (UnixInputStream sparql_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;
	[DBus (signature = "aaa{ss}")]
	public abstract async Variant update_blank (UnixInputStream sparql_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;
	public abstract async void batch_update (UnixInputStream sparql_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;
	[DBus (signature = "as")]
	public abstract async Variant update_array (UnixInputStream sparql_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;
	[DBus (signature = "as")]
	public abstract async Variant batch_update_array (UnixInputStream sparql_stream, Cancellable? cancellable) throws Sparql.Error, DBusError;

	[DBus (visible = false)]
	public void update_begin (UnixInputStream sparql_stream, int priority, Cancellable? cancellable, AsyncReadyCallback callback) {
		if (priority <= GLib.Priority.DEFAULT) {
			update.begin (sparql_stream, cancellable, callback);
		} else {
			batch_update.begin (sparql_stream, cancellable, callback);
		}
	}

	[DBus (visible = false)]
	public void update_array_begin (UnixInputStream sparql_stream, int priority, Cancellable? cancellable, AsyncReadyCallback callback) {
		if (priority <= GLib.Priority.DEFAULT) {
			update_array.begin (sparql_stream, cancellable, callback);
		} else {
			batch_update_array.begin (sparql_stream, cancellable, callback);
		}
	}
}

[DBus (name = "org.freedesktop.Tracker1.Statistics")]
private interface Tracker.Bus.Statistics : DBusProxy {
	public abstract string[,] Get (Cancellable? cancellable) throws DBusError;
	public async abstract string[,] Get_async (Cancellable? cancellable) throws DBusError;
}

// Actual class definition
public class Tracker.Bus.Connection : Tracker.Sparql.Connection {
	static Resources resources_object;
	static Steroids steroids_object;
	static Statistics statistics_object;
	static bool initialized;

	public Connection () throws Sparql.Error, IOError, DBusError
	requires (!initialized) {
		// FIXME: Ideally we would just get these as and when we need them
		resources_object = GLib.Bus.get_proxy_sync (BusType.SESSION,
		                                            TRACKER_DBUS_SERVICE,
		                                            TRACKER_DBUS_OBJECT_RESOURCES,
		                                            DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
		resources_object.set_default_timeout (int.MAX);
		steroids_object = GLib.Bus.get_proxy_sync (BusType.SESSION,
		                                           TRACKER_DBUS_SERVICE,
		                                           TRACKER_DBUS_OBJECT_STEROIDS,
		                                           DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);
		steroids_object.set_default_timeout (int.MAX);
		statistics_object = GLib.Bus.get_proxy_sync (BusType.SESSION,
		                                             TRACKER_DBUS_SERVICE,
		                                             TRACKER_DBUS_OBJECT_STATISTICS,
		                                             DBusProxyFlags.DO_NOT_LOAD_PROPERTIES | DBusProxyFlags.DO_NOT_CONNECT_SIGNALS);

		initialized = true;
	}

	public override void init () throws Sparql.Error, IOError, DBusError {
	}

	~Connection () {
		initialized = false;
	}

	void pipe (out UnixInputStream input, out UnixOutputStream output) throws IOError {
		int pipefd[2];
		if (Posix.pipe (pipefd) < 0) {
			throw new IOError.FAILED ("Pipe creation failed");
		}
		input = new UnixInputStream (pipefd[0], true);
		output = new UnixOutputStream (pipefd[1], true);
	}

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws Sparql.Error, IOError, DBusError {
		// use separate main context for sync operation
		var context = new MainContext ();
		var loop = new MainLoop (context, false);
		context.push_thread_default ();
		AsyncResult async_res = null;
		query_async.begin (sparql, cancellable, (o, res) => {
			async_res = res;
			loop.quit ();
		});
		loop.run ();
		context.pop_thread_default ();
		return query_async.end (async_res);
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		UnixInputStream input;
		UnixOutputStream output;
		pipe (out input, out output);

		// send D-Bus request
		AsyncResult dbus_res = null;
		bool received_result = false;
		steroids_object.query.begin (sparql, output, cancellable, (o, res) => {
			dbus_res = res;
			if (received_result) {
				query_async.callback ();
			}
		});

		output = null;

		// receive query results via FD
		var mem_stream = new MemoryOutputStream (null, GLib.realloc, GLib.free);
		yield mem_stream.splice_async (input, OutputStreamSpliceFlags.CLOSE_SOURCE | OutputStreamSpliceFlags.CLOSE_TARGET, Priority.DEFAULT, cancellable);

		// wait for D-Bus reply
		received_result = true;
		if (dbus_res == null) {
			yield;
		}
		string[] variable_names = steroids_object.query.end (dbus_res);
		mem_stream.close ();
		return new FDCursor (mem_stream.steal_data (), mem_stream.data_size, variable_names);
	}

	public override void update (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		// use separate main context for sync operation
		var context = new MainContext ();
		var loop = new MainLoop (context, false);
		context.push_thread_default ();
		AsyncResult async_res = null;
		update_async.begin (sparql, priority, cancellable, (o, res) => {
			async_res = res;
			loop.quit ();
		});
		loop.run ();
		context.pop_thread_default ();
		update_async.end (async_res);
	}

	public async override void update_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		UnixInputStream input;
		UnixOutputStream output;
		pipe (out input, out output);

		// send D-Bus request
		AsyncResult dbus_res = null;
		bool sent_update = false;
		steroids_object.update_begin (input, priority, cancellable, (o, res) => {
			dbus_res = res;
			if (sent_update) {
				update_async.callback ();
			}
		});

		// send sparql string via fd
		var data_stream = new DataOutputStream (output);
		data_stream.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);
		data_stream.put_int32 ((int32) sparql.length);
		data_stream.put_string (sparql);
		data_stream = null;

		// wait for D-Bus reply
		sent_update = true;
		if (dbus_res == null) {
			yield;
		}

		if (priority <= GLib.Priority.DEFAULT) {
			steroids_object.update.end (dbus_res);
		} else {
			steroids_object.batch_update.end (dbus_res);
		}
	}

	public async override GenericArray<Error?>? update_array_async (string[] sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		UnixInputStream input;
		UnixOutputStream output;
		pipe (out input, out output);

		// send D-Bus request
		AsyncResult dbus_res = null;
		bool sent_update = false;
		steroids_object.update_array_begin (input, priority, cancellable, (o, res) => {
			dbus_res = res;
			if (sent_update) {
				update_array_async.callback ();
			}
		});

		// send sparql strings via fd
		var data_stream = new DataOutputStream (output);
		data_stream.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);
		data_stream.put_int32 ((int32) sparql.length);
		for (int i = 0; i < sparql.length; i++) {
			data_stream.put_int32 ((int32) sparql[i].length);
			data_stream.put_string (sparql[i]);
		}
		data_stream = null;

		// wait for D-Bus reply
		sent_update = true;
		if (dbus_res == null) {
			yield;
		}

		// process results (errors)
		var result = new GenericArray<Error?> ();
		Variant resultv;
		if (priority <= GLib.Priority.DEFAULT) {
			resultv = steroids_object.update_array.end (dbus_res);
		} else {
			resultv = steroids_object.batch_update_array.end (dbus_res);
		}
		var iter = resultv.iterator ();
		string code, message;
		while (iter.next ("s", out code)) {
			if (iter.next ("s", out message)) {
				if (code != "" && message != "") {
					result.add (new Sparql.Error.INTERNAL (message));
				} else {
					result.add (null);
				}
			}
		}
		return result;
	}

	public override GLib.Variant? update_blank (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		// use separate main context for sync operation
		var context = new MainContext ();
		var loop = new MainLoop (context, false);
		context.push_thread_default ();
		AsyncResult async_res = null;
		update_blank_async.begin (sparql, priority, cancellable, (o, res) => {
			async_res = res;
			loop.quit ();
		});
		loop.run ();
		context.pop_thread_default ();
		return update_blank_async.end (async_res);
	}

	public async override GLib.Variant? update_blank_async (string sparql, int priority = GLib.Priority.DEFAULT, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		UnixInputStream input;
		UnixOutputStream output;
		pipe (out input, out output);

		// send D-Bus request
		AsyncResult dbus_res = null;
		bool sent_update = false;
		steroids_object.update_blank.begin (input, cancellable, (o, res) => {
			dbus_res = res;
			if (sent_update) {
				update_blank_async.callback ();
			}
		});

		// send sparql strings via fd
		var data_stream = new DataOutputStream (output);
		data_stream.set_byte_order (DataStreamByteOrder.HOST_ENDIAN);
		data_stream.put_int32 ((int32) sparql.length);
		data_stream.put_string (sparql);
		data_stream = null;

		// wait for D-Bus reply
		sent_update = true;
		if (dbus_res == null) {
			yield;
		}

		return steroids_object.update_blank.end (dbus_res);
	}

	public override void load (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		resources_object.load (file.get_uri (), cancellable);

		if (cancellable != null && cancellable.is_cancelled ()) {
			throw new IOError.CANCELLED ("Operation was cancelled");
		}
	}
	public async override void load_async (File file, Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		yield resources_object.load_async (file.get_uri (), cancellable);

		if (cancellable != null && cancellable.is_cancelled ()) {
			throw new IOError.CANCELLED ("Operation was cancelled");
		}
	}

	public override Sparql.Cursor? statistics (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		string[,] results = statistics_object.Get (cancellable);
		Sparql.ValueType[] types = new Sparql.ValueType[2];
		string[] var_names = new string[2];

		var_names[0] = "class";
		var_names[1] = "count";
		types[0] = Sparql.ValueType.STRING;
		types[1] = Sparql.ValueType.INTEGER;

		return new Tracker.Bus.ArrayCursor ((owned) results,
		                                    results.length[0],
		                                    results.length[1],
		                                    var_names,
		                                    types);
	}

	public async override Sparql.Cursor? statistics_async (Cancellable? cancellable = null) throws Sparql.Error, IOError, DBusError {
		string[,] results = yield statistics_object.Get_async (cancellable);
		Sparql.ValueType[] types = new Sparql.ValueType[2];
		string[] var_names = new string[2];

		var_names[0] = "class";
		var_names[1] = "count";
		types[0] = Sparql.ValueType.STRING;
		types[1] = Sparql.ValueType.INTEGER;

		return new Tracker.Bus.ArrayCursor ((owned) results,
		                                    results.length[0],
		                                    results.length[1],
		                                    var_names,
		                                    types);
	}
}
