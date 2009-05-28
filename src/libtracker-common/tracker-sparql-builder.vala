/*
 * Copyright (C) 2009, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

public class Tracker.SparqlBuilder : Object {
	enum State {
		UPDATE,
		INSERT,
		DELETE,
		SUBJECT,
		PREDICATE,
		OBJECT,
		BLANK
	}

	public string result {
		get { return str.str; }
	}

	State state {
		get { return states[states.length - 1]; }
	}

	State[] states;
	StringBuilder str = new StringBuilder ();

	public SparqlBuilder.update () {
		states += State.UPDATE;
	}

	public void insert_open ()
		requires (state == State.UPDATE)
	{
		states += State.INSERT;
		str.append ("INSERT {\n");
	}

	public void insert_close ()
		requires (state == State.INSERT || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (".\n");
			states.length -= 3;
		}
		states.length--;
		str.append ("}\n");
	}

	public void subject_iri (string iri) {
		subject ("<%s>".printf (iri));
	}

	public void subject (string s)
		requires (state == State.INSERT || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (".\n");
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
			str.append (";");
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
			str.append (",");
			states.length--;
		}
		str.append (" ");
		str.append (s);
		states += State.OBJECT;
	}

	public void object_string (string literal)
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (",");
			states.length--;
		}

		str.append (" \"");

		char* p = literal;
		while (*p != '\0') {
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
			case '\b':
				str.append ("\\b");
				break;
			case '\f':
				str.append ("\\f");
				break;
			case '"':
				str.append ("\\\"");
				break;
			case '\\':
				str.append ("\\\\");
				break;
			default:
				str.append_c (*p);
				break;
			}
			p++;
		}

		str.append ("\"");

		states += State.OBJECT;
	}

	public void object_boolean (bool literal) {
		object (literal ? "true" : "false");
	}

	public void object_blank_open ()
		requires (state == State.PREDICATE || state == State.OBJECT)
	{
		if (state == State.OBJECT) {
			str.append (",");
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
	}
}

