namespace Tracker {
	[CCode (cheader_filename = "libtracker-sparql/remote/tracker-remote-statement.h")]
	class Remote.Statement : Sparql.Statement {
		public Statement (Sparql.Connection conn, string query) throws Sparql.Error;
	}
}
