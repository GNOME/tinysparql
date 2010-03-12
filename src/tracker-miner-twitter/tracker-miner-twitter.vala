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

public class MinerTwitter : Tracker.MinerWeb {
	public Twitter.Provider provider { get; construct; }
	private string friendly_name; /* Human readable name of the provider */
	private string channel_urn = null;

	private Twitter.Client service;

	private static const string DATASOURCE_URN = "urn:103e7d6e-2334-4cd2-b0a5-f1b0c8bb10ef";
	private static const string SERVICE_DESCRIPTION = "Tracker miner for %s";

	private static const uint PULL_INTERVAL = 60; /* seconds */
	private uint pull_timeout_handle;

	private QueryQueue query_queue;

	/* used to store state information like last pulled tweet*/
	private static const string STATE_FILE_NAME = "tracker-miner-%s.state";
	private static const string STATE_FILE_GROUP = "General";
 	private KeyFile state_file;

	private uint last_status_timestamp;

	private static MainLoop main_loop;


	construct {
		if (provider == Twitter.Provider.DEFAULT_PROVIDER) {
			set ("name", "Twitter");
			friendly_name = "Twitter";
		} else if (provider == Twitter.Provider.IDENTI_CA) {
			set ("name", "Identica");
			friendly_name = "Identi.ca";
		} else {
			assert_not_reached ();
		}

		set ("association-status", MinerWebAssociationStatus.UNASSOCIATED);

		service = new Twitter.Client.full (provider, null, null, null);
		service.status_received.connect (status_received_cb);
		service.timeline_complete.connect (timeline_complete_cb);

		state_file = new KeyFile ();
		load_state_file ();

		pull_timeout_handle = 0;
		this.notify["association-status"].connect (association_status_changed);

		query_queue = new QueryQueue (this);

		init_feed_channel ();
	}

	public void shutdown () {
		set ("association-status", MinerWebAssociationStatus.UNASSOCIATED);
		save_state_file ();
	}

	private async void init_feed_channel () {
		string sparql;
		unowned PtrArray results;

		sparql = "select ?c where { ?c a mfo:FeedChannel ; mfo:type ?type ."
		                        + " ?type mfo:name ?typeName . "
		                        + " FILTER (?typeName = \"%s\") }".printf (friendly_name);

		try {
			results = yield execute_sparql (sparql);

			if (results.len == 0) {
				/* No optimal, but we're waiting for blank support in TrackerMiner */
				sparql = "insert { _:channel a mfo:FeedChannel ; rdfs:label \"%s home timeline\";".printf (friendly_name)
				               + " mfo:type [ a mfo:FeedType ; mfo:name \"%s\" ] }".printf (friendly_name);
				yield execute_update (sparql);
				sparql = "select ?c where { ?c a mfo:FeedChannel ; mfo:type ?type ."
				                        + " ?type mfo:name ?typeName . "
				                        + " FILTER (?typeName = \"%s\") }".printf (friendly_name);
				results = yield execute_sparql (sparql);
			}

			assert (results.len == 1);
			channel_urn = ((string[][])results.pdata)[0][0];
		} catch (Error tracker_error) {
			critical ("Couldn't initialize feed channel: %s", tracker_error.message);
		}
	}

