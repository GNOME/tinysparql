/*
 * Copyright (C) 2010, Adrien Bustany <abustany@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

namespace Tracker {

private errordomain RestCallError {
	INVALID_RESPONSE, /* Malformed XML */
	CALL_ERROR        /* Call failed */
}

public class MinerFlickr : Tracker.MinerWeb {
	private static const string MINER_NAME = "Flickr";
	private static const string MINER_DESCRIPTION = "Tracker miner for Flickr";
	/* The API_KEY and SECRET constants identify the application on Flickr */
	private static const string API_KEY = "7983269709fa3158c752e3e4d6b3b9e5";
	private static const string SHARED_SECRET = "c0316d1cb4b15e2d";
	private static const string DATASOURCE_URN = "urn:nepomuk:datasource:2208f9fc-3c5b-4e40-ade4-45a0d7b0cf6f";
	private static const string FLICKR_AUTH_URL = "http://api.flickr.com/services/auth/";
	private static const string FLICKR_REST_URL = "http://api.flickr.com/services/rest/";
	private static const string FLICKR_PHOTOSET_URL = "http://www.flickr.com/photos/%s/sets/%s";
	private static const string FLICKR_PHOTO_URL = "http://farm%s.static.flickr.com/%s/%s_%s.jpg";
	private static const string NMM_PHOTO = "http://www.tracker-project.org/temp/nmm#Photo";

	/* Values taken from the EXIF spec */
	private enum ExifTag {
		CAMERA = 271,
		FLASH = 37385,
		FNUMBER = 33437,
		FOCAL_LENGTH = 37386,
		ISO_SPEED = 2,
		METERING_MODE = 37383,
		WHITE_BALANCE = 5
	}

	private enum ExifMeteringMode {
		AVERAGE = 1,
		CENTER_WEIGHTED_AVERAGE,
		SPOT,
		MULTISPOT,
		PATTERN,
		PARTIAL
	}

	private enum ExifWhiteBalance {
		AUTO = 0,
		MANUAL
	}

	private static const uint PULL_INTERVAL = 5 * 60; /* seconds */
	private uint pull_timeout_handle;

	private static MainLoop main_loop;

	/* Needed to connect to the writeback signal */
	private Tracker.Client tracker_client;

	private Rest.Proxy rest;

	/* Only used during association phase */
	private string frob;
	/* Used to sign calls */
	private string auth_token;
	/* Used to form some urls */
	private string user_id;

	construct {
		name = MINER_NAME;
		associated = false;
		status = "Not authenticated";
		progress = 0.0;

		rest = new Rest.Proxy (FLICKR_REST_URL, false);

		tracker_client = new Tracker.Client (0, -1);
		tracker_client.writeback_connect (writeback);

		init_datasource ();

		this.notify["associated"].connect (association_status_changed);
	}

	public void shutdown () {
		status = "Shutting down";
		associated = false;
	}

	private void init_datasource () {
		try {
			tracker_client.sparql_update ("insert { <%s> a nie:DataSource ; nao:identifier \"flickr\" }".printf (DATASOURCE_URN));
		} catch (Error e) {
			warning ("Couldn't init datasource: %s", e.message);
		}
	}

	private void association_status_changed (Object source, ParamSpec pspec) {
		if (associated) {
			if (pull_timeout_handle != 0)
				return;

			message ("Miner is now associated. Initiating periodic pull.");
			pull_timeout_handle = Timeout.add_seconds (PULL_INTERVAL, pull_timeout_cb);
			Idle.add ( () => { pull_timeout_cb (); return false; });
		} else {
			if (pull_timeout_handle == 0)
				return;

			Source.remove (pull_timeout_handle);
		}
	}

	private bool pull_timeout_cb () {
		init_pull ();
		return true;
	}

	private async void init_pull () {
		Rest.ProxyCall albums_call;
		Rest.XmlNode photosets_node;

		status = "Refreshing photo albums";
		progress = 0.0;

		Idle.add (init_pull.callback);
		yield;


		/* First get the list of albums */
		albums_call = rest.new_call ();
		albums_call.add_param ("method", "flickr.photosets.getList");

		try {
			photosets_node = run_call (albums_call);
			insert_photosets (photosets_node);
		} catch (Error call_error) {
			string error_message= "Could not get photosets list: %s".printf (call_error.message);
			status = error_message;
			warning (error_message);
		}

		status = "Idle";
		progress = 1.0;
		message ("Pull finished");
	}

