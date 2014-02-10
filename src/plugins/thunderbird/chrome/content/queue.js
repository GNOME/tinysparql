org.bustany.TrackerBird.Queue = function(delay) {
	this._ui = org.bustany.TrackerBird.Ui;
	this.__console = Components.classes["@mozilla.org/consoleservice;1"].getService(Components.interfaces.nsIConsoleService);
	this._log = function(msg) {
	   this.__console.logStringMessage(msg);
	}

	this._delay = delay;
	this._items = [];
	this._active = false;

	var queue = this;
	this._timerEvent = { notify: function(timer) { queue._active = false; queue.process(); } };
	this._queueTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
	dump("Trackerbird created queue with delay " + delay + "\n");
}

org.bustany.TrackerBird.Queue.prototype.add = function(item) {
	this._items.push(item);
	this.process();
}

org.bustany.TrackerBird.Queue.prototype.addImmediate = function(item) {
	this._items.unshift(item);
	this.process();
}

org.bustany.TrackerBird.Queue.prototype.process = function() {
	if (this._items.length == 0) {
		this._ui.showMessage("indexerIdle");
		return;
	}
	this._ui.showFormattedMessage("actionsRemaining", [this._items.length]);

	if (this._active) {
		return;
	}
	this._active = true;

	var item = this._items.shift();

	try {
		item.callback(item.data);
	} catch (ex) {
		dump ("Trackbird could not execute: " + ex + "\n");
		this._log("Trackerbird could not execute: " + ex);
	}

	this._queueTimer.initWithCallback(this._timerEvent, this._delay, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

org.bustany.TrackerBird.Queue.prototype.size = function() {
	return this._items.length;
}
