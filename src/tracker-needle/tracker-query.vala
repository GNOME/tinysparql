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
		ALL_ONLY_IN_TITLES,
		CONTACTS,
		APPLICATIONS,
		MUSIC,
		IMAGES,
		VIDEOS,
		DOCUMENTS,
		MAIL,
		CALENDAR,
		FOLDERS
	}

        private string [] where_clauses = {
        	// ALL
                "WHERE {
                   ?urn fts:match \"%s\" ;
                        nfo:belongsToContainer ?parent ;
                        tracker:available true .
                }",

		// ALL_ONLY_IN_TITLES
                "WHERE {
                    ?urn a nfo:FileDataObject ;
                         nfo:belongsToContainer ?parent ;
                         tracker:available true .
                         FILTER (fn:contains (fn:lower-case (nfo:fileName(?urn)), \"%s\"))
                }",

		// CONTACTS
                "",

		// APPLICATIONS
                "WHERE {
		   ?urn a nfo:Software ;
		        fts:match \"%s\"
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
		   ?match fts:match \"%s\" .
		   ?urn nmm:performer [ nmm:artistName ?performer ] ;
		         nmm:musicAlbum [ nie:title ?album ] ;
		         nie:url ?tooltip
		}",

		// IMAGES
                "WHERE {
		   ?urn a nfo:Image ;
		        nie:url ?tooltip ;
		        fts:match \"%s\"
		}",

		// VIDEOS
                "WHERE {
		   ?urn a nfo:Video ;
		        nie:url ?tooltip ;
		        fts:match \"%s\" .
		}",

		// DOCUMENTS
                "WHERE {
		   ?urn a nfo:Document ;
		        nie:url ?tooltip ;
		        fts:match \"%s\" .
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
		        nmo:to ?to ;
		        fts:match \"%s\" .
		}",

		// CALENDAR
                "",

		// FOLDERS
                "WHERE {
		   ?urn a nfo:Folder ;
		        nie:url ?tooltip ;
		        fts:match \"%s\" .
		   OPTIONAL {
		     ?urn nfo:belongsToContainer ?parent .
		   }
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
		} catch (Sparql.Error ea) {
			warning ("Could not get Sparql connection: %s", ea.message);
		} catch (GLib.IOError eb) {
			warning ("Could not get Sparql connection: %s", eb.message);
		} catch (GLib.DBusError ec) {
			warning ("Could not get Sparql connection: %s", ec.message);
		}
	}

        public async uint get_count_async (Type query_type,
                                           Cancellable? cancellable = null) throws IOError
        requires (connection != null) {
		Sparql.Cursor cursor = null;

		if (criteria == null || criteria.length < 1) {
			warning ("Criteria was NULL or an empty string, no query performed");
			return 0;
		}

                string criteria_escaped = Tracker.Sparql.escape_string (criteria);

                query = "SELECT count(?urn) " + where_clauses[query_type].printf (criteria_escaped);

		try {
			cursor = yield connection.query_async (query, null);
                        yield cursor.next_async ();
		} catch (Sparql.Error ea) {
			warning ("Could not run Sparql count query: %s", ea.message);
		} catch (GLib.IOError eb) {
			warning ("Could not run Sparql count query: %s", eb.message);
		} catch (GLib.DBusError ec) {
			warning ("Could not run Sparql count query: %s", ec.message);
		} catch (GLib.Error ge) {
			warning ("Could not run Sparql count query: %s", ge.message);
                }

                return (uint) cursor.get_integer (0);
        }

        public async Sparql.Cursor? perform_async (Type         query_type,
                                                   string []    ?args,
                                                   Cancellable? cancellable = null) throws IOError
	requires (connection != null) {
		Sparql.Cursor cursor = null;

		if (criteria == null || criteria.length < 1) {
			warning ("Criteria was NULL or an empty string, no query performed");
			return null;
		}

		if (limit < 1) {
			warning ("Limit was < 1, no query performed");
			return null;
		}

		string criteria_escaped = Tracker.Sparql.escape_string (criteria);

		query = "SELECT " + string.joinv (" ", args) + " " + where_clauses[query_type].printf (criteria_escaped);
		query += @" OFFSET $offset LIMIT $limit";

		debug ("Running query: '%s'", query);

		try {
			cursor = yield connection.query_async (query, null);
		} catch (Sparql.Error ea) {
			warning ("Could not run Sparql query: %s", ea.message);
		} catch (GLib.IOError eb) {
			warning ("Could not run Sparql query: %s", eb.message);
		} catch (GLib.DBusError ec) {
			warning ("Could not run Sparql query: %s", ec.message);
		}

		debug ("Done");

		return cursor;
	}
}
