using Tracker;
using Tracker.Sparql;

public class TestApp : GLib.Object {
	MainLoop loop;
	Sparql.Connection con;
	int res = 0;

	public TestApp (Sparql.Connection connection) {
		con = connection;
	}

	int iter_cursor (Cursor cursor) {
		try {
			while (cursor.next()) {
				int i;

				for (i = 0; i < cursor.n_columns; i++) {
					print ("%s%s", i != 0 ? ",":"", cursor.get_string (i));
				}

				print ("\n");
			}
		} catch (GLib.Error e) {
			warning ("Couldn't iterate query results: %s", e.message);
			return -1;
		}

		return (0);
	}

	private void test_query () {
		Cursor cursor;

		try {
			cursor = con.query ("SELECT ?u WHERE { ?u a rdfs:Class }");
		} catch (GLib.Error e) {
			warning ("Couldn't perform query: %s", e.message);
			res = -1;
			return;
		}

		res = iter_cursor (cursor);

		if (res == -1)
			return;

		print ("\nRewinding\n");
		cursor.rewind ();

		print ("\nSecond run\n");
		res = iter_cursor (cursor);
	}

	private async void test_query_async () {
		Cursor cursor;

		try {
			cursor = yield con.query_async ("SELECT ?u WHERE { ?u a rdfs:Class }");
		} catch (GLib.Error e) {
			warning ("Couldn't perform query: %s", e.message);
			res = -1;
			return;
		}

		res = iter_cursor (cursor);

		if (res == -1)
			return;

		print ("\nRewinding\n");
		cursor.rewind ();

		print ("\nSecond run\n");
		res = iter_cursor (cursor);
	}

	void do_sync_tests () {
		test_query ();
	}

	async void do_async_tests () {
		yield test_query_async ();

		print ("Async tests done, now I can quit the mainloop\n");
		loop.quit ();
	}

	bool in_mainloop () {

		do_sync_tests ();
		do_async_tests ();

		return false;
	}

	public int run () {
		loop = new MainLoop (null, false);

		Idle.add (in_mainloop);

		loop.run ();

		return res;
	}
}
