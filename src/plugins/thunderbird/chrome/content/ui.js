if (!org.bustany.TrackerBird.Ui || !org.bustany.TrackerBird.Ui.__initialized)
org.bustany.TrackerBird.Ui = {
	// Init barrier
	__initialized: true,

	_statusPanel: null,

	init: function() {
		this._statusPanel = document.getElementById("trackerbird-status-panel");
		this._strbundle = document.getElementById("trackerbird-strings");

		return true;
	},

	showMessage: function(str) {
		this._statusPanel.label = this._strbundle.getString(str);
	},

	showFormattedMessage: function(str, repls) {
		this._statusPanel.label = this._strbundle.getFormattedString(str, repls);
	}
}
