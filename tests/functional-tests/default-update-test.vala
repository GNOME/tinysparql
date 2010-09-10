using Tracker.Sparql;

private static int res;
private static MainLoop loop;

private async void get_connection (bool? direct_only = false) {
	try {
		Connection c;

		// Test async
		print ("Getting connection async (direct=%s)\n", direct_only ? "yes" : "no");
		if (direct_only) {
			c = yield Connection.get_direct_async ();
		} else {
			c = yield Connection.get_async ();
		}

		print ("Got it %p\n", c);

		// Quite this loo because we start another one in app.run ()
		loop.quit ();

		print ("Creating app with connection\n");
		TestApp app = new TestApp (c);

		print ("Running app\n");
		res = app.run();
	} catch (GLib.IOError e1) {
		warning ("Couldn't perform test: %s", e1.message);
	} catch (Tracker.Sparql.Error e2) {
		warning ("Couldn't perform test: %s", e2.message);
	}
}

int
main( string[] args )
{
	print ("Starting...\n");
	loop = new MainLoop (null, false);

	// Test non-direct first
	get_connection.begin (false);

	loop.run ();

	// Test direct first
	get_connection.begin (true);

	loop.run ();

	return res;
}
