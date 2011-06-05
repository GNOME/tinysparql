if (!org.bustany.TrackerBird.PersistentStore || !org.bustany.TrackerBird.PersistentStore.__initialized)
org.bustany.TrackerBird.PersistentStore = {
	// Init barrier
	__initialized: true,

	_db: null,
	_rememberMessageStatement: null,
	_fetchUrisStatement: null,

	_transactionPending: false,
	_nInsertsPending: 0,

	init: function() {
		// Get profile directory
		var file = Components.classes["@mozilla.org/file/directory_service;1"]
		           .getService(Components.interfaces.nsIProperties)
		           .get("ProfD", Components.interfaces.nsIFile);
		file.append("trackerbird.sqlite");

		var storageService = Components.classes["@mozilla.org/storage/service;1"]
		                     .getService(Components.interfaces.mozIStorageService);

		this._db = storageService.openDatabase(file);

		if (!this._db) {
			dump("Could not open DB file " + file + "\n");
			return false;
		}

		this._db.executeSimpleSQL("CREATE TABLE IF NOT EXISTS knownMessages (folderUri VARCHAR(255), msgUri VARCHAR(255) NOT NULL UNIQUE);");

		this._rememberMessageStatement = this._db.createStatement("INSERT INTO knownMessages VALUES (:folderUri, :msgUri)");
		this._fetchUrisStatement = this._db.createStatement("SELECT msgUri FROM knownMessages WHERE folderUri = :folderUri");

		return true;
	},

	shutdown: function() {
		this.endTransaction();
		this._rememberMessageStatement.finalize();
		this._db.close();
	},

	rememberMessage: function(folder, msg) {
		var stmt = this._rememberMessageStatement;
		var uri = folder.getUriForMsg(msg);

		this.startTransaction();

		stmt.params.folderUri = folder.folderURL;
		stmt.params.msgUri = uri;
		// execute() also calls reset() on the statement
		stmt.execute();
		this._nInsertsPending ++;

		// Commit every 100 INSERTS
		if (this._nInsertsPending > 100) {
			this.endTransaction();
			this._nInsertsPending = 0;
		}
	},

	getUrisForFolder: function(folder) {
		var stmt = this._fetchUrisStatement;
		var uris = [];

		this.endTransaction();

		stmt.params.folderUri = folder.folderURL;

		while (stmt.step()) {
			uris.push(stmt.row.msgUri);
		}

		return uris;
	},

	startTransaction: function() {
		if (this._transactionPending) {
			return;
		}

		this._db.beginTransaction();
		this._transactionPending = true;
	},

	endTransaction: function() {
		if (!this._transactionPending) {
			return;
		}

		this._db.commitTransaction();
		this._transactionPending = false;
	}
}
