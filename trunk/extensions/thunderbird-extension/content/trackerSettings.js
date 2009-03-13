//
// trackerSettings.js: A convenient way for accessing all tracker settings
//
// Copyright (C) 2007 Pierre Östlund
//

//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

var prefs = Components.classes ['@mozilla.org/preferences-service;1']
	.getService (Components.interfaces.nsIPrefBranch);
var environment = Components.classes ['@mozilla.org/process/environment;1']
	.getService (Components.interfaces.nsIEnvironment);

// We store all available settings here together with access key, default values and type
const TRACKER_SETTINGS = new Array (
	['tracker.enabled',					'Enabled',					true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.destination.directory',	'DestinationDirectory',		getDestinationDirectory (),	Components.interfaces.nsIPrefBranch.PREF_STRING],
	['tracker.index.batch_count',		'IndexBatchCount',			10,		Components.interfaces.nsIPrefBranch.PREF_INT],
	['tracker.index.queue_count',		'IndexQueueCount',			10,		Components.interfaces.nsIPrefBranch.PREF_INT],
	['tracker.index.delay',				'IndexDelay',				1,		Components.interfaces.nsIPrefBranch.PREF_INT],
	['tracker.enable.imap',				'EnableImap',				true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.pop',				'EnablePop',				true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.rss',				'EnableRss',				true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.contacts',			'EnableContacts',			true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.news',				'EnableNews',				true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.mailspool',			'EnableMailspool',			true,	Components.interfaces.nsIPrefBranch.PREF_BOOL],
	['tracker.enable.local',				'EnableLocal',				true,	Components.interfaces.nsIPrefBranch.PREF_BOOL]
);

var loadedPrefs = new Array ();

// The .tracker directory is normally stored in $HOME, but this can be overriden by using the
// $TRACKER_STORAGE environment variable. $HOME can also be overriden by $TRACKER_HOME.
function getDestinationDirectory ()
{
	var directory = '/ThunderbirdEmails';
	var trackerStorage = environment.get ('TRACKER_STORAGE');
	
	if (trackerStorage)
		directory = trackerStorage + directory;
	else {
		var trackerHome = environment.get ('TRACKER_HOME');
		if (trackerHome)
			directory = trackerHome + '/.tracker/' + directory;
		else
			directory = environment.get ('HOME') + '/.xesam/' + directory;
	}
	
	return directory;
}

Component.prototype = {

	reload: function() {
		loader.loadSubScript(SOURCE, this.__proto__);
	},

	QueryInterface: function(aIID) {
		if(!aIID.equals(INTERFACE) && !aIID.equals(Ci.nsISupports))
			throw Cr.NS_ERROR_NO_INTERFACE;
		return this;
	},
	
	init: function ()
	{
		// Load settings
		this.forceLoad ();
		
		// Make sure we catch updates as we should
		var prefbranch = Components.classes ['@mozilla.org/preferences-service;1']
			.getService (Components.interfaces.nsIPrefBranch2);
		prefbranch.removeObserver ('tracker', gObserver);
		prefbranch.addObserver ('tracker', gObserver, false);
	},

	// Use this to force a preference read
	forceLoad: function ()
	{
		for (var i = 0; i < TRACKER_SETTINGS.length; i++) {
			var val = TRACKER_SETTINGS [i] [2];
			
			try {
				var type = TRACKER_SETTINGS [i] [3], domain = TRACKER_SETTINGS [i] [0];
				if (type == Components.interfaces.nsIPrefBranch.PREF_BOOL)
					val = prefs.getBoolPref (domain);
				else if (type == Components.interfaces.nsIPrefBranch.PREF_INT)
					val = prefs.getIntPref (domain);
				else if (type == Components.interfaces.nsIPrefBranch.PREF_STRING)
					val = prefs.getCharPref (domain);
			} catch (ex) {
			}
			
			loadedPrefs [TRACKER_SETTINGS [i] [1]] = val;
		}
	},

	getBoolPref: function (prefName)
	{
		return loadedPrefs [prefName];
	},

	getCharPref: function (prefName)
	{
		return loadedPrefs [prefName];
	},

	getIntPref: function (prefName)
	{
		return loadedPrefs [prefName];
	},

	getDomain: function (prefName)
	{
		for (var i = 0; i < TRACKER_SETTINGS.length; i++) {
			var setting = TRACKER_SETTINGS [i];
			if (setting [1] == prefName)
				return setting [0];
		}
		
		return null;
	},

	setBoolPref: function (prefName, value)
	{
		var domain = this.getDomain (prefName);
		if (!domain)
			throw Components.results.NS_ERROR_FAILURE;
		
		prefs.setBoolPref (domain, value);
	},

	setCharPref: function (prefName, value)
	{
		var domain = this.getDomain (prefName);
		if (!domain)
			throw Components.results.NS_ERROR_FAILURE;
		
		prefs.setCharPref (domain, value);
	},

	setIntPref: function (prefName, value)
	{
		var domain = this.getDomain (prefName);
		if (!domain)
			throw Components.results.NS_ERROR_FAILURE;
		
		prefs.setIntPref (domain, value);
	}

};

// We use this to catch updates
var gObserver = {

	observe: function (subject, topic, data)
	{
		var branch = subject.QueryInterface (Components.interfaces.nsIPrefBranch);
		
		// Find the correct setting so that we can update
		for (var i = 0; i < TRACKER_SETTINGS.length; i++) {
			var domain = TRACKER_SETTINGS [i] [0];
			
			if (domain != data)
				continue;
			
			var key = TRACKER_SETTINGS [i][1], val = loadedPrefs [key];
			
			try {
				switch (branch.getPrefType (data)) {
				case Components.interfaces.nsIPrefBranch.PREF_BOOL:
					val = branch.getBoolPref (data);
					break;
				case Components.interfaces.nsIPrefBranch.PREF_INT:
					val = branch.getIntPref (data);
					break;
				case Components.interfaces.nsIPrefBranch.PREF_STRING:
					val = Branch.getCharPref (data);
					break;
				}
			} catch (ex) {
			}
			
			loadedPrefs [key] = val;
			
			break;
		}
	}
};

