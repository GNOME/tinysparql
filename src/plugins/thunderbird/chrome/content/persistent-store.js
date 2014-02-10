if (!org.bustany.TrackerBird.PersistentStore || !org.bustany.TrackerBird.PersistentStore.__initialized)
org.bustany.TrackerBird.PersistentStore = {
	// Init barrier
	__initialized: true,
	__console: Components.classes["@mozilla.org/consoleservice;1"].getService(Components.interfaces.nsIConsoleService),
	_log: function(msg) {
	   this.__console.logStringMessage(msg);
	},

	_schemaVersion: 1,

	_db: null,
	_rememberMessageStatement: null,
	_forgetMessageStatement: null,
	_fetchUrisStatement: null,
	_insertMetaStatement: null,
	_updateMetaStatement: null,
	_selectMetaStatement: null,

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
		this._db.executeSimpleSQL("CREATE TABLE IF NOT EXISTS meta (key VARCHAR(255) NOT NULL UNIQUE, value VARCHAR(255));");
		this._rememberMessageStatement = this._db.createStatement("INSERT INTO knownMessages VALUES (:folderUri, :msgUri)");
		this._forgetMessageStatement = this._db.createStatement("DELETE FROM knownMessages WHERE msgUri = :msgUri");
		this._fetchUrisStatement = this._db.createStatement("SELECT msgUri FROM knownMessages WHERE folderUri = :folderUri");
		this._insertMetaStatement = this._db.createStatement("INSERT OR IGNORE INTO meta VALUES (:key, :value)");
		this._updateMetaStatement = this._db.createStatement("INSERT OR REPLACE INTO meta VALUES (:key, :value)");
		this._selectMetaStatement = this._db.createStatement("SELECT value FROM meta WHERE key = :key");

		this.insertDefaultSettings();

		var currentSchemaVersion = this.getSetting("version");
		if (!currentSchemaVersion || (currentSchemaVersion < this._schemaVersion)) {
			dump("Schema changed, reseting index\n");
			this._db.executeSimpleSQL("DELETE FROM knownMessages;");
			this.insertSetting("version", this._schemaVersion, true);
		}

		dump ("Trackerbird persistent store initialized\n")
		this._log("trackerbird: persistent store initialized")
		return true;
	},

	shutdown: function() {
		dump ("Trackerbird persistent store shutting down...\n")
		this.endTransaction();
		this._rememberMessageStatement.finalize();
		this._forgetMessageStatement.finalize();
		this._fetchUrisStatement.finalize();
		this._insertMetaStatement.finalize();
		this._updateMetaStatement.finalize();
		this._selectMetaStatement.finalize();
		this._db.close();
		dump ("Trackerbird persistent store shut down\n")
	},

	rememberMessage: function(msg) {
		var folder = msg.folder;
		var stmt = this._rememberMessageStatement;
		var uri = folder.getUriForMsg(msg);

		stmt.params.folderUri = folder.folderURL;
		stmt.params.msgUri = uri;

		this.runUpdate(stmt);
	},

	forgetMessage: function(msg) {
		var folder = msg.folder;
		var stmt = this._forgetMessageStatement;
		var uri = folder.getUriForMsg(msg);

		stmt.params.msgUri = uri;

		this.runUpdate(stmt);
	},

	runUpdate: function(stmt) {
		this.startTransaction();

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
	},

	insertDefaultSettings: function() {
		this.startTransaction();

		// We perform "SILENT" inserts, ie don't replace any existing value
		var stmt = this._insertMetaStatement;

		// Schema version
		this.insertSetting("version", this._schemaVersion, false);

		this.endTransaction();
	},

	insertSetting: function(key, value, replace) {
		var stmt = (replace ? this._updateMetaStatement : this._insertMetaStatement);

		stmt.params.key = key;
		stmt.params.value = value;

		try {
			stmt.execute();
		} catch (e) {
			dump("Couldn't save setting " + key + ": " + e + "\n");
		}
	},

	getSetting: function(key) {
		var stmt = this._selectMetaStatement;
		stmt.params.key = key;

		try {
			if (!stmt.step()) {
				return null;
			}

			var value = stmt.row.value;
			stmt.reset();
			return value;
		} catch (e) {
			dump("Could not get setting " + key + ": " + e + "\n");
			return null;
		}
	}
}
