using Tracker;
using Tracker.Sparql;

int
main( string[] args )
{
	TestApp app = new TestApp (new  Tracker.Bus.Connection ());

	app.run ();

	return 0;
}
