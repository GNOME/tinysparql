using Tracker;
using Tracker.Sparql;

private int iter_cursor (Cursor cursor)
{
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

int
main( string[] args )
{
	Sparql.Connection con = new Tracker.Bus.Connection ();
	Cursor cursor;
	int a;

	try {
		cursor = con.query ("SELECT ?u WHERE { ?u a rdfs:Class }");
	} catch (GLib.Error e) {
		warning ("Couldn't perform query: %s", e.message);
		return -1;
	}

	a = iter_cursor (cursor);

	if (a == -1)
		return a;

	print ("\nRewinding\n");
	cursor.rewind ();

	print ("\nSecond run\n");
	a = iter_cursor (cursor);


	return a;
}
