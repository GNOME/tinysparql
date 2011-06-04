if (!org.bustany.TrackerBird.Plugin || !org.bustany.TrackerBird.Plugin.__initialized)
org.bustany.TrackerBird.Plugin = {
	// Init barrier
	__initialized: true,

	_mailstore: org.bustany.TrackerBird.MailStore,

	onLoad: function() {
		dump("Initialiazing TrackerBird...\n");

		if (!this.initTracker()) {
			return;
		}

		if (!this._mailstore.init()) {
			dump("Could not initialize mail store\n");
			_mailstore = null;
			return;
		}
	},

	onUnload: function() {
		dump("Shutting down TrackerBird...\n");

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

        var error = new tracker.Error.ptr;
        this._connection = tracker.connection_open (null, error.address());

        if (!error.isNull ()) {
            dump ("Could not initialize Tracker: " + error.contents.message.readString() + "\n");
            tracker.error_free(error);
            return false;
        }

		return true;
	}
}

window.addEventListener("load", function () { org.bustany.TrackerBird.Plugin.onLoad(); }, false);
window.addEventListener("unload", function () { org.bustany.TrackerBird.Plugin.onUnload(); }, false);