	private void status_received_cb (ulong handle, Twitter.Status? status, Error error) {
		SparqlBuilder builder;
		string name;
		TimeVal tv = TimeVal ();

		get ("name", out name);

		if (error != null) {
			if (!(error is Twitter.Error.NOT_MODIFIED)) {
				warning ("An error occurred while pulling statuses: %s", error.message);
			}
			return;
		}

		builder = new SparqlBuilder.update ();

		builder.insert_open (status.url);
		builder.subject ("_:author");
		builder.predicate ("a");
		builder.object ("nco:Contact");
		builder.predicate ("nco:fullname");
		builder.object_string (status.user.name);
		builder.predicate ("nco:photo");
		builder.object_blank_open ();
		builder.predicate ("a");
		builder.object ("nfo:RemoteDataObject");
		builder.predicate ("nie:url");
		builder.object_string (status.user.profile_image_url);
		builder.object_blank_close (); /* nco:photo */

		builder.subject ("_:message");
		builder.predicate ("a");
		builder.object ("mfo:FeedMessage");

		builder.predicate ("a");
		builder.object ("nfo:RemoteDataObject");
		builder.predicate ("nie:url");
		builder.object_string (status.url);

		builder.predicate ("nmo:communicationChannel");
		builder.object_iri (channel_urn);

		builder.predicate ("nmo:messageId");
		builder.object_string (rdf_message_id (status.id));

		builder.predicate ("nmo:from");
		builder.object ("_:author");

		builder.predicate ("nie:plainTextContent");
		builder.object_string (status.text);

		if (status.reply_to_status != 0) {
			builder.predicate ("nmo:inReplyTo");
			builder.object_blank_open ();
			builder.predicate ("a");
			builder.object ("mfo:FeedMessage");
			builder.predicate ("nmo:communicationChannel");
			builder.object_iri (channel_urn);
			builder.predicate ("nmo:messageId");
			builder.object_string (rdf_message_id (status.reply_to_status));
			builder.object_blank_close ();
		}

		if (Twitter.date_to_time_val (status.created_at, out tv)) {
			builder.predicate ("nmo:receivedDate");
			builder.object_string (tv.to_iso8601 ());

			/* We receive the status in chronological order */
			last_status_timestamp = (int)tv.tv_sec;
		}

		tv.get_current_time ();
		builder.predicate ("mfo:downloadedTime");
		builder.object_string (tv.to_iso8601 ());

		builder.insert_close ();

		query_queue.append (builder.get_result ());
	}

	private void timeline_complete_cb () {
		message ("Timeline downloaded");

		query_queue.flush ();
		state_file.set_integer (STATE_FILE_GROUP, "since", (int)last_status_timestamp);
	}

	private string rdf_message_id (uint status_id)
	{
		string name;

		get ("name", out name);
		return "feed:%s:%u".printf (name, status_id);
	}

	private void association_status_changed (Object source, ParamSpec pspec) {
		MinerWebAssociationStatus status;

		get ("association-status", out status);

		switch (status) {
			case MinerWebAssociationStatus.ASSOCIATED:
				if (pull_timeout_handle != 0)
					return;

				message ("Miner is now associated. Initiating periodic pull.");
				Timeout.add_seconds (PULL_INTERVAL, pull_timeout_cb);
				Idle.add ( () => { pull_timeout_cb (); return false; });
				break;
			case MinerWebAssociationStatus.UNASSOCIATED:
				if (pull_timeout_handle == 0)
					return;

				Source.remove (pull_timeout_handle);
				break;
		}
	}

	private bool pull_timeout_cb () {
		int since;

		if (channel_urn == null) {
			message ("Feed channel not initialized yet, skipping this cycle");
			return true;
		}

		message ("Pulling new data");
		try {
			since = state_file.get_integer (STATE_FILE_GROUP, "since");
		} catch (Error error) {
			critical ("Cannot load config variable: %s", error.message);
			return true;
		}

		service.get_friends_timeline ("", since);
		return true;
	}

	private void load_state_file () {
		string name;
		get ("name", out name);

		try {
			state_file.load_from_file (Path.build_filename (Environment.get_user_cache_dir (),
			                                                "tracker",
			                                                STATE_FILE_NAME.printf (name)),
			                           KeyFileFlags.NONE);
		} catch (Error error) {
			message ("Couldn't load the state file");
		}


		try {
			state_file.get_integer (STATE_FILE_GROUP, "since");
		} catch (Error error) {
			state_file.set_integer (STATE_FILE_GROUP, "since", 0);
		}
	}

