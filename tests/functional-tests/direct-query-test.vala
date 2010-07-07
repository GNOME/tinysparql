using Tracker;
using Tracker.Sparql;

int
main( string[] args )
{
	int res = -1;

	try {
		TestApp app = new TestApp (new Tracker.Direct.Connection ());
		res = app.run ();
	} catch (Sparql.Error e) {
		warning ("Couldn't perform test: %s", e.message);
	}

	return res;
}
