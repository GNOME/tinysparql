using Tracker;
using Tracker.Sparql;


public class TestApp : GLib.Object {
	MainLoop loop;
	Sparql.Connection con;

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

	void update_query () {
		Cursor cursor;
		int a;

		try {
			con.update ("INSERT { <test01> a nie:InformationElement ; nie:title 'test01' }");
		} catch (Tracker.Sparql.Error ea) {
			warning ("Couldn't update: %s", ea.message);
		}

		try {
			cursor = con.query ("SELECT ?title WHERE { <test01> nie:title ?title }");
			a = iter_cursor (cursor);
		} catch (Tracker.Sparql.Error eb) {
			warning ("Couldn't query: %s", eb.message);
		}

	}

	async void update_query_async () {
		Cursor cursor;
		int a;

		try {
			yield con.update_async ("INSERT { <test01> a nie:InformationElement ; nie:title 'test01' }");
		} catch (Tracker.Sparql.Error ea) {
			warning ("Couldn't update: %s", ea.message);
		}

		try {
			cursor = con.query ("SELECT ?title WHERE { <test01> nie:title ?title }");
			a = iter_cursor (cursor);
		} catch (Tracker.Sparql.Error eb) {
			warning ("Couldn't query: %s", eb.message);
		}

	}

	void do_sync_tests () {
		update_query ();
	}

	async void do_async_tests () {
		yield update_query_async ();

		print ("Async tests done, now I can quit the mainloop\n");
		loop.quit ();
	}

	bool in_mainloop () {

		do_sync_tests ();
		do_async_tests ();

		return false;
	}

	public void run () {
		loop = new MainLoop (null, false);

		Idle.add (in_mainloop);

		loop.run ();
	}
}
