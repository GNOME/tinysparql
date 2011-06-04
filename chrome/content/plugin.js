if (!org.bustany.TrackerBird.Plugin || !org.bustany.TrackerBird.Plugin.__initialized)
org.bustany.TrackerBird.Plugin = {
	// Init barrier
	__initialized: true,

	_mailstore: org.bustany.TrackerBird.MailStore,

	onLoad: function() {
		dump("Initialiazing TrackerBird...\n");

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
	}
}

window.addEventListener("load", function () { org.bustany.TrackerBird.Plugin.onLoad(); }, false);
window.addEventListener("unload", function () { org.bustany.TrackerBird.Plugin.onUnload(); }, false);
