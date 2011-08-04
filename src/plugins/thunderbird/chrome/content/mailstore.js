if (!org.bustany.TrackerBird.MailStore || !org.bustany.TrackerBird.MailStore.__initialized)
org.bustany.TrackerBird.MailStore = {
	// Init barrier
	__initialized: true,

	_trackerStore: org.bustany.TrackerBird.TrackerStore,
	_persistentStore: org.bustany.TrackerBird.PersistentStore,
	_ui: org.bustany.TrackerBird.Ui,

	_folderListener: {
		OnItemAdded: function(parentItem, item) {
			dump("Item added\n");
			var store = org.bustany.TrackerBird.MailStore;
			var hdr = item.QueryInterface(Components.interfaces.nsIMsgDBHdr);

			store._queue.addImmediate({
			                           callback: store._indexMessageCallback,
			                           data: hdr
			                          });
		},

		OnItemRemoved: function(parentItem, item) {
			dump("Item removed\n");
			var store = org.bustany.TrackerBird.MailStore;
			var hdr = item.QueryInterface(Components.interfaces.nsIMsgDBHdr);

			store._queue.addImmediate({
			                           callback: store._removeMessageCallback,
			                           data: hdr
			                          });
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
	_removeMessageCallback: null,

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
		this._indexMessageCallback = function(msg) { store.indexMessage(msg); }
		this._removeMessageCallback = function(msg) { store.removeMessage(msg); }

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
			                 data: msg
			                });
		}

		// Close database
		db = null;
	},

	indexMessage: function(msg) {
		var msgContents = this.getMessageContents(msg);
		if (this._trackerStore.storeMessage(msg, msgContents)) {
			this._persistentStore.rememberMessage(msg);
		}

		this._ui.showMessage(this._queue.size() + " items remaining");
	},

	removeMessage: function(msg) {
		this._trackerStore.deleteMessage(msg);
		this._persistentStore.forgetMessage(msg);
	},

	getMessageContents: function(header) {
		var folder = header.folder;
		var contents = "";

		var messenger = Components.classes["@mozilla.org/messenger;1"].
		                createInstance(Components.interfaces.nsIMessenger);
		var uri = folder.getUriForMsg(header);
		var msgService = messenger.messageServiceFromURI(uri);
		var msgStream = Components.classes["@mozilla.org/network/sync-stream-listener;1"].
		                createInstance();
		var consumer = msgStream.QueryInterface(Components.interfaces.nsIInputStream);
		var scriptInput = Components.classes["@mozilla.org/scriptableinputstream;1"].
		                  createInstance();
		var scriptInputStream = scriptInput.
		                        QueryInterface(Components.interfaces.nsIScriptableInputStream);
		scriptInputStream.init(msgStream);

		try {
			msgService.streamMessage(uri, msgStream, null, null, true, null);
		} catch (e) {
			dump("Could not get contents of message " + uri + "\n");
			return null;
		}

		scriptInputStream.available();
		while (scriptInputStream.available()) {
			contents += scriptInputStream.read(1024);
		}

		// Basic html removing
		contents = contents.replace(/<[^>]+?>/g, "");

		return contents;
	},

	shutdown: function() {
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.Remove(this._folderListener);
	}
}
