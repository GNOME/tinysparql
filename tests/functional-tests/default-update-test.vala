int
main( string[] args )
{
	int res = -1;

	try {
		TestApp app = new TestApp (Tracker.Sparql.Connection.get());
		res = app.run();
	} catch (Tracker.Sparql.Error e) {
		warning ("Couldn't perform test: %s", e.message);
	}

	return res;
}
