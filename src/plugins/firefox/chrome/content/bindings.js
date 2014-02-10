if (!org.bustany.TrackerFox.TrackerSparql || !org.bustany.TrackerFox.TrackerSparql.__initialized)
org.bustany.TrackerFox.TrackerSparql = {
	__initialized: true,

	_trackerSparqlPath1: "libtracker-sparql-1.0.so.0",
	_trackerSparqlPath2: "libtracker-sparql-0.16.so.0",
	_lib: null,

	init: function () {
		var tracker = org.bustany.TrackerFox.TrackerSparql;

		// Safeguard just in case a fool would call that twice
		if (tracker._lib) {
			return true;
		}

		Components.utils.import ("resource://gre/modules/ctypes.jsm");

		try {
			tracker._lib = ctypes.open (tracker._trackerSparqlPath1);
		} catch (e) {
			dump ("Could not load " + tracker._trackerSparqlPath1 + ": " + e + "\n");

		        try {
			        tracker._lib = ctypes.open (tracker._trackerSparqlPath2);
			} catch (e) {
			        dump ("Could not load " + tracker._trackerSparqlPath2 + ": " + e + "\n");
			        return false;
			}
		}

		// GLib types
		tracker.Object = new ctypes.StructType ("GObject");
		tracker.Cancellable = new ctypes.StructType ("GCancellable");
		tracker.Error = new ctypes.StructType ("GError", [
			{domain  : ctypes.uint32_t},
			{code    : ctypes.int32_t},
			{message : ctypes.char.ptr}
		]);
		tracker.AsyncResult = new ctypes.StructType ("GAsyncResult");
		tracker.AsyncReadyCallback = new ctypes.FunctionType(
			ctypes.default_abi,
			ctypes.void_t,
			[ tracker.Object.ptr, tracker.AsyncResult.ptr, ctypes.voidptr_t ]);

		// TrackerSparql types
		tracker.Connection = ctypes.StructType ("TrackerSparqlConnection");
		tracker.Cursor = ctypes.StructType ("TrackerSparqlCursor");

		// GLib methods
		tracker.object_unref = tracker._lib.declare (
			"g_object_unref",
			ctypes.default_abi,
			ctypes.void_t,
			ctypes.void_t.ptr // Binding GObject* as a void* since we don't have GObject binding...
		);

		tracker.error_free = tracker._lib.declare (
			"g_error_free",
			ctypes.default_abi,
			ctypes.void_t,
			tracker.Error.ptr
		);

		// TrackerSparql methods (name, ABI, args with first arg = return type)
		tracker.connection_open = tracker._lib.declare (
			"tracker_sparql_connection_get",
			ctypes.default_abi,
			tracker.Connection.ptr,
			tracker.Cancellable.ptr,
			tracker.Error.ptr.ptr
		);

		tracker.connection_open_async = tracker._lib.declare (
			"tracker_sparql_connection_get_async",
			ctypes.default_abi,
			ctypes.void_t,
			tracker.Cancellable.ptr,
			tracker.AsyncReadyCallback.ptr,
			ctypes.voidptr_t);

		tracker.connection_open_finish = tracker._lib.declare (
			"tracker_sparql_connection_get_finish",
			ctypes.default_abi,
			tracker.Connection.ptr,
			tracker.AsyncResult.ptr,
			tracker.Error.ptr.ptr);

		tracker.connection_query = tracker._lib.declare (
			"tracker_sparql_connection_query",
			ctypes.default_abi,
			tracker.Cursor.ptr,
			tracker.Connection.ptr,
			ctypes.char.ptr,
			tracker.Cancellable.ptr,
			tracker.Error.ptr.ptr
		);

		tracker.connection_update = tracker._lib.declare (
			"tracker_sparql_connection_update",
			ctypes.default_abi,
			ctypes.void_t,
			tracker.Connection.ptr,
			ctypes.char.ptr,
			ctypes.int32_t,
			tracker.Cancellable.ptr,
			tracker.Error.ptr.ptr
		);

		tracker.cursor_next = tracker._lib.declare (
			"tracker_sparql_cursor_next",
			ctypes.default_abi,
			ctypes.bool,
			tracker.Cursor.ptr,
			tracker.Cancellable.ptr,
			tracker.Error.ptr.ptr
		);

		tracker.cursor_get_string = tracker._lib.declare (
			"tracker_sparql_cursor_get_string",
			ctypes.default_abi,
			ctypes.char.ptr,
			tracker.Cursor.ptr,
			ctypes.int,
			ctypes.long.ptr
		);

		tracker.escape_string = tracker._lib.declare (
			"tracker_sparql_escape_string",
			ctypes.default_abi,
			ctypes.char.ptr,
			ctypes.char.ptr
		);

		return true;
	},

	shutdown: function () {
		var tracker = org.bustany.TrackerFox.TrackerSparql;

		if (this._connection) {
			tracker.object_unref(this._connection);
		}

		if (this._lib) {
			this._lib.close ();
		}
	}
}