	private void insert_photosets (Rest.XmlNode root_node) {
		Rest.XmlNode photoset_node;
		Rest.XmlNode title_node;
		Rest.XmlNode photos_node;
		string photoset_url;
		string photoset_identifier;
		bool resource_created;
		string photoset_urn;
		string delete_query;
		Rest.ProxyCall photos_call;
		SparqlBuilder builder;
		uint n_photosets;
		uint indexed_photosets = 0;

		photoset_node = root_node.find ("photoset");
		n_photosets = root_node.children.size ();

		while (photoset_node != null) {
			try {
				photoset_url = FLICKR_PHOTOSET_URL.printf (user_id, photoset_node.get_attr ("id"));
				photoset_identifier = "flickr:photoset:%s".printf (photoset_node.get_attr ("id"));
				photoset_urn = get_resource (photoset_url,
				                             {"nfo:MediaList", "nfo:RemoteDataObject"},
				                             photoset_identifier,
				                             out resource_created);

				message ("Getting photos for album %s", photoset_url);

				builder = new SparqlBuilder.update ();
				builder.insert_open (photoset_url);
				builder.subject_iri (photoset_urn);

				if (!resource_created) {
					delete_query = ("delete { <%1$s> nie:title ?title }"
					             +  "where  { <%1$s> nie:title ?title }")
					             .printf (photoset_urn);
					tracker_client.sparql_update (delete_query);
				} else {
					builder.predicate ("nie:dataSource");
					builder.object_iri (DATASOURCE_URN);
					builder.predicate ("nie:url");
					builder.object_string (photoset_url);
				}

				title_node = photoset_node.find ("title");
				if (title_node != null) {
					builder.predicate ("nie:title");
					builder.object_string (title_node.content);
				}

				builder.insert_close ();

				tracker_client.sparql_update (builder.get_result ());

				status = "Refresing album \"%s\"".printf (title_node.content);

				photos_call = rest.new_call ();
				photos_call.add_params ("method", "flickr.photosets.getPhotos",
				                        "photoset_id", photoset_node.get_attr ("id"),
				                        "media", "photos",
				                        "extras", "original_format");
				photos_node = run_call (photos_call);
				insert_photos (photos_node);
			} catch (Error err) {
				string error_message = "Could not list photos for photoset %s: %s".printf (photoset_url, err.message);
				status = error_message;
				warning (error_message);
			}
			photoset_node = photoset_node.next;

			indexed_photosets ++;
			progress = ((double) indexed_photosets) / n_photosets;
		}
	}

	private void insert_photos (Rest.XmlNode root_node) {
		Rest.XmlNode photoset_node;
		string photoset_url;
		Rest.XmlNode photo_node;
		string photo_url;
		string photo_urn;
		bool resource_created;
		SparqlBuilder builder;

		photoset_node = root_node.find ("photoset");
		if (photoset_node == null || photoset_node.get_attr ("id") == null) {
			warning ("Malformed response for flickr.photosets.getPhotos");
			return;
		}

		photoset_url = FLICKR_PHOTOSET_URL.printf (user_id, photoset_node.get_attr ("id"));
		message ("Indexing photoset %s", photoset_url);

		photo_node = root_node.find ("photo");

		while (photo_node != null) {
			try {
				photo_url = FLICKR_PHOTO_URL.printf (photo_node.get_attr ("farm"),
				                                     photo_node.get_attr ("server"),
				                                     photo_node.get_attr ("id"),
				                                     photo_node.get_attr ("secret"));
				photo_urn = get_resource (photo_url,
				                          {"nmm:Photo", "nfo:RemoteDataObject", "nfo:MediaFileListEntry"},
				                          "flickr:photo:%s".printf (photo_node.get_attr ("id")),
				                          out resource_created);

				builder = new SparqlBuilder.update ();

				if (resource_created) {
					builder.insert_open (photo_url);
					builder.subject_iri (photo_urn);
					builder.predicate ("nie:dataSource");
					builder.object_iri (DATASOURCE_URN);
					builder.predicate ("nie:url");
					builder.object_string (photo_url);
					builder.insert_close ();
				}

				insert_photo_info (photo_node, builder, photo_url, photo_urn);
				insert_exif_data (photo_node, builder, photo_url, photo_urn);


				tracker_client.sparql_update (builder.get_result ());
			} catch (Error err) {
				warning ("Couldn't insert photo %s: %s", photo_url, err.message);
			}

			photo_node = photo_node.next;
		}
	}

