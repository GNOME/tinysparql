//
// beagle.js: Starting point for the Thunderbird extension
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

var gBeagleSettings = Components.classes ['@beagle-project.org/services/settings;1']
	.getService (Components.interfaces.nsIBeagleSettings);

function beagleInit ()
{
	dump ("Beagle init started\n");
	
	// Load settings
	gBeagleSettings.init ();
	
	dump ('Adding settings observer...');
	var prefbranch = Components.classes ['@mozilla.org/preferences-service;1']
		.getService (Components.interfaces.nsIPrefBranch2);
	prefbranch.addObserver ('beagle', gSettingObserver, false);
	dump ("Done.\n");
	
	var enabled = gBeagleSettings.getBoolPref ('Enabled');
	if (!enabled) {
		dump ("Beagle backend is now disabled\n");
	} else {
		// Make sure we catch changes
		gBeagleDataTracker.RegisterSelf ();
		
		// The following timeout handler will initiate the indexing process by locating data that
		// needs to be indexed. We delay a while go give Thunderbird some time to settle.
		dump ("Beagle extension is now enabled\n");
		window.setTimeout (function () { gBeagleMainloop.Start (); }, 3000);
	}
	
	updateStatus (enabled);
	dump ("Beagle init ended\n");
}

function onShowSettings (event)
{
	window.openDialog ('chrome://beagle/content/beaglePrefs.xul',
						'PreferenceWindow',
						'chrome,toolbar,modal=yes,resizable=no',
						'pref-indexing');
}

function onStatusbarClick ()
{
	// We invert current running mode
	var enabled = !gBeagleSettings.getBoolPref ('Enabled');
	gBeagleSettings.setBoolPref ('Enabled', enabled);
}

// Update status of the little dog in the corner
function updateStatus (enabled)
{
	var elem = document.getElementById ('beagle-icon');
	var bundle = document.getElementById ('bundle_beagle');
	
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
		
		if (data == 'beagle.enabled') {
			var enabled = branch.getBoolPref (data);
			
			// Enable or disabled depending on new status
			if (enabled) {
				gBeagleDataTracker.RegisterSelf ();
				gBeagleMainloop.Restart (3);
				dump ("Beagle extension is now enabled\n");
			} else {
				gBeagleDataTracker.UnregisterSelf ();
				gBeagleMainloop.Stop ();
				dump ("Beagle extension is now disabled\n");
			}
			
			updateStatus (enabled);
		} else if (data == 'beagle.index.delay') {
			// In case delay time changed, restart mainloop to get immediate effect. We need to get
			// the new value from included branch as it may not be updated in gBeagleSettings yet.
			gBeagleMainloop.Restart (branch.getIntPref ('beagle.index.delay'));
		}
	}
};

window.addEventListener ('load', beagleInit, false);

