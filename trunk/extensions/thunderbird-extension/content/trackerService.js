//
// trackerService.js: This is the entry point of the extension
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

var tracker_init = false;
var gTrackerSettings = null;
var gTrackerIndexer = null;
var gTrackerQueue = null;
var gTrackerTimer = Components.classes ["@mozilla.org/timer;1"]
	.createInstance (Components.interfaces.nsITimer);

var running = false;
var warnings = new Array (); // We will only hold javascript strings here

function notify (data)
{
	var observer = Components.classes['@mozilla.org/observer-service;1']
		.getService(Components.interfaces.nsIObserverService);
	observer.notifyObservers (null, "tracker-service", data);
}

function initService ()
{
	if (tracker_init)
		return;
	
	dump ("Tracker init started\n");
	
	// Set our globally needed variables
	gTrackerSettings = GetJsService ('@tracker-project.org/services/settings;1');
	gTrackerIndexer = GetJsService ('@tracker-project.org/services/indexer;1');
	gTrackerQueue = GetJsService ('@tracker-project.org/services/queue;1');
	tracker_init = true;
	
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
		restart (3);
	}
	
	notify (running ? 'update-enabled' : 'update-disabled');
	dump ("Tracker init ended\n");
}

function start ()
{
	if (!tracker_init)
		initService ();
	
	gTrackerTimer.cancel ();
	gTrackerTimer.initWithCallback (gTrackerMainLoop,
		gTrackerSettings.getIntPref ('IndexDelay') * 1000,
		Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
	running = true;
}

function stop (disable)
{
	if (!tracker_init)
		initService ();
	
	gTrackerTimer.cancel ();
	running = false;
	
	if (disable) {
		try {
			gTrackerSettings.setBoolPref ('Enabled', false);
		} catch (ex) {
			dump ("Failed to disable tracker extension!\n");
		}
	}
}

function restart (seconds)
{
	if (!tracker_init)
		initService ();
		
	gTrackerTimer.cancel ();
	gTrackerTimer.initWithCallback (gTrackerMainLoop, 
		seconds * 1000,
		Components.interfaces.nsITimer.TYPE_REPEATING_SLACK);
	running = true;
}

function addWarning (text, params, length)
{
	if (!text)
		return;

	var bundleService = Components.classes ['@mozilla.org/intl/stringbundle;1']
		.getService (Components.interfaces.nsIStringBundleService);
	var bundle = bundleService.createBundle ('chrome://tracker/locale/strings.properties');
	
	try {
		if (params == null)
			warnings [warnings.length] = bundle.GetStringFromName (text);
		else 
			warnings [warnings.length] = bundle.formatStringFromName (text, params, length);
	} catch (ex) {
		warnings [warnings.length] = "Failed to add error message! You should report this as a bug. Details: " + ex;
	}
	
	notify ('new-warning');
}

function getLastWarning ()
{
	var str = null;

	for (var i = 0; i < warnings.length; i++) {
		if (warnings [i] != null) {
			str = warnings [i];
			warnings [i] = null;
			break;
		}
	}

	// Make sure we notify if we have no more warnings
	if (getWarningCount () == 0) {
		notify ('warnings-empty');
		warnings = new Array ();
	}
	
	return str;
}

function getWarningCount ()
{
	var count = 0;
	
	for (var i = 0; i < warnings.length; i++) {
		if (warnings [i] != null)
			count++;
	}
	
	return count;
}

function clearWarnings ()
{
	warnings = new Array ();
	notify ('warnings-empty');
}

var gTrackerMainLoop = {

	notify: function (timer)
	{
		// Make sure we have a destination directory. If it doesn't exists, create it and mark 
		// everything as not indexed.
		try {
			// Check if destination directory exists
			var dir = Components.classes ["@mozilla.org/file/local;1"]
				.createInstance (Components.interfaces.nsILocalFile);
			dir.initWithPath (gTrackerSettings.getCharPref ('DestinationDirectory'));
			if (dir.exists ()) {
				if (!dir.isDirectory ()) {
					dump ("Destination directory exists but is not a directory! Bailing out!\n");
					addWarning ('destinationDirInvalidType', null, 0);
					stop (true);
					return;
				}
                        }
				
				// We need to create the ToIndex directory in case it doesn't exist
				dir.initWithPath (gTrackerSettings.getCharPref ('DestinationDirectory') + '/ToIndex');
				if (!dir.exists ()) {
					// We create this directory and mark all content as not indexed
					dir.create (Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
					gTrackerUnindex.UnindexEverything (false);
				} else if (dir.isFile ()) {
					dump ("The ToIndex directory exists but is not a directory!\n");
					addWarning ('toIndexDirInvalidType', null, 0);
					stop (true);
					return;
				} else if (!dir.isWritable ()) {
					dump ("The ToIndex directory exists but is not writable!\n");
					addWarning ('toIndexDirectoryLackPermission', null, 0);
					stop (true);
					return;
				}
               // Commented, if the directory does not exists it will be created by plugin
		//	} else {
		//		dump ("Destination directory does not exist!\n");
		//		addWarning ('destinationDirectoryNoExist', null, 0);
		//		stop (true);
		//		return;
		//	}
		} catch (ex) {
			dump ("Failed to create ToIndex directory! Bailing out! (" + ex + ")\n");
			addWarning ('failedCreateToIndexDir', [ex], 1);
			stop (true);
			return;
		}

		// Index next set
		try {
			gTrackerDataCollector.Process ();
		} catch (ex) {
			dump ("Error while indexing: " + ex + "\n");
			addWarning ('errorWhileIndexing', [ex], 1);
		}
		
		gTrackerTimer.delay = gTrackerSettings.getIntPref ('IndexDelay') * 1000;
	}
};

// We use this observer to check if we have been enabled or disabled and if we should restart
// the main loop in case indexing speed changed. We also make sure the GUI part gets to know this
// so that the little dog icon can be updated.
var gSettingObserver = {

	observe: function (subject, topic, data)
	{
		var branch = subject.QueryInterface (Components.interfaces.nsIPrefBranch);
		
		if (data == 'tracker.enabled') {
			var enabled = branch.getBoolPref (data);
			
			// Enable or disabled depending on new status
			if (enabled) {
				gTrackerDataTracker.RegisterSelf ();
				restart (3);
				dump ("Tracker extension is now enabled\n");
			} else {
				gTrackerDataTracker.UnregisterSelf ();
				stop (true);
				dump ("Tracker extension is now disabled\n");
			}
			
			notify (running ? 'update-enabled' : 'update-disabled');
		} else if (data == 'tracker.index.delay') {
			// In case delay time changed, restart mainloop to get immediate effect. We need to get
			// the new value from included branch as it may not be updated in gTrackerSettings yet.
			restart (branch.getIntPref ('tracker.index.delay'));
		}
	}
};

var gTrackerDataTracker = {

	Notifications: Components.classes ['@mozilla.org/messenger/msgnotificationservice;1']
		.getService(Components.interfaces.nsIMsgFolderNotificationService),

	RegisterSelf: function ()
	{
		dump ('Registering tracker data tracker...');
		this.Notifications.removeListener (this);
		this.Notifications.addListener (this);
		dump ("Done.\n");
	},
	
	UnregisterSelf: function ()
	{
		dump ('Unregistering tracker data tracker...');
		this.Notifications.removeListener (this);
		dump ("Done.\n");
	},
	
	//
	//	Below are the functios that will get all updates
	//
	
	// Message added
	itemAdded: function (item)
	{
		if (!item)
			return;
		
		dump ("Adding new messages and restarting loop\n");
		restart (10);
		
		// New approach here: mark folder as not indexed and let main loop find new messages.
		// Doing it this way will make sure we don't hammer the system since we take
		// advantage of the indexing queue.
		try {
			var hdr = item.QueryInterface (Components.interfaces.nsIMsgDBHdr);
			if (!hdr)
				return;
			
			gTrackerIndexer.resetFolder (hdr.folder, false, false, false);
		} catch (ex) {
		}
	},
	
	// Message _or_ folder removed
	itemDeleted: function (item)
	{
		dump ("Removing message(s) or folder and restarting loop\n");
		restart (10);
		if (item instanceof Components.interfaces.nsIMsgDBHdr) {
			gTrackerQueue.removeHdr (item)
		} else if (item instanceof Components.interfaces.nsIMsgFolder) {
			gTrackerQueue.removeFolder (item)
		}
	},
	
	itemMoveCopyCompleted: function (move, items, dest)
	{
		dump ("Moving/copying message(s)/folder and restarting loop\n");
		restart (10);
		
		// There can be at most one folder in "items", so we check for that
		var folder = items.GetElementAt (0);
		if (folder instanceof Components.interfaces.nsIMsgFolder) {
			
			// We have a folder. This is an ugly solution...
			var finished = false;
			var enumerator = dest.GetSubFolders ();
			while (!finished) {
				var f = enumerator.currentItem ().QueryInterface (Components.interfaces.nsIMsgFolder);
				if (f.name == folder.name) {
					// We do this recursively to ensure all sub-content are re-indexed too
					gTrackerIndexer.resetFolder (f, false, true, true);
					break;
				}
			
				try { enumerator.next (); }
				catch (ex) { finished = true; }
			}
			
			// If we moved the folder, then we also have to remove the source folder. Otherwise
			// we'll end up with messages and folders that doesn't exist
			if (move)
				gTrackerQueue.removeFolder (folder);
		} else {
			// We have a bunch of messages. Reset all messages.
			for (var i = 0; i < items.Count (); i++) {
				var message = items.QueryElementAt (i, Components.interfaces.nsIMsgDBHdr);
				gTrackerIndexer.resetHdr (message, false);
			}
			
			// Also make sure we reset the destination folder, otherwise the new messages
			// won't be caught by the main loop
			gTrackerIndexer.resetFolder (dest, false, false, true);
		}
	},
	
	folderRenamed: function (oldFolder, newFolder)
	{
		dump ("Renaming folder and restarting loop\n");
		restart (10);
		gTrackerQueue.moveFolder (oldFolder, newFolder);
	},
	
	itemEvent: function (item, event, data)
	{
	}
};

var gTrackerDataCollector = {

	GetNextFolder: function ()
	{
		var gAccountManager = Components.classes ['@mozilla.org/messenger/account-manager;1']
			.getService (Components.interfaces.nsIMsgAccountManager);
		var accounts = gAccountManager.accounts;
		
		for (var i = 0; i < accounts.Count (); i++) {
			var account = accounts.QueryElementAt (i, Components.interfaces.nsIMsgAccount);
			
			// This check the overall type
			if (!gTrackerIndexer.shouldIndexAccount (account)) 
				continue;
			else if (!account.incomingServer) { // Invalid accounts might exist for some reason
				var email = "Unknown";
				
				if (account.defaultIdentity)
					email = account.defaultIdentity.email;
				
				dump (account.key + ' does not have an incoming server! Sender address: ' + email + "\n");
				continue;
			}
			
			var allFolders = Components.classes ['@mozilla.org/supports-array;1']
				.createInstance (Components.interfaces.nsISupportsArray);
			account.incomingServer.rootFolder.ListDescendents (allFolders);
			
			for (var j = 0; j < allFolders.Count (); j++) {
				var folder = allFolders.QueryElementAt (j, Components.interfaces.nsIMsgFolder);
				
				// We don't bother if there's nothing to index
				if (folder.getTotalMessages (false) == 0)
					continue;

				// We only need to index a folder if it isn't already indexed and if the user
				// hasn't explicitly excluded it
				if (!gTrackerIndexer.isFolderIndexed (folder) && gTrackerIndexer.shouldIndexFolder (folder))
					return folder;
			}
		}
		
		return null;
	},
	
	// Some times Thunderbird lists "invalid" folders. An invalid folder is a folder where all messages have
	// been downloaded but the mork database is not available. This probably only happen if a user is messing
	// around with the file structure manually.
	ValidFolder: function ()
	{
		var filePath = this.CurrentFolder.path.unixStyleFilePath + '.msf';

		try {
			var file = Components.classes ['@mozilla.org/file/local;1']
				.createInstance (Components.interfaces.nsILocalFile);
			if (!file)
				return false;
			
			file.initWithPath (filePath);
		} catch (ex) {
			return false;
		}
		
		return file.exists ();
	},

	// Add new mails to the indexing queue
	Process: function ()
	{
		// If we don't have a folder available at this time, get next available
		if (!this.CurrentFolder)
			this.CurrentFolder = this.GetNextFolder ();
			
		// Note that we don't have any folders left to index if GetNextFolder returned null
		if (!this.CurrentFolder) {
			gTrackerQueue.forceProcess ();
			stop (false);
			return;
		}
		
		dump ('Processing messages in ' + this.CurrentFolder.prettyName + "\n");
		
		// We have a valid folder to enumerate over, make sure we have a valid enumerator as well
		if (this.CurrentEnumerator == null) {
			try {
				this.CurrentEnumerator = this.CurrentFolder.getMessages (null);
			} catch (ex) {
				// Only display the error message to the user in case the folder is valid and
				// messages could not be listed
				if (this.ValidFolder ()) {
					dump ('Failed to list messages in ' + this.CurrentFolder.prettyName + ': ' + ex + "\n");
					addWarning ('failedListingMessages', [this.CurrentFolder.prettyName, ex], 2);
				}
				gTrackerIndexer.markFolderAsIndexed (this.CurrentFolder);
				this.CurrentFolder = null;
				return;
			}
		}
		
		// Process items. We skip already indexed items.
		var batchCounter = gTrackerSettings.getIntPref ('IndexBatchCount');
		while (batchCounter > 0 && this.CurrentEnumerator.hasMoreElements ()) {
			var hdr = this.CurrentEnumerator.getNext ().QueryInterface (Components.interfaces.nsIMsgDBHdr);
			if (!hdr || gTrackerIndexer.isHdrIndexed (hdr))
				continue;
			
			// We only count down the counter in case we actually did add the message (since a
			// filter could have picked this up)		
			if (gTrackerQueue.addHdr (hdr)) 
				batchCounter--;
		}
		
		// We might have missed items in case the database content changed, so we set the enumerator
		// to null to make sure we enumerate the same database again. We keep doing this until
		// batchCounter does not change, this way we'll know that everything in this mailbox has
		// been indexed and that we can move on.
		if (!this.CurrentEnumerator.hasMoreElements ()) {
			// We are done and mark this folder to reflect this. Doing so will make it a lot faster
			// finding not already indexed folders and we don't have to keep track of this somewhere
			// else. It's also stored across sessions.
			gTrackerIndexer.markFolderAsIndexed (this.CurrentFolder);
			this.CurrentFolder.getMsgDatabase (null).Commit (1);
			dump ('Finished indexing ' + this.CurrentFolder.prettyName + "\n");
			this.CurrentFolder = null;
		}
		this.CurrentEnumerator = null;
	},
	
	CurrentFolder: null,
	CurrentEnumerator: null
};

