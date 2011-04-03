if (!org.bustany.TrackerFox.Bookmarks || !org.bustany.TrackerFox.Bookmarks.__initialized)
org.bustany.TrackerFox.Bookmarks = {
	__initialized: true,

	_tracker: org.bustany.TrackerFox.TrackerSparql,
	_connection: null,
	_dataSourceUrn: "urn:nepomuk:datasource:4754847fa79e41c6badc0bfc69e324d2",

	_bmService: null,
	_histService: null,

	init: function (connection) {
		this._connection = connection;

		this._bmService = Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"]
		    .getService (Components.interfaces.nsINavBookmarksService);
		this._histService = Components.classes["@mozilla.org/browser/nav-history-service;1"]
		    .getService (Components.interfaces.nsINavHistoryService);

		if (!this.insertDataSource ()) {
			return false;
		}

		return true;
	},

	insertDataSource: function () {
		var tracker = this._tracker;

		var error = new tracker.Error.ptr;
		tracker.connection_update (this._connection,
		                           "INSERT { <" + this._dataSourceUrn + "> " +
		                           "a nie:DataSource; " +
		                           "rdfs:label \"TrackerFox\"}",
		                           0,
		                           null,
		                           error.address ());

		if (!error.isNull ()) {
			this.handleError (error, "Can't insert DataSource");
			return false;
		}

		return true;
	},

	syncBookmarks: function () {
		var localBookmarks = this.getLocalBookmarks();
		var trackerBookmarks = this.getTrackerBookmarks ();

		// Quirky way of detecting an error
		if (trackerBookmarks.length == 1 && trackerBookmarks[0] == null) {
			return false;
		}

		var trackerDict = {};
		var newBookmarks = new Array ();
		var updatedBookmarks = new Array();
		var deletedBookmarks = new Array();

		for (var i in trackerBookmarks) {
			var bookmark = trackerBookmarks[i]
			trackerDict[bookmark.identifier] = bookmark;
		}

		for (var i = localBookmarks.length - 1; i >= 0; --i) {
			var local = localBookmarks[i];
			var id = local.itemId.toString();

			if (!trackerDict[id]) {
				newBookmarks.push (local);
				continue;
			}

			var remote = trackerDict[id];

			// We divide by 1000000 here since Tracker's resolution is second
			if (Math.round(local.lastModified / 1000000) > (this.dateToPRTime(remote.lastModified) / 1000000)) {
				updatedBookmarks.push (local);
			}

			trackerDict[id] = null;
		}

		for (var id in trackerDict) {
			if (trackerDict[id]) {
				deletedBookmarks.push (trackerDict[id]);
			}
		}

		this.insertBookmarks (newBookmarks);
		this.updateBookmarks (updatedBookmarks);
		this.deleteBookmarks (deletedBookmarks);
	},

	getLocalBookmarks: function (folder) {
		var bookmarks = this._bmService;
		var history = this._histService;

		const foldersToList = [bookmarks.toolbarFolder,
		                       bookmarks.bookmarksMenuFolder,
		                       bookmarks.unfiledBookmarksFolder];

		var query = history.getNewQuery ();
		query.setFolders (foldersToList, foldersToList.length);
		var options = history.getNewQueryOptions ();
		options.queryType = options.QUERY_TYPE_BOOKMARKS;
		var result = history.executeQuery (query, options);
		var root = result.root;
		root.containerOpen = true;

		var bookmarks = new Array ();

		for (var i = 0; i < root.childCount; ++i) {
			bookmarks.push (root.getChild (i));
		}

		return bookmarks;
	},

	// Returns a list of
	// {
	//     urn: xxx
	//     identifier: xxx
	//     lastModified: xxx
	// }
	//
	// or [ null ] in case of error
	getTrackerBookmarks: function () {
		var tracker = this._tracker;

		var error = new tracker.Error.ptr;

		var cursor = tracker.connection_query (this._connection,
		                                       "SELECT ?b nao:identifier(?b) nie:contentLastModified(?b) " +
		                                       "{ ?b a nfo:Bookmark ; " +
		                                       "nie:dataSource <" + this._dataSourceUrn + ">}",
		                                       null,
		                                       error.address ());

		if (!error.isNull ()) {
			this.handleError (error, "Can't get bookmarks from Tracker");
			return [null];
		}

		var bookmarks = new Array ();

		while (tracker.cursor_next (cursor, null, error.address())) {
			if (!error.isNull ()) {
				this.handleError (error, "Can't get bookmarks from Tracker")
				return [null];
			}

			var bookmark = {
				urn: tracker.cursor_get_string (cursor, 0, null).readString (),
				identifier: tracker.cursor_get_string (cursor, 1, null).readString (),
				lastModified: tracker.cursor_get_string (cursor, 2, null).readString ()
			};

			bookmarks.push (bookmark);
		}

		return bookmarks;
	},

	insertBookmarks: function (bookmarks) {
		if (bookmarks.length == 0) {
			return;
		}

		var tracker = this._tracker;

		var globalQuery = "";

		for (var i in bookmarks) {
			var bookmark = bookmarks[i];

			var iri = "<urn:trackerfox:bookmark:" + bookmark.itemId + ">";

			var query = "INSERT { " + iri + " a nfo:Bookmark, nie:DataObject; " +
			            "nie:dataSource <" + this._dataSourceUrn + ">; " +
			            "nie:usageCounter " + bookmark.accessCount + "; " +
			            "nie:contentCreated \"" + this.PRTimeToDate (bookmark.dateAdded).toISOString () + "\"; " +
			            "nao:identifier \"" + bookmark.itemId + "\"; " +
			            "nie:contentLastModified \"" + this.PRTimeToDate (bookmark.lastModified).toISOString () + "\"; " +
			            "nie:url \"" + bookmark.uri + "\"";
			if (bookmark.time != 0) {
				query += "; nie:contentAccessed \"" + this.PRTimeToDate (bookmark.time).toISOString () + "\"";
			}

			if (bookmark.title != "") {
				query += "; nie:title \"" + tracker.escape_string (bookmark.title).readString () + "\"";
			}

			query += "}\n\n";

			globalQuery += query;
		}

		var error = new tracker.Error.ptr;
		tracker.connection_update (this._connection,
		                           globalQuery,
		                           0,
		                           null,
		                           error.address ());

		if (!error.isNull ()) {
			this.handleError (error, "Can't save bookmarks");
			return;
		}
	},

	updateBookmarks: function (bookmarks) {
		if (bookmarks.length == 0) {
			return;
		}

		// FIXME: Not optimal
		this.deleteBookmarks (bookmarks);
		this.insertBookmarks (bookmarks);
	},

	deleteBookmarks: function (bookmarks) {
		if (bookmarks.length == 0) {
			return;
		}

		var tracker = this._tracker;

		var globalQuery = "";

		for (var i in bookmarks) {
			var bookmark = bookmarks[i];

			var iri = "<urn:trackerfox:bookmark:" + bookmark.itemId + ">";

			globalQuery += "DELETE {" + iri + " a rdfs:Resource}\n\n";
		}

		var error = new tracker.Error.ptr;
		tracker.connection_update (this._connection,
		                           globalQuery,
		                           0,
		                           null,
		                           error.address ());

		if (!error.isNull ()) {
			this.handleError (error, "Can't save bookmarks");
			return;
		}
	},

	handleError: function (error, message) {
		var tracker = this._tracker;

		dump (message + ": " + error.contents.message.readString () + "\n");
		tracker.error_free(error);
	},

	dateToPRTime: function (date) {
		return Date.parse (date) * 1000;
	},

	PRTimeToDate: function (date) {
		return new Date (date / 1000);
	}
}
