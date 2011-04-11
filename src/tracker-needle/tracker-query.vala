//
// Copyright 2010, Martyn Russell <martyn@lanedo.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//

using Tracker.Sparql;

public class Tracker.Query {
	public enum Type {
		ALL,
		CONTACTS,
		APPLICATIONS,
		MUSIC,
		IMAGES,
		VIDEOS,
		DOCUMENTS,
		MAIL,
		CALENDAR,
		FOLDERS,
		BOOKMARKS
	}

	public enum Match {
		NONE,
		FTS,
		FTS_INDIRECT,
		TITLES,
		TITLES_INDIRECT
	}

	private string [] match_clauses = {
		// NONE (i.e. just show all)
		"",

		// FTS
		"?urn fts:match \"%s\"",

		// FTS_INDIRECT (with sub-matching)
		"?match fts:match \"%s\"",

		// TITLES
		"FILTER (fn:contains (fn:lower-case (nfo:fileName(?urn)), \"%s\"))",

		// TITLES INDIRECT (with sub-matching)
		"FILTER (fn:contains (fn:lower-case (nie:title (?match), \"%s\"))"
	};

		// ALL_ONLY_IN_TITLES
//		"WHERE {
//		  ?urn a nfo:FileDataObject ;
//		  nfo:belongsToContainer ?parent ;
//		  tracker:available true .
//		  FILTER (fn:contains (fn:lower-case (nfo:fileName(?urn)), \"%s\"))
//		}",

	private string [] where_clauses = {
		// ALL
		"WHERE {
		  %s .
		  ?urn nfo:belongsToContainer ?parent ;
		  tracker:available true .
		}",

		// CONTACTS
		"",

		// APPLICATIONS
		"WHERE {
		  ?urn a nfo:Software .
		  %s
		}",

		// MUSIC
		"WHERE {
		  {
		    ?urn nmm:musicAlbum ?match
		  } UNION {
		    ?urn nmm:performer ?match
		  } UNION {
		    ?urn a nfo:Audio .
		    ?match a nfo:Audio
		    FILTER (?urn = ?match)
		  }
		  %s .
		  ?urn nmm:performer [ nmm:artistName ?performer ] ;
		       nmm:musicAlbum [ nie:title ?album ] ;
		       nie:url ?tooltip
		}",

		// IMAGES
		"WHERE {
		  ?urn a nfo:Image ;
		         nie:url ?tooltip .
		  %s
		}",

		// VIDEOS
		"WHERE {
		  ?urn a nfo:Video ;
		         nie:url ?tooltip .
		  %s
		}",

		// DOCUMENTS
		"WHERE {
		  ?urn a nfo:Document ;
		         nie:url ?tooltip .
		  %s
		  OPTIONAL {
		    ?urn nco:creator ?creator .
		  }
		  OPTIONAL {
		    ?urn nco:publisher ?publisher .
		  }
		}",

		// MAIL
		"WHERE {
		  ?urn a nmo:Email ;
		         nmo:from ?sender ;
		         nmo:to ?to .
		  %s
		}",

		// CALENDAR
		"",

		// FOLDERS
		"WHERE {
		  ?urn a nfo:Folder ;
		         nie:url ?tooltip .
		  %s
		  OPTIONAL {
		    ?urn nfo:belongsToContainer ?parent .
		  }
		}",

		// BOOKMARKS
		"WHERE {
		  ?urn a nfo:Bookmark ;
		       nie:url ?tooltip .
		  %s
		}"
	};

	public string criteria { get; set; }
	public uint offset { get; set; }
	public uint limit { get; set; }
	public string query { get; private set; }

	private static Sparql.Connection connection;

	public Query () {
		try {
			connection = Sparql.Connection.get ();
		} catch (GLib.Error e) {
			warning ("Could not get Sparql connection: %s", e.message);
		}
	}

	private bool check_query_and_match_type (Type query_type, Match match_type) {
		if (query_type != Type.IMAGES && match_type == Match.NONE) {
			critical ("You can not use a non-IMAGES query (%d) with NONE matching", query_type);
			return false;
		}

		if (query_type == Type.MUSIC && !(match_type == Match.FTS_INDIRECT ||
		                                  match_type == Match.TITLES_INDIRECT)) {
			critical ("You can not use a MUSIC query with match TITLES or FTS, INDIRECT required");
			return false;
		}

		if (query_type != Type.MUSIC && !(match_type == Match.NONE ||
		                                  match_type == Match.FTS ||
		                                  match_type == Match.TITLES)) {
			critical ("You can not use a non-MUSIC query (%d) with INDIRECT matching (%d)", query_type, match_type);
			return false;
		}

		return true;
	}

	public async uint get_count_async (Type query_type, Match match_type, Cancellable? cancellable = null) throws IOError
	requires (connection != null) {
		Sparql.Cursor cursor = null;

		if (!check_query_and_match_type (query_type, match_type)) {
			return 0;
		}

		if (match_type != Match.NONE && (criteria == null || criteria.length < 1)) {
			warning ("Criteria was NULL or an empty string, no query performed");
			return 0;
		}

		string criteria_escaped = Tracker.Sparql.escape_string (criteria);
		string match = match_clauses[match_type].printf (criteria_escaped);

		query = "SELECT count(?urn) " + where_clauses[query_type].printf (match);

		try {
			cursor = yield connection.query_async (query, null);
			yield cursor.next_async ();
		} catch (GLib.Error e) {
			warning ("Could not run Sparql count query: %s", e.message);
		}

		return (uint) cursor.get_integer (0);
	}

	public async Sparql.Cursor? perform_async (Type query_type, Match match_type, string [] ?args, Cancellable? cancellable = null) throws IOError
	requires (connection != null) {
		Sparql.Cursor cursor = null;

		if (!check_query_and_match_type (query_type, match_type)) {
			return null;
		}

		if (match_type != Match.NONE && (criteria == null || criteria.length < 1)) {
			warning ("Criteria was NULL or an empty string, no query performed");
			return null;
		}

		if (limit < 1) {
			warning ("Limit was < 1, no query performed");
			return null;
		}

		string criteria_escaped = Tracker.Sparql.escape_string (criteria);
		string match = match_clauses[match_type].printf (criteria_escaped);

		query = "SELECT " + string.joinv (" ", args) + " " + where_clauses[query_type].printf (match);
		query += @" OFFSET $offset LIMIT $limit";

		debug ("Running query: '%s'", query);

		try {
			cursor = yield connection.query_async (query, null);
		} catch (GLib.Error e) {
			warning ("Could not run Sparql query: %s", e.message);
		}

		debug ("Done");

		return cursor;
	}
}
