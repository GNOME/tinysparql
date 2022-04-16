/*
 * Copyright (C) 2016 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */
public class Tracker.Remote.Connection : Tracker.Sparql.Connection {
	internal HttpClient _client;
	internal string _base_uri;
	internal NamespaceManager _namespaces;

	public Connection (string base_uri) {
		Object ();
		_base_uri = base_uri;
		_client = new HttpClient();
	}

	public override Sparql.Cursor query (string sparql, Cancellable? cancellable) throws GLib.Error, Sparql.Error, IOError {
		uint flags =
			(1 << SerializerFormat.JSON) |
			(1 << SerializerFormat.XML);
		SerializerFormat format;
		var istream = _client.send_message (_base_uri, sparql, flags, cancellable, out format);

		return new Tracker.Deserializer(istream, null, format);
	}

	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable) throws GLib.Error, Sparql.Error, IOError {
		uint flags =
			(1 << SerializerFormat.JSON) |
			(1 << SerializerFormat.XML);
		SerializerFormat format;
		var istream = yield _client.send_message_async (_base_uri, sparql, flags, cancellable, out format);

		return new Tracker.Deserializer(istream, null, format);
	}

	public override Sparql.Statement? query_statement (string sparql, GLib.Cancellable? cancellable = null) throws Sparql.Error {
		return new Remote.Statement (this, sparql);
	}

	public override void close () {
	}

	public async override bool close_async () throws GLib.IOError {
		return true;
	}

	public async override GLib.InputStream serialize_async (SerializeFlags flags, RdfFormat format, string sparql, GLib.Cancellable? cancellable = null) throws Sparql.Error, GLib.Error, GLib.IOError, GLib.DBusError {
		uint formats = 0;
		if (format == RdfFormat.TURTLE)
			formats = 1 << SerializerFormat.TTL;
		else if (format == RdfFormat.TRIG)
			formats = 1 << SerializerFormat.TRIG;

		SerializerFormat unused;
		return yield _client.send_message_async (_base_uri, sparql, formats, cancellable, out unused);
	}

	public override Tracker.NamespaceManager? get_namespace_manager () {
		if (_namespaces == null)
			_namespaces = new Tracker.Remote.NamespaceManager (this);

		return _namespaces;
	}
}