	private void insert_photo_info (Rest.XmlNode photo_node, SparqlBuilder builder, string graph, string urn) {
		var info_call = rest.new_call ();
		Rest.XmlNode root_node;
		Rest.XmlNode title_node;
		Rest.XmlNode description_node;
		Rest.XmlNode tag_node;

		info_call.add_params ("method", "flickr.photos.getInfo",
		                      "photo_id", photo_node.get_attr ("id"));

		try {
			root_node = run_call (info_call);
		} catch (Error call_error) {
			string error_message = "Couldn't get info for photo %s: %s".printf (photo_node.get_attr ("id"), call_error.message);
			status = error_message;
			warning (error_message);
			return;
		}

		title_node = root_node.find ("title");
		if (title_node != null && title_node.content != null) {
			update_triple_string (builder, graph, urn, "nie:title", title_node.content);
		}

		description_node = root_node.find ("description");
		if (description_node != null && description_node.content != null) {
			update_triple_string (builder, graph, urn, "nie:comment", description_node.content);
		}

		tag_node = root_node.find ("tags").find ("tag");

		builder.insert_open (graph);
		builder.subject_iri (urn);

		while (tag_node != null) {
			builder.predicate ("nao:hasTag");

			builder.object_blank_open ();
			builder.predicate ("a");
			builder.object ("nao:Tag");
			builder.predicate ("nao:prefLabel");
			builder.object_string (tag_node.get_attr ("raw"));
			builder.object_blank_close ();

			tag_node = tag_node.next;
		}

		builder.insert_close ();
	}

	private void insert_exif_data (Rest.XmlNode photo_node, SparqlBuilder builder, string graph, string urn) {
		var exif_call = rest.new_call ();
		Rest.XmlNode root_node;
		Rest.XmlNode exif_node;
		string exif_value;

		exif_call.add_params ("method", "flickr.photos.getExif",
		                      "photo_id", photo_node.get_attr ("id"));

		try {
			root_node = run_call (exif_call);
		} catch (Error call_error) {
			string error_message = "Couldn't get EXIF data for photo %s: %s".printf (photo_node.get_attr ("id"), call_error.message);
			status = error_message;
			warning (error_message);
			return;
		}

		exif_node = root_node.find ("exif");

		while (exif_node != null) {
			exif_value = exif_node.find ("raw").content;

			switch (exif_node.get_attr ("tag").to_int ()) {
				case ExifTag.CAMERA:
					update_triple_string (builder, graph, urn, "nfo:device", exif_value);
					break;
				case ExifTag.FLASH:
					update_triple_object (builder, graph, urn, "nmm:flash", exif_value.to_int ()%2 == 1 ? "nmm:flash-on" : "nmm:flash-off");
					break;
				case ExifTag.FNUMBER:
					update_triple_double (builder, graph, urn, "nmm:fnumber", ratio_to_double (exif_value));
					break;
				case ExifTag.FOCAL_LENGTH:
					update_triple_double (builder, graph, urn, "nmm:focalLength", ratio_to_double (exif_value));
					break;
				case ExifTag.ISO_SPEED:
					update_triple_int64 (builder, graph, urn, "nmm:isoSpeed", (int64)exif_value.to_int ());
					break;
				case ExifTag.METERING_MODE:
					switch (exif_value.to_int ()) {
						case ExifMeteringMode.AVERAGE:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-average");
							break;
						case ExifMeteringMode.CENTER_WEIGHTED_AVERAGE:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-center-weighted-average");
							break;
						case ExifMeteringMode.SPOT:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-spot");
							break;
						case ExifMeteringMode.MULTISPOT:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-multispot");
							break;
						case ExifMeteringMode.PATTERN:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-pattern");
							break;
						case ExifMeteringMode.PARTIAL:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-partial");
							break;
						default:
							update_triple_object (builder, graph, urn, "nmm:meteringMode", "nmm:meteringMode-other");
							break;
					}
					break;
				case ExifTag.WHITE_BALANCE:
					switch (exif_value.to_int ()) {
						case ExifWhiteBalance.AUTO:
							update_triple_object (builder, graph, urn, "nmm:whiteBalance", "nmm:whiteBalance-auto");
							break;
						case ExifWhiteBalance.MANUAL:
							update_triple_object (builder, graph, urn, "nmm:whiteBalance", "nmm:whiteBalance-manual");
							break;
					}
					break;
				default:
					break;
			}
			exif_node = exif_node.next;
		}
	}

