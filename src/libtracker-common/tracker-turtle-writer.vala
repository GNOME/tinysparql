/*
 * Copyright (C) 2009, Nokia
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
 */

public class Tracker.TurtleWriter : Object {
	enum State {
		SUBJECT,
		PREDICATE,
		OBJECT,
		BLANK
	}

	DataOutputStream stream;

	State state {
		get { return states[states.length - 1]; }
	}

	State[] states;

	IOError? err;

	public TurtleWriter (OutputStream stream) {
		this.stream = new DataOutputStream (stream);
	}

	void append (string s) {
		if (err != null) {
			return;
		}
		try {
			stream.put_string (s, null);
		} catch (Error e) {
			this.err = (IOError) e;
		}
	}

	public void subject_iri (string iri) {
		subject ("<%s>".printf (iri));
	}

	public void subject (string s)
		requires (states.length == 0 || state == State.OBJECT)
	{
		if (states.length > 0 && state == State.OBJECT) {
			append (" .\n");
			states.length -= 3;
		}
		append (s);
		states += State.SUBJECT;
	}

	public void predicate_iri (string iri) {
		predicate ("<%s>".printf (iri));
	}

	public void predicate (string s)
		requires (state == State.SUBJECT || state == State.OBJECT || state == State.BLANK)
	{
		if (state == State.OBJECT) {
			append (" ;");
			states.length -= 2;
		}
		append (" ");
		append (s);
		states += State.PREDICATE;
	}

	public void object_iri (string iri) {
		object ("<%s>".printf (iri));
	}

	public void object (string s)
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			append (" ,");
			states.length--;
		}
		append (" ");
		append (s);
		states += State.OBJECT;
	}

	public void object_string (string literal)
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			append (" ,");
			states.length--;
		}

		append (" \"");

		char* p = literal;
		while (*p != '\0') {
			size_t len = Posix.strcspn ((string) p, "\t\n\r\"\\");
			append (((string) p).ndup ((long) len));
			p += len;
			switch (*p) {
			case '\t':
				append ("\\t");
				break;
			case '\n':
				append ("\\n");
				break;
			case '\r':
				append ("\\r");
				break;
			case '"':
				append ("\\\"");
				break;
			case '\\':
				append ("\\\\");
				break;
			default:
				continue;
			}
			p++;
		}

		append ("\"");

		states += State.OBJECT;
	}

	public void object_boolean (bool literal) {
		object (literal ? "true" : "false");
	}

	public void object_int64 (int64 literal) {
		object (literal.to_string ());
	}

	public void object_date (time_t literal) {
		var tm = Time.gm (literal);

		object_string ("%04d-%02d-%02dT%02d:%02d:%02d".printf (tm.year + 1900, tm.month + 1, tm.day, tm.hour, tm.minute, tm.second));
	}

	public void object_blank_open ()
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			append (" ,");
			states.length--;
		}
		append (" [");
		states += State.BLANK;
	}

	public void object_blank_close ()
		requires (state == State.OBJECT && states[states.length - 3] == state.BLANK)
	{
		append ("]");
		states.length -= 3;
		states += State.OBJECT;
	}

	public void close () throws IOError {
		if (err != null) {
			throw err;
		}
	}
}
