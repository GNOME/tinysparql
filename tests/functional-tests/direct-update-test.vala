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
	Sparql.Connection con = new Tracker.Direct.Connection ();
	Cursor cursor;
	int a;

	try {
		con.update ("INSERT { <test01> a nie:InformationElement ; nie:title 'test01' }");
		cursor = con.query ("SELECT ?title WHERE { <test01> nie:title ?title }");
		a = iter_cursor (cursor);
	} catch (GLib.Error e) {
		warning ("Couldn't perform query: %s", e.message);
		return -1;
	}

	return a;
}