	private void add_tags (string photo_id, string[] tags) {
		Rest.ProxyCall tag_call;

		tag_call = rest.new_call ();
		tag_call.add_params ("method", "flickr.photos.addTags",
		                     "photo_id", photo_id,
		                     "tags", string.joinv (",", tags));

		try {
			run_call (tag_call);
		} catch (Error call_error) {
			string error_message = "Couldn't add tags for photo %s: %s".printf (photo_id, call_error.message);
			status = error_message;
			warning (error_message);
			return;
		}
	}

	private void remove_tag (string tag_id) {
		Rest.ProxyCall tag_call;

		tag_call = rest.new_call ();
		tag_call.add_params ("method", "flickr.photos.removeTag",
		                     "tag_id", tag_id);

		try {
			run_call (tag_call);
		} catch (Error call_error) {
			string error_message = "Couldn't remove tag: %s".printf (call_error.message);
			status = error_message;
			warning (error_message);
			return;
		}
	}

	private async void writeback_photo (string uri) {
		weak PtrArray results;
		weak string[][] triples;
		string photo_id;
		string[] local_tags = {};
		HashTable<string, string> flickr_tags = new HashTable<string, string> (str_hash, str_equal);
		string[] tags_to_add = {};
		string[] tags_to_remove = {};
		Rest.ProxyCall tag_call;
		Rest.XmlNode root_node;
		Rest.XmlNode tag_node;

		try {
			results = yield execute_sparql (("select ?photo_id ?tag where { <%s> nie:dataSource <%s> ;"
			                                                               +    "nao:identifier ?photo_id ;"
			                                                               +    "nao:hasTag ?t ."
			                                                               + "?t nao:prefLabel ?tag }").printf (uri, DATASOURCE_URN));
		} catch (Error tracker_error) {
			warning ("Tracker error when doing writeback for photo %s: %s", uri, tracker_error.message);
			return;
		}

		if (results.len == 0) {
			return;
		}

		triples = (string[][])results.pdata;
		photo_id = triples[0][0];

		for (uint i = 0 ; i < results.len ; ++i) {
			local_tags += triples[i][1];
		}

		tag_call = rest.new_call ();
		tag_call.add_params ("method", "flickr.tags.getListPhoto",
		                     "photo_id", photo_id);
		try {
			root_node = run_call (tag_call);
		} catch (Error get_tags_error) {
			string error_message = "Couldn't get tags for photo %s: %s".printf (uri, get_tags_error.message);
			status = error_message;
			warning (error_message);
			return;
		}

		tag_node = root_node.find ("tags").find ("tag");

		while (tag_node != null) {
			flickr_tags.insert (tag_node.get_attr ("raw"),
			                    tag_node.get_attr ("id"));
			tag_node = tag_node.next;
		}

		foreach (string local_tag in local_tags) {
			if (flickr_tags.lookup (local_tag) == null) {
				tags_to_add += local_tag;
			}
		}

		foreach (weak string flickr_tag in flickr_tags.get_keys ()) {
			if (array_search_str (flickr_tag, local_tags) == -1)
				tags_to_remove += flickr_tags.lookup (flickr_tag);
		}

		add_tags (photo_id, tags_to_add);

		foreach (string tag in tags_to_remove)
			remove_tag (tag);
	}

	private double ratio_to_double (string ratio) {
		string[] tokens = ratio.split ("/");
		if (tokens[1].to_int () == 0) {
			critical ("fracToDouble : divide by zero while parsing ratio '%s'", ratio);
			return 0;
		}
		return (tokens[0].to_int () * 1.0) / (tokens[1].to_int ());
	}

	private int array_search_str (string needle, string[] haystack) {
		for (int i = 0 ; i < haystack.length ; ++i) {
			if (haystack[i] == needle)
				return i;
		}

		return -1;
	}

	private void sign_call (Rest.ProxyCall call) {
		StringBuilder signature;
		HashTable<string, string> parameters;
		List<weak string> parameter_names;

		call.add_param ("api_key", API_KEY);
		if (auth_token != null)
			call.add_param ("auth_token", auth_token);

		signature = new StringBuilder (SHARED_SECRET);
		parameters = call.get_params ();

		parameter_names = parameters.get_keys ().copy ();
		parameter_names.sort ((CompareFunc)strcmp);

		foreach (string parameter in parameter_names) {
			signature.append (parameter);
			signature.append (parameters.lookup (parameter));
		}

		call.add_param ("api_sig", Checksum.compute_for_string (ChecksumType.MD5, signature.str));
	}

