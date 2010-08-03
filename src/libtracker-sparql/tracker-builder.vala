/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

public class Tracker.Sparql.Builder : Object {

	public enum State {
		UPDATE,
		INSERT,
		DELETE,
		SUBJECT,
		PREDICATE,
		OBJECT,
		BLANK,
		WHERE,
		EMBEDDED_INSERT
	}

	public string result {
		get {
			warn_if_fail (states.length == 1);
			return str.str;
		}
	}

	public int length { get; private set; }

	public State state {
		get { return states[states.length - 1]; }
	}

	State[] states;
	StringBuilder str = new StringBuilder ();

	public Builder.update () {
		states += State.UPDATE;
	}

	public Builder.embedded_insert () {
		states += State.EMBEDDED_INSERT;
		states += State.INSERT;
		states += State.SUBJECT;
	}

	public void drop_graph (string iri)
		requires (state == State.UPDATE)
	{
		str.append ("DROP GRAPH <%s>\n".printf (iri));
	}

	public void insert_open (string? graph)
		requires (state == State.UPDATE)
	{
		states += State.INSERT;
		if (graph != null)
			str.append ("INSERT INTO <%s> {\n".printf (graph));
		else
			str.append ("INSERT {\n");
	}

	public void insert_silent_open (string? graph)
		requires (state == State.UPDATE)
	{
		states += State.INSERT;
		if (graph != null)
			str.append ("INSERT SILENT INTO <%s> {\n".printf (graph));
		else
			str.append ("INSERT SILENT {\n");
	}

	public void insert_close ()
		requires (state == State.INSERT || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" .\n");
			states.length -= 3;
		}
		states.length--;

		if (state != State.EMBEDDED_INSERT) {
			str.append ("}\n");
		}
	}

	public void delete_open (string? graph)
		requires (state == State.UPDATE)
	{
		states += State.DELETE;
		if (graph != null)
			str.append ("DELETE FROM <%s> {\n".printf (graph));
		else
			str.append ("DELETE {\n");
	}

	public void delete_close ()
		requires (state == State.DELETE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" .\n");
			states.length -= 3;
		}
		states.length--;

		str.append ("}\n");
	}

	public void where_open ()
	       requires (state == State.UPDATE)
	{
		states += State.WHERE;
		str.append ("WHERE {\n");
	}

	public void where_close ()
		requires (state == State.WHERE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" .\n");
			states.length -= 3;
		}
		states.length--;
		str.append ("}\n");
	}

	public void subject_variable (string var_name) {
		subject ("?%s".printf (var_name));
	}

	public void object_variable (string var_name) {
		object ("?%s".printf (var_name));
	}

	public void subject_iri (string iri) {
		subject ("<%s>".printf (iri));
	}

	public void subject (string s)
		requires (state == State.INSERT || state == State.OBJECT || state == State.EMBEDDED_INSERT || state == State.DELETE || state == State.WHERE)
	{
		if (state == State.OBJECT) {
			str.append (" .\n");
			states.length -= 3;
		}
		str.append (s);
		states += State.SUBJECT;
	}

	public void predicate_iri (string iri) {
		predicate ("<%s>".printf (iri));
	}

	public void predicate (string s)
		requires (state == State.SUBJECT || state == State.OBJECT || state == State.BLANK)
	{
		if (state == State.OBJECT) {
			str.append (" ;\n\t");
			states.length -= 2;
		}
		str.append (" ");
		str.append (s);
		states += State.PREDICATE;
	}

	public void object_iri (string iri) {
		object ("<%s>".printf (iri));
	}

	public void object (string s)
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" ,");
			states.length--;
		}
		str.append (" ");
		str.append (s);
		states += State.OBJECT;

		length++;
	}

	public void object_string (string literal)
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" ,");
			states.length--;
		}

		str.append (" \"");

		char* p = literal;
		while (*p != '\0') {
			size_t len = Posix.strcspn ((string) p, "\t\n\r\"\\");
			str.append_len ((string) p, (long) len);
			p += len;
			switch (*p) {
			case '\t':
				str.append ("\\t");
				break;
			case '\n':
				str.append ("\\n");
				break;
			case '\r':
				str.append ("\\r");
				break;
			case '"':
				str.append ("\\\"");
				break;
			case '\\':
				str.append ("\\\\");
				break;
			default:
				continue;
			}
			p++;
		}

		str.append ("\"");

		states += State.OBJECT;

		length++;
	}

	public void object_unvalidated (string value) {
		char* end;

		if (!utf8_validate (value, -1, out end)) {
			if (value != end) {
				object_string (value.ndup (end - (char*) value));
			} else {
				object_string ("(invalid data)");
			}

			return;
		}

		object_string (value);
	}

	public void object_boolean (bool literal) {
		object (literal ? "true" : "false");
	}

	public void object_int64 (int64 literal) {
		object (literal.to_string ());
	}

	public void object_date (ref time_t literal) {
		var tm = Time.gm (literal);

		object_string ("%04d-%02d-%02dT%02d:%02d:%02dZ".printf (tm.year + 1900, tm.month + 1, tm.day, tm.hour, tm.minute, tm.second));
	}

	public void object_double (double literal) {
		object (literal.to_string ());
	}

	public void object_blank_open ()
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (" ,");
			states.length--;
		}
		str.append (" [");
		states += State.BLANK;
	}

	public void object_blank_close ()
		requires (state == State.OBJECT && states[states.length - 3] == state.BLANK)
	{
		str.append ("]");
		states.length -= 3;
		states += State.OBJECT;

		length++;
	}

	public void prepend (string raw)
	{
		str.prepend ("%s\n".printf (raw));
	}

	public void append (string raw)
	{
		if (state == State.OBJECT) {
			str.append (" .\n");
			states.length -= 3;
		}

		str.append (raw);
	}
}

