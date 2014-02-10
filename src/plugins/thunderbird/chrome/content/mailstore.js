if (!org.bustany.TrackerBird.MailStore || !org.bustany.TrackerBird.MailStore.__initialized) {

Components.utils.import("resource:///modules/MailUtils.js");

org.bustany.TrackerBird.ContentRetriever = function(header, callback) {
	this._header = header;
	this._callback = callback;
	this._contents = "";
}

org.bustany.TrackerBird.ContentRetriever.prototype = {
	onStartRequest: function(request, ctx) {
	},

	onDataAvailable: function(request, context, inputStream, offset, count) {
	    var scriptableInputStream =
			Components.classes["@mozilla.org/scriptableinputstream;1"].
				createInstance(Components.interfaces.nsIScriptableInputStream);
	    scriptableInputStream.init(inputStream);
	    var data = scriptableInputStream.read(count);
	    this._contents += data;
	},

	onStopRequest: function(request, ctx, status) {
		// Basic html removing
		this._contents = this._contents.replace(/<[^>]+?>/g, "");
		this._callback(this._header, this._contents);
	},

	QueryInterface: function (aIID) {
		if (aIID.equals(Components.interfaces.nsIStreamListener) ||
			aIID.equals(Components.interfaces.nsISupports)) {
			return this;
		}
		throw Components.results.NS_NOINTERFACE;
	}
}

org.bustany.TrackerBird.MailStore = {
	// Init barrier
	__initialized: true,
	__console: Components.classes["@mozilla.org/consoleservice;1"].getService(Components.interfaces.nsIConsoleService),
	_log: function(msg) {
	   this.__console.logStringMessage(msg);
	},

	_trackerStore: org.bustany.TrackerBird.TrackerStore,
	_persistentStore: org.bustany.TrackerBird.PersistentStore,

	_folderListener: {
		OnItemAdded: function(parentItem, item) {
			dump("Trackerbird: new item to be tracked\n");
			var store = org.bustany.TrackerBird.MailStore;
			var hdr = item.QueryInterface(Components.interfaces.nsIMsgDBHdr);

			store._queue.addImmediate({
			                           callback: store._indexMessageCallback,
			                           data: hdr
			                          });
		},

		OnItemRemoved: function(parentItem, item) {
			dump("Trackerbird: item to be untracked\n");
			var store = org.bustany.TrackerBird.MailStore;
			var hdr = item.QueryInterface(Components.interfaces.nsIMsgDBHdr);

			store._queue.addImmediate({
			                           callback: store._removeMessageCallback,
			                           data: hdr
			                          });
		},

		OnItemPropertyChanged: function(item, property, oldValue, newValue) {
		},

		OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {
		},

		OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {
		},

		OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue) {
		},

		OnItemPropertyFlagChanged: function(header, property, oldValue, newValue) {
		},

		OnItemEvent: function(folder, event) {
		}
	},

	_queue: null,
	_walkFolderCallback: null,
	_indexMessageCallback: null,
	_indexMessageContentsCallback: null,
	_removeMessageCallback: null,

	_prefs: null,

	init: function() {
		dump ("Trackerbird initializing mailstore...\n")
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
		this._indexMessageContentsCallback = function(msg, contents) {
			store.indexMessageContents(msg, contents);
		}
		this._removeMessageCallback = function(msg) { store.removeMessage(msg); }

		MailUtils.discoverFolders();
		dump ("Trackerbird mailstore initialized...\n")
		this._log("trackerbird: mailstore initialized");
		return true;
	},

	listAllFolders: function() {
		var store = this;
		var servers = MailServices.accounts.allServers;

		for (var i = 0; i < servers.length; i++) {
			var s = servers.queryElementAt(i, Components.interfaces.nsIMsgIncomingServer);

			var folders = Components.classes["@mozilla.org/array;1"].
			              createInstance(Components.interfaces.nsIMutableArray);
			s.rootFolder.ListDescendants(folders);

			for (var j = 0; j < folders.length; j++) {
				var folder = folders.queryElementAt(j, Components.interfaces.nsIMsgFolder);

				this._queue.add({
					callback: store._walkFolderCallback,
					data: folder
				});
			}
		}

		this._queue.add({
			callback: function() {
				dump("Trackerbird walked all folders\n");
				store._log("trackerbird: walked all folders");
			},
			data: null
		})
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
			var hdr = enumerator.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);

			if (uriCache[folder.getUriForMsg(hdr)]) {
				continue;
			}

			this._queue.add({
			                 callback: this._indexMessageCallback,
			                 data: hdr
			                });
		}

		// Close database
		db = null;
	},

	/**
	 * This method indexes a new message. In order to do this, it has to retrieve its contents
	 * first. The contents is only available as a stream that has to be read first.
	 * When the contents has been read, indexMessageContents is called and the actual
	 * indexing happens.
	 */
	indexMessage: function(header) {
		var folder = header.folder;

		var messenger = Components.classes["@mozilla.org/messenger;1"].
		                createInstance(Components.interfaces.nsIMessenger);
		var uri = folder.getUriForMsg(header);
		var msgService = messenger.messageServiceFromURI(uri);

		// Streaming data into a nsScriptableInputStream and then reading from it here
		// makes thunderbird hang sometimes, so continue asynchronously.
		try {
			var msgStream = new org.bustany.TrackerBird.ContentRetriever(header, this._indexMessageContentsCallback);
			msgService.streamMessage(uri, msgStream, null, null, true, null);
		} catch (ex) {
			dump("Trackerbird could not get contents of message " + ex + "\n");
		}
	},

	/**
	 * Index the message by inserting it into our local store of indexed messages and
	 * into the tracker store.
	 */
	indexMessageContents: function(msg, contents) {
		try {
			if (this._trackerStore.storeMessage(msg, contents)) {
				this._persistentStore.rememberMessage(msg);
			}
		} catch (ex) {
			dump("Trackerbird failed to index message: " + ex + "\n");
			this._log("Trackerbird failed to index message: " + ex);
		}
	},

	removeMessage: function(msg) {
		this._trackerStore.deleteMessage(msg);
		this._persistentStore.forgetMessage(msg);
	},

	shutdown: function() {
		dump ("Trackerbird mailstore store shutting down...\n")
		var mailSession = Components.classes["@mozilla.org/messenger/services/session;1"].
		                  getService(Components.interfaces.nsIMsgMailSession);

		mailSession.Remove(this._folderListener);
		dump ("Trackerbird mailstore store shut down\n")
	}
}
}