	private Rest.XmlNode? run_call (Rest.ProxyCall call) throws GLib.Error {
		Rest.XmlParser parser;
		Rest.XmlNode root_node;

		sign_call (call);

		try {
			call.run (null);
		} catch (Error call_error) {
			throw call_error;
		}

		parser = new Rest.XmlParser ();
		root_node = parser.parse_from_data (call.get_payload (), call.get_payload_length ());
		if (root_node == null || root_node.name != "rsp") {
			throw new RestCallError.INVALID_RESPONSE ("Empty payload or root node not \"rsp\"");
		}

		return root_node;
	}

	public override HashTable<string, string> get_association_data () throws Tracker.MinerWebError {
		var association_data = new HashTable<string, string> (str_hash, str_equal);
		var frob_call = rest.new_call ();
		Rest.XmlNode root_node;
		Rest.XmlNode frob_node;
		string api_signature;
		string url;

		frob_call.add_param ("method", "flickr.auth.getFrob");

		try {
			root_node = run_call (frob_call);
		} catch (Error call_error) {
			throw new MinerWebError.SERVICE ("Error while getting association data: %s", call_error.message);
		}

		frob_node = root_node.find ("frob");
		if (frob_node == null || frob_node.content == null) {
			throw new MinerWebError.SERVICE ("Malformed XML response while getting frob");
		}

		this.frob = frob_node.content;

		api_signature = Checksum.compute_for_string (ChecksumType.MD5,
		                                             SHARED_SECRET + "api_key" + API_KEY + "frob" + this.frob + "permswrite");
		url = FLICKR_AUTH_URL + "?api_key=" + API_KEY + "&perms=write&frob=" + this.frob + "&api_sig=" + api_signature;

		association_data.insert ("url", url);

		return association_data;
	}

	public override void associate (HashTable<string, string> association_data) throws Tracker.MinerWebError {
		var password_provider = PasswordProvider.get ();
		var token_call = rest.new_call ();
		Rest.XmlNode root_node;
		Rest.XmlNode token_node;
		Rest.XmlNode user_node;

		token_call.add_params ("method", "flickr.auth.getToken",
		                       "frob", this.frob);

		try {
			root_node = run_call (token_call);
		} catch (Error call_error) {
			throw new MinerWebError.SERVICE ("Unable to get authentication token: %s", call_error.message);
		}

		token_node = root_node.find ("token");
		user_node = root_node.find ("user");
		if (token_node == null || token_node.content == null ||
		    user_node == null || user_node.get_attr ("username") == null) {
			throw new MinerWebError.SERVICE ("Malformed XML response while getting token");
		}

		try {
			password_provider.store_password (MINER_NAME,
			                                  MINER_DESCRIPTION,
			                                  user_node.get_attr ("username"),
			                                  token_node.content);
		} catch (Error e) {
			if (e is PasswordProviderError.SERVICE) {
				throw new MinerWebError.KEYRING (e.message);
			}

			critical ("Internal error: %s", e.message);
			return;
		}
	}

	public override void authenticate () throws MinerWebError {
		PasswordProvider password_provider;
		Rest.ProxyCall login_call;
		Rest.XmlNode root_node;
		Rest.XmlNode user_node;

		password_provider = PasswordProvider.get ();

		associated = false;

		try {
			auth_token = password_provider.get_password (MINER_NAME, null);
		} catch (Error e) {
			if (e is PasswordProviderError.NOTFOUND) {
				throw new MinerWebError.NO_CREDENTIALS ("Miner is not associated");
			}
			throw new MinerWebError.KEYRING (e.message);
		}

		login_call = rest.new_call ();
		login_call.add_param ("method", "flickr.auth.checkToken");

		try {
			root_node = run_call (login_call);
		} catch (Error call_error) {
			throw new MinerWebError.SERVICE ("Cannot verify login: %s", call_error.message);
		}

		user_node = root_node.find ("user");
		if (user_node == null || user_node.get_attr ("nsid") == null) {
			throw new MinerWebError.WRONG_CREDENTIALS ("Stored authentication token is not valid");
		}

		user_id = user_node.get_attr ("nsid");

		message ("Authentication successful");
		associated = true;
	}

