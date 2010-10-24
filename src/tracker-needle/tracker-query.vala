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
		CALENDAR
	}

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
		}
	}

	public async Sparql.Cursor? perform_async (Type query_type, Cancellable? cancellable = null) throws IOError
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

		switch (query_type) {
		case Type.ALL:
			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nie:title(?u), nfo:fileName(?u), \"Unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u fts:match \"$criteria_escaped\" . ?u nfo:belongsToContainer ?c ; tracker:available true . } ORDER BY DESC(fts:rank(?u)) OFFSET $offset LIMIT $limit";
			break;
			
		case Type.ALL_ONLY_IN_TITLES:
			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nfo:fileName(?u), \"Unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u a nfo:FileDataObject ; nfo:belongsToContainer ?c ; tracker:available true . FILTER(fn:contains(nfo:fileName(?u), \"$criteria_escaped\")) } ORDER BY DESC(nfo:fileName(?u)) OFFSET $offset LIMIT $limit";
			break;

		case Type.APPLICATIONS:
			query = @"
			        SELECT
			          ?urn 
			          nie:url(?urn) 
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"Unknown\") 
			          nie:comment(?urn)
			        WHERE {
			          ?urn a nfo:Software .
			          ?urn fts:match \"$criteria_escaped\" .
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nie:title(?urn)) 
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.MUSIC:
			query = @"
			        SELECT
			          ?song
			          nie:url(?song)
			          tracker:coalesce(nie:title(?song), nfo:fileName(?song), \"Unknown\")
			          fn:string-join((?performer, ?album), \" - \")
			          nfo:duration(?song)
			          ?tooltip
			        WHERE {
			          ?match fts:match \"$criteria_escaped\"
			          {
			            ?song nmm:musicAlbum ?match
			          } UNION {
			            ?song nmm:performer ?match
			          } UNION {
			            ?song a nfo:Audio .
			            ?match a nfo:Audio
			            FILTER (?song = ?match)
			          }
			          ?song nmm:performer [ nmm:artistName ?performer ] ;
			                nmm:musicAlbum [ nie:title ?album ] ;
			                nfo:belongsToContainer [ nie:url ?tooltip ]
			        }
			        ORDER BY DESC(fts:rank(?song)) DESC(nie:title(?song))
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.IMAGES:
			query = @"
			        SELECT
			          ?urn 
			          nie:url(?urn) 
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"Unknown\") 
			          fn:string-join((nfo:height(?urn), nfo:width(?urn)), \" x \") 
			          nfo:fileSize(?urn)
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Image ;
			          nfo:belongsToContainer [ nie:url ?tooltip ] .
			          ?urn fts:match \"$criteria_escaped\" 
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nie:title(?urn)) 
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.VIDEOS:
			query = @"
			        SELECT
			          ?urn 
			          nie:url(?urn) 
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"Unknown\") 
			          \"\"
			          nfo:duration(?urn)
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Video ;
			          nfo:belongsToContainer [ nie:url ?tooltip ] .
			          ?urn fts:match \"$criteria_escaped\" .
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nie:title(?urn)) 
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.DOCUMENTS:
//			          fn:concat(nco:pageCount(?urn), \" pages\")
			string pages = _("Pages");
			
			query = @"
			        SELECT
			          ?urn 
			          nie:url(?urn) 
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"Unknown\") 
			          ?creator
			          fn:concat(nfo:pageCount(?urn), \" $pages\")
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Document ;
			          nco:creator [ nco:fullname ?creator ] ;
			          nfo:belongsToContainer [ nie:url ?tooltip ] .
			          ?urn fts:match \"$criteria_escaped\" .
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nie:title(?urn)) 
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		default:
			assert_not_reached ();
		}

		debug ("Running query: '%s'", query);

		try {
			cursor = yield connection.query_async (query, null);
		} catch (Sparql.Error ea) {
			warning ("Could not run Sparql query: %s", ea.message);
		} catch (GLib.IOError eb) {
			warning ("Could not run Sparql query: %s", eb.message);
		}

		debug ("Done");

		return cursor;
	}
}
