org.bustany.TrackerBird.Queue = function(delay) {
	this._delay = 100;
	this._items = [];
	this._active = false;

	var queue = this;
	this._timerEvent = { notify: function(timer) { queue._active = false; queue.process(); } };
	this._queueTimer = null;
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
	if (this._active) {
		return;
	}

	if (this._items.length == 0) {
		return;
	}

	this._active = true;

	var item = this._items.shift();

	item.callback(item.data);

	this._queueTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
	this._queueTimer.initWithCallback(this._timerEvent, this._delay, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

org.bustany.TrackerBird.Queue.prototype.size = function() {
	return this._items.length;
}
