using Tracker;
using Tracker.Sparql;

private static int res;
private static MainLoop loop;

private async void test_async () {
	// Quite this loo because we start another one in app.run ()
	loop.quit ();

	try {
		Connection c;

		// Test async
		print ("Getting connection asynchronously\n");
		c = yield Connection.get_async ();

		print ("Got it %p\n", c);

		print ("Creating app with connection\n");
		TestApp app = new TestApp (c);

		print ("Running app\n");
		res = app.run();
	} catch (GLib.IOError e1) {
		warning ("Couldn't perform test: %s", e1.message);
	} catch (Tracker.Sparql.Error e2) {
		warning ("Couldn't perform test: %s", e2.message);
	}

	print ("\n");
}

private void test_sync () {
	try {
		Connection c;

		// Test async
		print ("Getting connection synchronously\n");
		c = Connection.get ();

		print ("Got it %p\n", c);

		print ("Creating app with connection\n");
		TestApp app = new TestApp (c);

		print ("Running app\n");
		res = app.run();
	} catch (GLib.IOError e1) {
		warning ("Couldn't perform test: %s", e1.message);
	} catch (Tracker.Sparql.Error e2) {
		warning ("Couldn't perform test: %s", e2.message);
	}

	print ("\n");
}

int
main( string[] args )
{
	print ("Starting...\n");
	loop = new MainLoop (null, false);

	test_sync ();

	if (res < 0) {
		return res;
	}

	test_async.begin ();
	loop.run ();

	return res;
}