	public override void dissociate () throws MinerWebError {
		var password_provider = PasswordProvider.get ();

		try {
			password_provider.forget_password (MINER_NAME);
		} catch (Error e) {
			if (e is PasswordProviderError.SERVICE) {
				throw new MinerWebError.KEYRING (e.message);
			}

			critical ("Internal error: %s", e.message);
			return;
		}

		associated = false;
	}

	public void writeback (HashTable<string, void*> properties)
	{
		List<weak string> uris = (List<weak string>)properties.get_keys ();
		weak string[] rdf_classes;

		foreach (string uri in uris) {
			rdf_classes = (string[])properties.lookup (uri);

			for (uint i = 0; rdf_classes[i] != null; i++) {
				if (rdf_classes[i] == NMM_PHOTO) {
					writeback_photo (uri);
					return;
				}
			}
		}
	}

	private string get_resource (string? graph, string[] types, string identifier, out bool created) throws GLib.Error {
		string inner_query;
		string select_query;
		string insert_query;
		GLib.PtrArray query_results;
		unowned string[][] triples;
		HashTable<string, string> anonymous_nodes;


		select_query = "";
		inner_query = "";
		created = false;

		foreach (string type in types) {
			inner_query += " a %s; ".printf (type);
		}
		inner_query += "nao:identifier \"%s\"".printf (identifier);

		select_query = "select ?urn where { ?urn %s }".printf (inner_query);

		try {
			query_results = tracker_client.sparql_query (select_query);
		} catch (Error tracker_error) {
			throw tracker_error;
		}

		if (query_results.len > 0) {
			triples = (string[][]) query_results.pdata;
			return triples[0][0];
		}

		if (graph == null) {
			insert_query = "insert { _:res %s }".printf (inner_query);
		} else {
			insert_query = "insert into <%s> { _:res %s }".printf (graph, inner_query);
		}

		try {
			created = true;
			query_results = tracker_client.sparql_update_blank (insert_query);
			anonymous_nodes = ((HashTable<string, string>[])(((PtrArray[])query_results.pdata)[0].pdata))[0];
			return anonymous_nodes.lookup ("res");
		} catch (Error tracker_error) {
			throw tracker_error;
		}
	}

	public void update_triple_string (SparqlBuilder builder, string graph, string urn, string property, string new_value) {
		builder.delete_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_string (new_value);
		builder.delete_close ();

		builder.insert_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_string (new_value);
		builder.insert_close ();
	}

	public void update_triple_object (SparqlBuilder builder, string graph, string urn, string property, string new_value) {
		builder.delete_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object (new_value);
		builder.delete_close ();

		builder.insert_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object (new_value);
		builder.insert_close ();
	}

	public void update_triple_double (SparqlBuilder builder, string graph, string urn, string property, double new_value) {
		builder.delete_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_double (new_value);
		builder.delete_close ();

		builder.insert_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_double (new_value);
		builder.insert_close ();
	}

	public void update_triple_int64 (SparqlBuilder builder, string graph, string urn, string property, int64 new_value) {
		builder.delete_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_int64 (new_value);
		builder.delete_close ();

		builder.insert_open (graph);
		builder.subject_iri (urn);
		builder.predicate (property);
		builder.object_int64 (new_value);
		builder.insert_close ();
	}

#if G_OS_WIN32
#else
	private static bool in_loop = false;
	private static void signal_handler (int signo) {
		if (in_loop) {
			Posix.exit (Posix.EXIT_FAILURE);
		}

		switch (signo) {
			case Posix.SIGINT:
			case Posix.SIGTERM:
				in_loop = true;
				main_loop.quit ();
				break;
		}

		stdout.printf ("\nReceived signal:%d\n", signo);
	}
#endif

	private static void init_signals () {
#if G_OS_WIN32
#else
		Posix.sigaction_t act = Posix.sigaction_t ();
		Posix.sigset_t    empty_mask = Posix.sigset_t ();
		Posix.sigemptyset (empty_mask);
		act.sa_handler = signal_handler;
		act.sa_mask    = empty_mask;
		act.sa_flags   = 0;

		Posix.sigaction (Posix.SIGTERM, act, null);
		Posix.sigaction (Posix.SIGINT, act, null);
#endif
	}

	public static void main (string[] args) {
		Environment.set_application_name ("Flickr tracker miner");
		MinerFlickr flickr_miner = Object.new (typeof (MinerFlickr)) as MinerFlickr;

		init_signals ();

		main_loop = new MainLoop (null, false);
		main_loop.run ();

		flickr_miner.shutdown ();
	}
}

} // End namespace Tracker
