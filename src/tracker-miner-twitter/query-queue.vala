public class QueryQueue : GLib.Object {
	/* Holds the pending sparql updates and monitors them */
	private HashTable<uint, string> queue;
	private uint cookie;

	private Mutex flush_mutex;

	private Tracker.Miner miner;

	public QueryQueue (Tracker.Miner parent) {
		miner = parent;

		queue = new HashTable<uint, string> (direct_hash, direct_equal);
		cookie = 0;

		flush_mutex = new Mutex ();
	}

	public async void append (string query) {
		uint current_cookie = cookie ++;
		queue.insert (current_cookie, query);

		message ("SPARQL query: %s", query);

		try {
			yield miner.execute_batch_update (query);
		} catch (Error tracker_error) {
			warning ("BatchUpdate query failed: %s", tracker_error.message);
		}

		queue.remove (current_cookie);
	}

	/* BLOCKING flush */
	public void flush () {
		if (!flush_mutex.trylock ()) {
			message ("There's already a flush taking place");
			return;
		}

		if (queue.size () > 0) {
			MainLoop wait_loop;
			try {
				wait_loop = new MainLoop (null, false);
				miner.commit (null, () => { wait_loop.quit (); });
				wait_loop.run ();
			} catch (Error tracker_error) {
				warning ("Commit query failed: %s", tracker_error.message);
			}
		}

		flush_mutex.unlock ();
	}

	public uint size () {
		return queue.size ();
	}
}
