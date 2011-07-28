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

		this._bookmarks = org.bustany.TrackerFox.Bookmarks;

		if (!this._bookmarks.init (this._connection)) {
			dump ("Couldn't initialize bookmarks service!\n");
			return;
		}

		this._bookmarks.syncBookmarks();
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

		var error = new tracker.Error.ptr;
		this._connection = tracker.connection_open (null, error.address());

		if (!error.isNull ()) {
			dump ("Could not initialize Tracker: " + error.contents.message.readString() + "\n");
			tracker.error_free(error);
			return false;
		}

		return true;
	}
};

window.addEventListener ("load", function () { org.bustany.TrackerFox.Plugin.onLoad (); }, false);
window.addEventListener ("unload", function () { org.bustany.TrackerFox.Plugin.onUnload (); }, false);
