if (!org.bustany.TrackerBird.Plugin || !org.bustany.TrackerBird.Plugin.__initialized)
org.bustany.TrackerBird.Plugin = {
	// Init barrier
	__initialized: true,

	_trackerConnection: null,
	_mailstore: org.bustany.TrackerBird.MailStore,
	_persistentstore: org.bustany.TrackerBird.PersistentStore,
	_trackerstore: org.bustany.TrackerBird.TrackerStore,
	_ui: org.bustany.TrackerBird.Ui,

	onLoad: function() {
		dump("Initialiazing TrackerBird...\n");

		if (!this._ui.init()) {
			dump("Could not initialize Ui\n");
			return;
		}

		if (!this.initTracker()) {
			this._ui.showMessage("Cannot initialize Tracker");
			return;
		}

		// Rest of the init is done in onTrackerReady
	},

	onUnload: function() {
		dump("Shutting down TrackerBird...\n");

		if (this._persistentstore) {
			this._persistentstore.shutdown();
		}

		if (this._mailstore) {
			this._mailstore.shutdown();
		}
	},

	initTracker: function() {
		var tracker = org.bustany.TrackerBird.TrackerSparql;

		if (!tracker.init()) {
			dump("Could not load Tracker libraries\n");
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
		var tracker = org.bustany.TrackerBird.TrackerSparql;

        var error = new tracker.Error.ptr;
        this._trackerConnection = tracker.connection_open_finish (result, error.address());

        if (!error.isNull ()) {
            dump ("Could not initialize Tracker: " + error.contents.message.readString() + "\n");
			this._ui.showMessage("Cannot connect to Tracker");
            tracker.error_free(error);
			return;
        }

		// Tracker is ready, proceed with the rest of the init

		this._ui.showMessage("Initializing...");

		if (!this._persistentstore.init()) {
			this._ui.showMessage("Cannot initialize persistent storage");
			dump("Could not initialize Persistent store\n");
			_persistentstore = null;
			return;
		}

		if (!this._trackerstore.init(this._trackerConnection)) {
			dump("Could not initialize Tracker store\n");
			_trackerstore = null;
			return;
		}

		if (!this._mailstore.init()) {
			this._ui.showMessage("Cannot initialize mail store");
			dump("Could not initialize mail store\n");
			_mailstore = null;
			return;
		}

		this._mailstore.listAllFolders();
	}
}

window.addEventListener("load", function () { org.bustany.TrackerBird.Plugin.onLoad(); }, false);
window.addEventListener("unload", function () { org.bustany.TrackerBird.Plugin.onUnload(); }, false);
