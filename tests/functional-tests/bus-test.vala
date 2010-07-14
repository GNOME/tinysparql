using Tracker;
using Tracker.Sparql;

int
main( string[] args )
{
	Sparql.Connection con = new Tracker.Bus.Connection ();
	Cursor cursor;

	try {
		cursor = con.query ("SELECT ?u WHERE { ?u a rdfs:Class }");
	} catch (GLib.Error e) {
		warning ("Couldn't perform query: %s", e.message);
		return -1;
	}

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

	// Testing new API with GModule
	
//	print ("\n\n");

//	Lookup foo = new Lookup ();


	return( 0 );
}

