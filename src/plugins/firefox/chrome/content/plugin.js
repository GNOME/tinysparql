if (!org.bustany.TrackerFox.Plugin || !org.bustany.TrackerFox.Plugin.__initialized)
org.bustany.TrackerFox.Plugin={
	// Init barrier
	__initialized: true,

	// Private members
	_connection: null,
	_tracker: org.bustany.TrackerFox.TrackerSparql,
	_bookmarks: null,

	// Methods
	onLoad: function () {
		// initialization code
		this.initialized = true;
		this.strings = document.getElementById ("trackerfox-strings");

		dump ("Initializing TrackerFox...\n");

		if (!this.initTracker ()) {
			dump ("Couldn't initialize Tracker!\n");
			return;
		}

		// The rest of the init is done in onTrackerReady()
	},

	onUnload: function () {
		dump ("Closing TrackerFox...\n");

		var tracker = org.bustany.TrackerFox.TrackerSparql;
		this._bookmarks.shutdown ();
		tracker.shutdown ();
	},

	initTracker: function () {
		var tracker = this._tracker;

		if (!tracker.init ()) {
			dump ("Couldn't initialize tracker bindings, and I don't even know why :'(\n");
			return false;
		}

		var plugin = this;
		var callback_closure = function(source_object, result, user_data) {
			plugin.onTrackerReady(source_object, result, user_data);
		}

	        tracker.readyCallback = tracker.AsyncReadyCallback.ptr(callback_closure)
	        tracker.connection_open_async(null, tracker.readyCallback, null);

		return true;
	},

	onTrackerReady: function(source_object, result, user_data) {
		var tracker = this._tracker;

		var error = new tracker.Error.ptr;
		this._connection = tracker.connection_open_finish (result, error.address());

		if (!error.isNull ()) {
			dump ("Could not initialize Tracker: " + error.contents.message.readString() + "\n");
			tracker.error_free(error);
			return;
		}

		// Tracker is OK, let's continue with the initialization

		this._bookmarks = org.bustany.TrackerFox.Bookmarks;

		if (!this._bookmarks.init (this._connection)) {
			dump ("Couldn't initialize bookmarks service!\n");
			return;
		}

		this._bookmarks.syncBookmarks();
	}
};

window.addEventListener ("load", function () { org.bustany.TrackerFox.Plugin.onLoad (); }, false);
window.addEventListener ("unload", function () { org.bustany.TrackerFox.Plugin.onUnload (); }, false);
