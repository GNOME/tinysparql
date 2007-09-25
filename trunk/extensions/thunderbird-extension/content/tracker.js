//
// tracker.js: Starting point for the Thunderbird extension
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

var gTrackerSettings = Components.classes ['@tracker-project.org/services/settings;1']
	.getService (Components.interfaces.nsITrackerSettings);

function trackerInit ()
{
	dump ("Tracker init started\n");
	
	// Load settings
	gTrackerSettings.init ();
	
	dump ('Adding settings observer...');
	var prefbranch = Components.classes ['@mozilla.org/preferences-service;1']
		.getService (Components.interfaces.nsIPrefBranch2);
	prefbranch.addObserver ('tracker', gSettingObserver, false);
	dump ("Done.\n");
	
	var enabled = gTrackerSettings.getBoolPref ('Enabled');
	if (!enabled) {
		dump ("Tracker backend is now disabled\n");
	} else {
		// Make sure we catch changes
		gTrackerDataTracker.RegisterSelf ();
		
		// The following timeout handler will initiate the indexing process by locating data that
		// needs to be indexed. We delay a while go give Thunderbird some time to settle.
		dump ("Tracker extension is now enabled\n");
		window.setTimeout (function () { gTrackerMainloop.Start (); }, 3000);
	}
	
	updateStatus (enabled);
	dump ("Tracker init ended\n");
}

function onShowSettings (event)
{
	window.openDialog ('chrome://tracker/content/trackerPrefs.xul',
						'PreferenceWindow',
						'chrome,toolbar,modal=yes,resizable=no',
						'pref-indexing');
}

function onStatusbarClick ()
{
	// We invert current running mode
	var enabled = !gTrackerSettings.getBoolPref ('Enabled');
	gTrackerSettings.setBoolPref ('Enabled', enabled);
}

// Update status of the little dog in the corner
function updateStatus (enabled)
{
	var elem = document.getElementById ('tracker-icon');
	var bundle = document.getElementById ('bundle_tracker');
	
	if (enabled) {
		elem.setAttribute ('status', 'enabled');
		elem.setAttribute ('tooltiptext', bundle.getString ('indexingEnabledTooltip'));
	} else {
		elem.setAttribute ('status', 'disabled');
		elem.setAttribute ('tooltiptext', bundle.getString ('indexingDisabledTooltip'));
	}
}

// We use this observer to check if we have been enabled or disabled and if we should restart
// the main loop in case indexing speed changed
var gSettingObserver = {

	observe: function (subject, topic, data)
	{
		var branch = subject.QueryInterface (Components.interfaces.nsIPrefBranch);
		
		if (data == 'tracker.enabled') {
			var enabled = branch.getBoolPref (data);
			
			// Enable or disabled depending on new status
			if (enabled) {
				gTrackerDataTracker.RegisterSelf ();
				gTrackerMainloop.Restart (3);
				dump ("Tracker extension is now enabled\n");
			} else {
				gTrackerDataTracker.UnregisterSelf ();
				gTrackerMainloop.Stop ();
				dump ("Tracker extension is now disabled\n");
			}
			
			updateStatus (enabled);
		} else if (data == 'tracker.index.delay') {
			// In case delay time changed, restart mainloop to get immediate effect. We need to get
			// the new value from included branch as it may not be updated in gTrackerSettings yet.
			gTrackerMainloop.Restart (branch.getIntPref ('tracker.index.delay'));
		}
	}
};

window.addEventListener ('load', trackerInit, false);

