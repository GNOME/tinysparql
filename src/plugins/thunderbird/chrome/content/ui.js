if (!org.bustany.TrackerBird.Ui || !org.bustany.TrackerBird.Ui.__initialized)
org.bustany.TrackerBird.Ui = {
	// Init barrier
	__initialized: true,

	_statusPanel: null,

	init: function() {
		this._statusPanel = document.getElementById("trackerbird-status-panel");

		return true;
	},

	showMessage: function(str) {
		this._statusPanel.label = str;
	}
}
