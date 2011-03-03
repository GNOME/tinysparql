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
		string unknown = _("Unknown");

		switch (query_type) {
		case Type.ALL:
			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nie:title(?u), nfo:fileName(?u), \"$unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u fts:match \"$criteria_escaped\" . ?u nfo:belongsToContainer ?c ; tracker:available true . } ORDER BY DESC(fts:rank(?u)) OFFSET $offset LIMIT $limit";
			break;
			
		case Type.ALL_ONLY_IN_TITLES:
			string criteria_escaped_down = criteria_escaped.down();

			query = @"SELECT ?u nie:url(?u) tracker:coalesce(nfo:fileName(?u), \"$unknown\") nfo:fileLastModified(?u) nfo:fileSize(?u) nie:url(?c) WHERE { ?u a nfo:FileDataObject ; nfo:belongsToContainer ?c ; tracker:available true . FILTER(fn:contains(fn:lower-case(nfo:fileName(?u)), \"$criteria_escaped_down\")) } ORDER BY DESC(nfo:fileName(?u)) OFFSET $offset LIMIT $limit";
			break;

		case Type.APPLICATIONS:
			query = @"
			        SELECT
			          ?urn
			          tracker:coalesce(nfo:softwareCmdLine(?urn), ?urn)
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"$unknown\")
			          nie:comment(?urn)
			        WHERE {
			          ?urn a nfo:Software ;
			               fts:match \"$criteria_escaped\"
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
			          tracker:coalesce(nie:title(?song), nfo:fileName(?song), \"$unknown\")
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
			                nie:url ?tooltip
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
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"$unknown\") 
			          fn:string-join((nfo:height(?urn), nfo:width(?urn)), \" x \") 
			          nfo:fileSize(?urn)
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Image ;
			          nie:url ?tooltip ;
			          fts:match \"$criteria_escaped\" 
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
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"$unknown\") 
			          \"\"
			          nfo:duration(?urn)
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Video ;
			          nie:url ?tooltip ;
			          fts:match \"$criteria_escaped\" .
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
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"$unknown\") 
			          tracker:coalesce(nco:fullname(?creator), nco:fullname(?publisher), \"\")
			          fn:concat(nfo:pageCount(?urn), \" $pages\")
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Document ;
			          nie:url ?tooltip ;
			          fts:match \"$criteria_escaped\" .
			          OPTIONAL {
			            ?urn nco:creator ?creator .
			          }
			          OPTIONAL {
			            ?urn nco:publisher ?publisher .
			          }
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nie:title(?urn)) 
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.MAIL:
			string no_subject = _("No Subject");
			string to = _("To");
			
			query = @"
			        SELECT
			          ?urn
			          nie:url(?urn)
			          tracker:coalesce(nco:fullname(?sender), nco:nickname(?sender), nco:emailAddress(?sender), \"$unknown\")
			          tracker:coalesce(nmo:messageSubject(?urn), \"$no_subject\")
			          nmo:receivedDate(?urn)
			          fn:concat(\"$to: \", tracker:coalesce(nco:fullname(?to), nco:nickname(?to), nco:emailAddress(?to), \"$unknown\"))
			        WHERE {
			          ?urn a nmo:Email ;
			          nmo:from ?sender ;
			          nmo:to ?to ;
			          fts:match \"$criteria_escaped\" .
			        }
			        ORDER BY DESC(fts:rank(?urn)) DESC(nmo:messageSubject(?urn)) DESC(nmo:receivedDate(?urn))
			        OFFSET $offset LIMIT $limit
			        ";
			break;

		case Type.FOLDERS:
			query = @"
			        SELECT
			          ?urn
			          nie:url(?urn)
			          tracker:coalesce(nie:title(?urn), nfo:fileName(?urn), \"$unknown\")
			          tracker:coalesce(nie:url(?parent), \"\")
			          nfo:fileLastModified(?urn)
			          ?tooltip
			        WHERE {
			          ?urn a nfo:Folder ;
			          nie:url ?tooltip ;
			          fts:match \"$criteria_escaped\" .
			          OPTIONAL {
			            ?urn nfo:belongsToContainer ?parent .
			          }
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
		} catch (GLib.DBusError ec) {
			warning ("Could not run Sparql query: %s", ec.message);
		}

		debug ("Done");

		return cursor;
	}
}
