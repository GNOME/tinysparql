namespace Tracker {
	[CCode (cheader_filename = "libtracker-sparql/remote/tracker-remote-statement.h")]
	class Remote.Statement : Sparql.Statement {
		public Statement (Sparql.Connection conn, string query) throws Sparql.Error;
	}

	[CCode (cheader_filename = "libtracker-sparql/remote/tracker-http.h")]
	class HttpClient : GLib.Object {
		public HttpClient ();
		public async GLib.InputStream send_message_async (string uri, string query, uint formats, GLib.Cancellable? cancellable, out SerializerFormat format) throws GLib.Error;
		public GLib.InputStream send_message (string uri, string query, uint formats, GLib.Cancellable? cancellable, out SerializerFormat format) throws GLib.Error;
	}

	[CCode (cheader_filename = "libtracker-sparql/tracker-enums-private.h")]
        enum SerializerFormat {
	        JSON,
	        XML,
	        TTL,
	        TRIG,
        }
}