	private void save_state_file () {
		string name;
		string file_path;

		get ("name", out name);
		file_path = Path.build_filename (Environment.get_user_cache_dir (),
			                             "tracker",
		                                 STATE_FILE_NAME.printf (name));

		try {
			FileUtils.set_contents (file_path, state_file.to_data ());
		} catch (Error error) {
			warning ("Couldn't save state file: %s", error.message);
		}
	}

	// If we don't protect the function with a mutex, it could be called by the
	// inner loop, starting at inner-inner one, and so on...
	private Mutex authenticate_mutex = new Mutex ();
	public override void authenticate () throws MinerWebError {
		PasswordProvider password_provider;
		string name;
		string username;
		string password;
		bool verified = false;
		Error twitter_error = null;

		if(!authenticate_mutex.trylock ()) {
			warning ("authenticate called while it was still running");
			return;
		}

		password_provider = PasswordProvider.get ();
		get ("name", out name);

		set ("association-status", MinerWebAssociationStatus.UNASSOCIATED);

		try {
			password = password_provider.get_password (name, out username);
		} catch (Error e) {
			authenticate_mutex.unlock ();
			if (e is PasswordProviderError.SERVICE) {
				throw new MinerWebError.KEYRING (e.message);
			}
			if (e is PasswordProviderError.NOTFOUND) {
				throw new MinerWebError.NO_CREDENTIALS ("Miner is not associated");
			}

			critical ("Internal error: %s", e.message);
			return;
		}

		message ("Verifying username and password");
		service.set_user (username, password);
		service.verify_user ();

		var wait_loop = new MainLoop (null, false);
		service.user_verified.connect ( (h, v, e) => {
				verified = v;
				twitter_error = e;
				wait_loop.quit (); });
		wait_loop.run ();
		authenticate_mutex.unlock ();

		if (twitter_error != null) {
			throw new MinerWebError.SERVICE (twitter_error.message);
		}

		if (!verified) {
			throw new MinerWebError.WRONG_CREDENTIALS ("Wrong username and/or password");
		} else {
			message ("Authentication sucessful");
			set ("association-status", MinerWebAssociationStatus.ASSOCIATED);
		}

		return;
	}

	public override void dissociate () throws MinerWebError {
		var password_provider = PasswordProvider.get ();
		string name;
		get ("name", out name);

		try {
			password_provider.forget_password (name);
		} catch (Error e) {
			if (e is PasswordProviderError.SERVICE) {
				throw new MinerWebError.KEYRING (e.message);
			}

			critical ("Internal error: %s", e.message);
			return;
		}

		set ("association-status", MinerWebAssociationStatus.UNASSOCIATED);
	}

	public override void associate (HashTable<string, string> association_data) throws Tracker.MinerWebError {
		assert (association_data.lookup ("username") != null && association_data.lookup ("password") != null);

		var password_provider = PasswordProvider.get ();
		string name;
		get ("name", out name);

		try {
			password_provider.store_password (name,
			                                  SERVICE_DESCRIPTION.printf (name),
			                                  association_data.lookup ("username"),
			                                  association_data.lookup ("password"));
		} catch (Error e) {
			if (e is PasswordProviderError.SERVICE) {
				throw new MinerWebError.KEYRING (e.message);
			}

			critical ("Internal error: %s", e.message);
			return;
		}
	}

	public GLib.HashTable get_association_data () throws Tracker.MinerWebError {
		return new HashTable<string, string>(str_hash, str_equal);
	}

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
	}

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
		Environment.set_application_name ("Twitter/Identi.ca tracker miner");
		MinerTwitter twitter_miner;
		twitter_miner = Object.new (typeof (MinerTwitter),
		                                    "provider", Twitter.Provider.DEFAULT_PROVIDER) as MinerTwitter;

		MinerTwitter identica_miner;
		identica_miner = Object.new (typeof (MinerTwitter),
		                                     "provider", Twitter.Provider.IDENTI_CA) as MinerTwitter;

		init_signals ();

		main_loop = new MainLoop (null, false);
		main_loop.run ();

		twitter_miner.shutdown ();
		identica_miner.shutdown ();
	}
}

} // End namespace Tracker
