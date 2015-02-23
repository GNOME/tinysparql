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
		TITLES_INDIRECT,
		TAGS_ONLY,
		TAGS_ONLY_INDIRECT
	}

	private string [] match_clauses = {
		// NONE (i.e. just show all)
		"",

		// FTS
		"{
		   ?urn fts:match \"%s\"
		 } UNION {
		   ?urn nao:hasTag ?tag .
                   FILTER (fn:contains (fn:lower-case (nao:prefLabel(?tag)), \"%s\"))
		 }",

		// FTS_INDIRECT (with sub-matching)
		"{
		   ?match fts:match \"%s\"
		 } UNION {
		   ?match nao:hasTag ?tag .
                   FILTER (fn:contains (fn:lower-case (nao:prefLabel(?tag)), \"%s\"))
		 }",

		// TITLES
		"FILTER (fn:contains (fn:lower-case (nfo:fileName(?urn)), \"%s\"))",

		// TITLES_INDIRECT (with sub-matching)
		"FILTER (fn:contains (fn:lower-case (nie:title(?match)), \"%s\"))",

		// TAGS_ONLY (no fts:match, just nao:prefLabel matching, %s is filled in by get_tags_filter()
		"?urn nao:hasTag ?tag .
		 FILTER (nao:prefLabel(?tag) IN (%s))",

		// TAGS_ONLY_INDIRECT (same as TAGS_ONLY for ?match)
		"?match nao:hasTag ?tag .
		 FILTER (nao:prefLabel(?tag) IN (%s))"
	};

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
		       nie:url ?tooltip .
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
		  {
		    ?urn nco:creator ?match
		  } UNION {
		    ?urn nco:publisher ?match
		  } UNION {
		    ?urn a nfo:Document .
		    FILTER (! EXISTS { ?urn a nmo:Email } )
		    ?match a nfo:Document
		    FILTER (?urn = ?match)
		  }
		  %s .
		  ?urn nie:url ?tooltip .
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
                         nfo:bookmarks ?bookmark .
		  %s
		}"
	};

	private string [] sort_clauses = {
		// ALL
		"DESC(nfo:fileLastModified(?urn)) DESC(nie:contentCreated(?urn)) ASC(nie:title(?urn))",

		// CONTACTS
		"ASC(nco:fullname(?urn))",

		// APPLICATIONS
		"ASC(nie:title(?urn)) ASC(nie:comment(?urn))",

		// MUSIC
		"DESC(nfo:fileLastModified(?urn)) ASC(nie:title(?urn))",

		// IMAGES
		"DESC(nfo:fileLastModified(?urn)) ASC(nie:title(?urn))",

		// VIDEOS
		"DESC(nfo:fileLastModified(?urn)) ASC(nie:title(?urn))",

		// DOCUMENTS
		"DESC(nfo:fileLastModified(?urn)) ASC(nie:title(?urn))",

		// MAIL
		"DESC(nmo:receivedDate(?urn)) ASC(nmo:messageSubject(?urn))",

		// CALENDAR
		"DESC(nie:contentCreated(?urn))",

		// FOLDERS
		"DESC(nfo:fileLastModified(?urn)) ASC(nie:title(?urn))",

		// BOOKMARKS
		"DESC(nie:contentLastModified(?urn)) ASC(nie:title(?urn))"
	};

	public string criteria { get; set; }
	public uint offset { get; set; }
	public uint limit { get; set; }
	public string query { get; private set; }

	public GenericArray<string> tags { get; set; }

	private static Sparql.Connection connection;

	public Query () {
		try {
			connection = Sparql.Connection.get ();
		} catch (GLib.Error e) {
			warning ("Could not get Sparql connection: %s", e.message);
		}

		tags = null;
	}

	private string get_tags_filter () {
		string filter = "";

		if (tags != null && tags.length > 0) {
			for (int i = 0; i < tags.length; i++) {
				string escaped = Tracker.Sparql.escape_string (tags[i]);

				if (filter.length > 1)
					filter += ", ";

				filter += "\"%s\"".printf (escaped);
			}
		}

		return filter;
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

		if ((query_type != Type.MUSIC && query_type != Type.DOCUMENTS) &&
			!(match_type == Match.NONE ||
			  match_type == Match.FTS ||
			  match_type == Match.TITLES ||
			  match_type == Match.TAGS_ONLY)) {
			critical ("You can not use a non-MUSIC or non-DOCUMENTS query (%d) with INDIRECT matching (%d)", query_type, match_type);
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

		// If we have tags supplied, we ONLY show files from those tags
		if (tags != null && tags.length > 0) {
			if (match_type == Match.FTS_INDIRECT ||
			    match_type == Match.TITLES_INDIRECT) {
				match_type = Match.TAGS_ONLY_INDIRECT;
			} else {
				match_type = Match.TAGS_ONLY;
			}
		}
		debug ("match_type:%d", match_type);
		if (match_type != Match.NONE &&
		    match_type != Match.TAGS_ONLY &&
		    match_type != Match.TAGS_ONLY_INDIRECT &&
		    (criteria == null || criteria.length < 1)) {
			warning ("Criteria was NULL or an empty string no query performed");
			return 0;
		}

		string match;

		if (match_type == Match.TAGS_ONLY ||
		    match_type == Match.TAGS_ONLY_INDIRECT) {
			match = match_clauses[match_type].printf (get_tags_filter ());
		} else {
			string criteria_escaped = Tracker.Sparql.escape_string (criteria);

			// FTS queries take 2 arguments for tags and fts:match
			if (match_type == Match.FTS ||
			    match_type == Match.FTS_INDIRECT) {
				match = match_clauses[match_type].printf (criteria_escaped, criteria_escaped);
			} else {
				match = match_clauses[match_type].printf (criteria_escaped);
			}
		}

		query  = "SELECT count(?urn)";

		if (where_clauses[query_type].length > 0) {
			query += " " + where_clauses[query_type].printf (match);
		}

		if (sort_clauses[query_type].length > 0) {
			query += " ORDER BY " + sort_clauses[query_type];
		}

		try {
			cursor = yield connection.query_async (query, null);
			yield cursor.next_async ();
		} catch (GLib.Error e) {
			warning ("Could not run Sparql count query: %s", e.message);
		}

		return (uint) cursor.get_integer (0);
	}

	public async Sparql.Cursor? perform_async (Type query_type, Match match_type, string[] ?args, Cancellable? cancellable = null) throws IOError
	requires (connection != null) {
		Sparql.Cursor cursor = null;

		if (!check_query_and_match_type (query_type, match_type)) {
			return null;
		}

		// If we have tags supplied, we ONLY show files from those tags
		if (tags != null && tags.length > 0) {
			if (match_type == Match.FTS_INDIRECT ||
			    match_type == Match.TITLES_INDIRECT) {
				match_type = Match.TAGS_ONLY_INDIRECT;
			} else {
				match_type = Match.TAGS_ONLY;
			}
		}

		if (match_type != Match.NONE &&
		    match_type != Match.TAGS_ONLY &&
		    match_type != Match.TAGS_ONLY_INDIRECT &&
		    (criteria == null || criteria.length < 1)) {
			warning ("Criteria was NULL or an empty string no query performed");
			return null;
		}

		if (limit < 1) {
			warning ("Limit was < 1, no query performed");
			return null;
		}

		string match;

		if (match_type == Match.TAGS_ONLY ||
		    match_type == Match.TAGS_ONLY_INDIRECT) {
			match = match_clauses[match_type].printf (get_tags_filter ());
		} else {
			string criteria_escaped = Tracker.Sparql.escape_string (criteria);

			// FTS queries take 2 arguments for tags and fts:match
			if (match_type == Match.FTS ||
			    match_type == Match.FTS_INDIRECT) {
				match = match_clauses[match_type].printf (criteria_escaped, criteria_escaped);
			} else {
				match = match_clauses[match_type].printf (criteria_escaped);
			}
		}

		query  = "SELECT " + string.joinv (" ", args);
		if (where_clauses[query_type].length > 0) {
			query += " " + where_clauses[query_type].printf (match);
		}

		if (sort_clauses[query_type].length > 0) {
			query += " ORDER BY " + sort_clauses[query_type];
		}

		query += " OFFSET %u LIMIT %u".printf (offset, limit);

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
