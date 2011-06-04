if (!org.bustany.TrackerBird.MailStore || !org.bustany.TrackerBird.MailStore.__initialized)
org.bustany.TrackerBird.MailStore = {
	// Init barrier
	__initialized: true,


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

	_folderQueue: null,
	_messageQueue: null,

	init: function() {
		// To get notifications
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.AddFolderListener(this._folderListener,
		                              Components.interfaces.nsIFolderListener.all);

		var store = this;
		this._folderQueue = new org.bustany.TrackerBird.Queue(function(folder) { store.walkFolder(folder); }, 100 /* ms */),
		this._messageQueue = new org.bustany.TrackerBird.Queue(function(msg) { store.indexMessage(msg); }, 100 /* ms */),

		this.listAllFolders();

		return 1;
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

				this._folderQueue.add(folder);
			}
		}
	},

	walkFolder: function(folder) {
		dump("Walking folder " + folder.prettiestName + "\n");

		var db = folder.msgDatabase;
		var enumerator = db.EnumerateMessages();

		while (enumerator.hasMoreElements()) {
			var msg = enumerator.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);

			dump("Message " + folder.getUriForMsg(msg) + "\n");
		}

		// Close database
		db = null;
	},

	shutdown: function() {
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.Remove(this._folderListener);
	}
}
