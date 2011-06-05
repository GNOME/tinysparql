if (!org.bustany.TrackerBird.MailStore || !org.bustany.TrackerBird.MailStore.__initialized)
org.bustany.TrackerBird.MailStore = {
	// Init barrier
	__initialized: true,

	_trackerStore: org.bustany.TrackerBird.TrackerStore,
	_persistentStore: org.bustany.TrackerBird.PersistentStore,

	_folderListener: {
		OnItemAdded: function(parentItem, item) {
			dump("Item added\n");
		},

		OnItemRemoved: function(parentItem, item) {
			dump("Item removed\n");
		},

		OnItemPropertyChanged: function(item, property, oldValue, newValue) {
			dump("Item property changed\n");
		},

		OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {
			dump("Item property changed\n");
		},

		OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {
			dump("Item property changed\n");
		},

		OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue) {
			dump("Item property changed\n");
		},

		OnItemPropertyFlagChanged: function(header, property, oldValue, newValue) {
			dump("Item flag changed\n");
		},

		OnItemEvent: function(folder, event) {
			dump("Item event " + event + " " + folder + "\n");
		}
	},

	_queue: null,
	_walkFolderCallback: null,
	_indexMessageCallback: null,

	_prefs: null,

	init: function() {
		// To get notifications
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.AddFolderListener(this._folderListener,
		                              Components.interfaces.nsIFolderListener.all);

		this._prefs = Components.classes["@mozilla.org/preferences-service;1"]
		              .getService(Components.interfaces.nsIPrefService).getBranch("extensions.trackerbird.");

		var store = this;
		this._queue = new org.bustany.TrackerBird.Queue(this._prefs.getIntPref("indexDelay")),
		this._walkFolderCallback = function(item) { store.walkFolder(item); }
		this._indexMessageCallback = function(item) { store.indexMessage(item); }

		this.listAllFolders();

		return true;
	},

	listAllFolders: function() {
		var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].
		                     getService(Components.interfaces.nsIMsgAccountManager);

		var servers = accountManager.allServers;

		for (var i = 0; i < servers.Count(); i++) {
			var s = servers.QueryElementAt(i, Components.interfaces.nsIMsgIncomingServer);

			var folders = Components.classes["@mozilla.org/supports-array;1"].
			              createInstance(Components.interfaces.nsISupportsArray);

			s.rootFolder.ListDescendents(folders);

			for (var j = 0; j < folders.Count(); j++) {
				var folder = folders.GetElementAt(j).QueryInterface(Components.interfaces.nsIMsgFolder);

				var store = this;
				this._queue.add({
				                 callback: this._walkFolderCallback,
				                 data: folder
				                });
			}
		}
	},

	walkFolder: function(folder) {
		dump("Walking folder " + folder.prettiestName + "\n");

		var db = folder.msgDatabase;
		var enumerator = db.EnumerateMessages();
		var knownUris = this._persistentStore.getUrisForFolder(folder);

		var uriCache = {};

		for (var i in knownUris) {
			uriCache[knownUris[i]] = true;
		}

		knownUris = null;

		while (enumerator.hasMoreElements()) {
			var msg = enumerator.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);

			if (uriCache[folder.getUriForMsg(msg)]) {
				continue;
			}

			this._queue.add({
			                 callback: this._indexMessageCallback,
			                 data: {folder: folder, msg: msg}
			                });
		}

		// Close database
		db = null;
	},

	indexMessage: function(item) {
		if (this._trackerStore.storeMessage(item.folder, item.msg)) {
			this._persistentStore.rememberMessage(item.folder, item.msg);
		}

		document.getElementById("trackerbird-status-panel").label = this._queue.size() + " items remaining";
	},

	shutdown: function() {
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.Remove(this._folderListener);
	}
}
