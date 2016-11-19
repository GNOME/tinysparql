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

public class Tracker.SparqlScanner : Object {
	char* current;
	char* end;

	int line;
	int column;

	public SparqlScanner (char* input, size_t len) {
		char* begin = input;
		end = begin + len;

		current = begin;

		line = 1;
		column = 1;
	}

	public void seek (SourceLocation location) {
		current = location.pos;
		line = location.line;
		column = location.column;
	}

	SparqlTokenType get_identifier_or_keyword (char* begin, int len) {
		switch (len) {
		case 1:
			switch (begin[0]) {
			case 'A':
			case 'a':
				return SparqlTokenType.A;
			}
			break;
		case 2:
			switch (begin[0]) {
			case 'A':
			case 'a':
				if (matches (begin, "AS")) return SparqlTokenType.AS;
				break;
			case 'B':
			case 'b':
				if (matches (begin, "BY")) return SparqlTokenType.BY;
				break;
			case 'i':
			case 'I':
				switch (begin[1]) {
				case 'F':
				case 'f':
					if (matches (begin, "IF")) return SparqlTokenType.IF;
					break;
				case 'N':
				case 'n':
					if (matches (begin, "IN")) return SparqlTokenType.OP_IN;
					break;
				}
				break;
			case 'O':
			case 'o':
				if (matches (begin, "OR")) return SparqlTokenType.OR;
				break;
			}
			break;
		case 3:
			switch (begin[0]) {
			case 'A':
			case 'a':
				switch (begin[1]) {
				case 'S':
				case 's':
					switch (begin[2]) {
					case 'C':
					case 'c':
						if (matches (begin, "ASC")) return SparqlTokenType.ASC;
						break;
					case 'K':
					case 'k':
						if (matches (begin, "ASK")) return SparqlTokenType.ASK;
						break;
					}
					break;
				case 'B':
				case 'b':
					if (matches (begin, "ABS")) return SparqlTokenType.ABS;
					break;
				case 'V':
				case 'v':
					if (matches (begin, "AVG")) return SparqlTokenType.AVG;
					break;
				}
				break;
			case 'D':
			case 'd':
				if (matches (begin, "DAY")) return SparqlTokenType.DAY;
				break;
			case 'M':
			case 'm':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "MAX")) return SparqlTokenType.MAX;
					break;
				case 'D':
				case 'd':
					if (matches (begin, "MD5")) return SparqlTokenType.MD5;
					break;
				case 'I':
				case 'i':
					if (matches (begin, "MIN")) return SparqlTokenType.MIN;
					break;
				}
				break;
			case 'N':
			case 'n':
				switch (begin[2]) {
				case 'T':
				case 't':
					if (matches (begin, "NOT")) return SparqlTokenType.NOT;
					break;
				case 'W':
				case 'w':
					if (matches (begin, "NOW")) return SparqlTokenType.NOW;
					break;
				}
				break;
			case 'S':
			case 's':
				switch (begin[1]) {
				case 'T':
				case 't':
					if (matches (begin, "STR")) return SparqlTokenType.STR;
					break;
				case 'U':
				case 'u':
					if (matches (begin, "SUM")) return SparqlTokenType.SUM;
					break;
				}
				break;
			}
			break;
		case 4:
			switch (begin[0]) {
			case 'B':
			case 'b':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "BASE")) return SparqlTokenType.BASE;
					break;
				case 'I':
				case 'i':
					if (matches (begin, "BIND")) return SparqlTokenType.BIND;
					break;
				}
				break;
			case 'C':
			case 'c':
				if (matches (begin, "CEIL")) return SparqlTokenType.CEIL;
				break;
			case 'D':
			case 'd':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "DATA")) return SparqlTokenType.DATA;
					break;
				case 'E':
				case 'e':
					if (matches (begin, "DESC")) return SparqlTokenType.DESC;
					break;
				case 'R':
				case 'r':
					if (matches (begin, "DROP")) return SparqlTokenType.DROP;
					break;
				}
				break;
			case 'F':
			case 'f':
				if (matches (begin, "FROM")) return SparqlTokenType.FROM;
				break;
			case 'I':
			case 'i':
				if (matches (begin, "INTO")) return SparqlTokenType.INTO;
				break;
			case 'L':
			case 'l':
				if (matches (begin, "LANG")) return SparqlTokenType.LANG;
				break;
			case 'N':
			case 'n':
				if (matches (begin, "NULL")) return SparqlTokenType.NULL;
				break;
			case 'R':
			case 'r':
				if (matches (begin, "RAND")) return SparqlTokenType.RAND;
				break;
			case 'S':
			case 's':
				if (matches (begin, "SHA1")) return SparqlTokenType.SHA1;
				break;
			case 'T':
			case 't':
				if (matches (begin, "TRUE")) return SparqlTokenType.TRUE;
				break;
			case 'W':
			case 'w':
				if (matches (begin, "WITH")) return SparqlTokenType.WITH;
				break;
			case 'Y':
			case 'y':
				if (matches (begin, "YEAR")) return SparqlTokenType.YEAR;
				break;
			}
			break;
		case 5:
			switch (begin[0]) {
			case 'B':
			case 'b':
				if (matches (begin, "BOUND")) return SparqlTokenType.BOUND;
				break;
			case 'C':
			case 'c':
				if (matches (begin, "COUNT")) return SparqlTokenType.COUNT;
				break;
			case 'G':
			case 'g':
				switch (begin[2]) {
				case 'A':
				case 'a':
					if (matches (begin, "GRAPH")) return SparqlTokenType.GRAPH;
					break;
				case 'O':
				case 'o':
					if (matches (begin, "GROUP")) return SparqlTokenType.GROUP;
					break;
				}
				break;
			case 'H':
			case 'h':
				if (matches (begin, "HOURS")) return SparqlTokenType.HOURS;
				break;
			case 'L':
			case 'l':
				switch (begin[1]) {
				case 'C':
				case 'c':
					if (matches (begin, "LCASE")) return SparqlTokenType.LCASE;
					break;
				case 'I':
				case 'i':
					if (matches (begin, "LIMIT")) return SparqlTokenType.LIMIT;
					break;
				}
				break;
			case 'M':
			case 'm':
				if (matches (begin, "MONTH")) return SparqlTokenType.MONTH;
				break;
			case 'N':
			case 'n':
				if (matches (begin, "NAMED")) return SparqlTokenType.NAMED;
				break;
			case 'O':
			case 'o':
				if (matches (begin, "ORDER")) return SparqlTokenType.ORDER;
				break;
			case 'R':
			case 'r':
				switch (begin[1]) {
				case 'E':
				case 'e':
					if (matches (begin, "REGEX")) return SparqlTokenType.REGEX;
					break;
				case 'O':
				case 'o':
					if (matches (begin, "ROUND")) return SparqlTokenType.ROUND;
					break;
				}
				break;
			case 'U':
			case 'u':
				switch (begin[1]) {
				case 'N':
				case 'n':
					if (matches (begin, "UNION")) return SparqlTokenType.UNION;
					break;
				case 'C':
				case 'c':
					if (matches (begin, "UCASE")) return SparqlTokenType.UCASE;
					break;
				}
				break;
			case 'W':
			case 'w':
				if (matches (begin, "WHERE")) return SparqlTokenType.WHERE;
				break;
			case 'F':
			case 'f':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "FALSE")) return SparqlTokenType.FALSE;
					break;
				case 'L':
				case 'l':
					if (matches (begin, "FLOOR")) return SparqlTokenType.FLOOR;
					break;
				}
				break;
			case 'I':
			case 'i':
				switch (begin[1]) {
				case 'S':
				case 's':
					switch (begin[2]) {
					case 'I':
					case 'i':
						if (matches (begin, "ISIRI")) return SparqlTokenType.ISIRI;
						break;
					case 'U':
					case 'u':
						if (matches (begin, "ISURI")) return SparqlTokenType.ISURI;
						break;
					}
					break;
				}
				break;
			}
			break;
		case 6:
			switch (begin[0]) {
			case 'C':
			case 'c':
				if (matches (begin, "CONCAT")) return SparqlTokenType.CONCAT;
				break;
			case 'D':
			case 'd':
				if (matches (begin, "DELETE")) return SparqlTokenType.DELETE;
				break;
			case 'E':
			case 'e':
				if (matches (begin, "EXISTS")) return SparqlTokenType.EXISTS;
				break;
			case 'F':
			case 'f':
				if (matches (begin, "FILTER")) return SparqlTokenType.FILTER;
				break;
			case 'H':
			case 'h':
				if (matches (begin, "HAVING")) return SparqlTokenType.HAVING;
				break;
			case 'I':
			case 'i':
				if (matches (begin, "INSERT")) return SparqlTokenType.INSERT;
				break;
			case 'O':
			case 'o':
				if (matches (begin, "OFFSET")) return SparqlTokenType.OFFSET;
				break;
			case 'P':
			case 'p':
				if (matches (begin, "PREFIX")) return SparqlTokenType.PREFIX;
				break;
			case 'S':
			case 's':
				switch (begin[1]) {
				case 'E':
				case 'e':
					if (matches (begin, "SELECT")) return SparqlTokenType.SELECT;
					break;
				case 'I':
				case 'i':
					if (matches (begin, "SILENT")) return SparqlTokenType.SILENT;
					break;
				case 'T':
				case 't':
					if (matches (begin, "STRLEN")) return SparqlTokenType.STRLEN;
					break;
				case 'U':
				case 'u':
					if (matches (begin, "SUBSTR")) return SparqlTokenType.SUBSTR;
					break;
				case 'H':
				case 'h':
					switch (begin[3]) {
					case '2':
						if (matches (begin, "SHA256")) return SparqlTokenType.SHA256;
						break;
					case '5':
						if (matches (begin, "SHA512")) return SparqlTokenType.SHA512;
						break;
					}
					break;
				}
				break;
			}
			break;
		case 7:
			switch (begin[0]) {
			case 'M':
			case 'm':
				if (matches (begin, "MINUTES")) return SparqlTokenType.MINUTES;
				break;
			case 'R':
			case 'r':
				switch (begin[2]) {
				case 'D':
				case 'd':
					if (matches (begin, "REDUCED")) return SparqlTokenType.REDUCED;
					break;
				case 'P':
				case 'p':
					if (matches (begin, "REPLACE")) return SparqlTokenType.REPLACE;
					break;
				}
				break;
			case 'S':
			case 's':
				switch (begin[1]) {
				case 'E':
				case 'e':
					if (matches (begin, "SECONDS")) return SparqlTokenType.SECONDS;
					break;
				case 'T':
				case 't':
					if (matches (begin, "STRENDS")) return SparqlTokenType.STRENDS;
					break;
				}
				break;
			case 'I':
			case 'i':
				if (matches (begin, "ISBLANK")) return SparqlTokenType.ISBLANK;
				break;
			}
			break;
		case 8:
			switch (begin[0]) {
			case 'C':
			case 'c':
				switch (begin[2]) {
				case 'A':
				case 'a':
					if (matches (begin, "COALESCE")) return SparqlTokenType.COALESCE;
					break;
				case 'N':
				case 'n':
					if (matches (begin, "CONTAINS")) return SparqlTokenType.CONTAINS;
					break;
				}
				break;
			case 'D':
			case 'd':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "DATATYPE")) return SparqlTokenType.DATATYPE;
					break;
				case 'E':
				case 'e':
					if (matches (begin, "DESCRIBE")) return SparqlTokenType.DESCRIBE;
					break;
				case 'I':
				case 'i':
					if (matches (begin, "DISTINCT")) return SparqlTokenType.DISTINCT;
					break;
				}
				break;
			case 'O':
			case 'o':
				if (matches (begin, "OPTIONAL")) return SparqlTokenType.OPTIONAL;
				break;
			case 'S':
			case 's':
				switch (begin[1]) {
				case 'A':
				case 'a':
					if (matches (begin, "SAMETERM")) return SparqlTokenType.SAMETERM;
					break;
				case 'T':
				case 't':
					if (matches (begin, "STRAFTER")) return SparqlTokenType.STRAFTER;
					break;
				}
				break;
			}
			break;
		case 9:
			switch (begin[0]) {
			case 'C':
			case 'c':
				if (matches (begin, "CONSTRUCT")) return SparqlTokenType.CONSTRUCT;
				break;
			case 'I':
			case 'i':
				if (matches (begin, "ISLITERAL")) return SparqlTokenType.ISLITERAL;
				break;
			case 'S':
			case 's':
				switch (begin[3]) {
				case 'B':
				case 'b':
					if (matches (begin, "STRBEFORE")) return SparqlTokenType.STRBEFORE;
					break;
				case 'S':
				case 's':
					if (matches (begin, "STRSTARTS")) return SparqlTokenType.STRSTARTS;
					break;
				}
				break;
			}
			break;
		case 11:
			if (matches (begin, "LANGMATCHES")) return SparqlTokenType.LANGMATCHES;
			break;
		case 12:
			if (matches (begin, "GROUP_CONCAT")) return SparqlTokenType.GROUP_CONCAT;
			break;
		case 14:
			if (matches (begin, "ENCODE_FOR_URI")) return SparqlTokenType.ENCODE_FOR_URI;
			break;
		}
		return SparqlTokenType.PN_PREFIX;
	}

	SparqlTokenType read_number () {
		var type = SparqlTokenType.INTEGER;

		// integer part
		if (current < end - 2 && current[0] == '0'
		    && current[1] == 'x' && current[2].isxdigit ()) {
			// hexadecimal integer literal
			current += 2;
			while (current < end && current[0].isxdigit ()) {
				current++;
			}
		} else {
			// decimal number
			while (current < end && current[0].isdigit ()) {
				current++;
			}
		}

		// fractional part
		if (current < end - 1 && current[0] == '.' && current[1].isdigit ()) {
			type = SparqlTokenType.DOUBLE;
			current++;
			while (current < end && current[0].isdigit ()) {
				current++;
			}
		}

		// exponent part
		if (current < end && current[0].tolower () == 'e') {
			type = SparqlTokenType.DOUBLE;
			current++;
			if (current < end && (current[0] == '+' || current[0] == '-')) {
				current++;
			}
			while (current < end && current[0].isdigit ()) {
				current++;
			}
		}

		// type suffix
		if (current < end) {
			switch (current[0]) {
			case 'l':
			case 'L':
				if (type == SparqlTokenType.INTEGER) {
					current++;
					if (current < end && current[0].tolower () == 'l') {
						current++;
					}
				}
				break;
			case 'u':
			case 'U':
				if (type == SparqlTokenType.INTEGER) {
					current++;
					if (current < end && current[0].tolower () == 'l') {
						current++;
						if (current < end && current[0].tolower () == 'l') {
							current++;
						}
					}
				}
				break;
			case 'f':
			case 'F':
			case 'd':
			case 'D':
				type = SparqlTokenType.DOUBLE;
				current++;
				break;
			}
		}

		return type;
	}

	bool is_pn_char (char c) {
		return (c.isalnum () || c == '_' || c == '-');
	}

	bool is_pn_local_char (char c) {
		return (c.isalnum () || c == '_' || c == '-' || c == '.');
	}

	bool is_varname_char (char c) {
		return (c.isalnum () || c == '_');
	}

	public SparqlTokenType read_token (out SourceLocation token_begin, out SourceLocation token_end) throws Sparql.Error {
		space ();

		SparqlTokenType type;
		char* begin = current;
		token_begin = SourceLocation (begin, line, column);

		int token_length_in_chars = -1;

		if (current >= end) {
			type = SparqlTokenType.EOF;
		} else if (current[0].isalpha ()) {
			// keyword or prefixed name
			int len = 0;
			while (current < end && is_pn_char (current[0])) {
				current++;
				len++;
			}
			type = get_identifier_or_keyword (begin, len);
		} else if (current[0].isdigit ()) {
			type = read_number ();
		} else {
			switch (current[0]) {
			case '{':
				type = SparqlTokenType.OPEN_BRACE;
				current++;
				break;
			case '}':
				type = SparqlTokenType.CLOSE_BRACE;
				current++;
				break;
			case '(':
				type = SparqlTokenType.OPEN_PARENS;
				current++;
				break;
			case ')':
				type = SparqlTokenType.CLOSE_PARENS;
				current++;
				break;
			case '[':
				type = SparqlTokenType.OPEN_BRACKET;
				current++;
				break;
			case ']':
				type = SparqlTokenType.CLOSE_BRACKET;
				current++;
				break;
			case '.':
				type = SparqlTokenType.DOT;
				current++;
				break;
			case ':':
				type = SparqlTokenType.COLON;
				current++;
				while (current < end && is_pn_local_char (current[0])) {
					current++;
				}
				if (current[-1] == '.') {
					// last character must not be a dot (conflict with SparqlTokenType.DOT)
					current--;
				}
				break;
			case ',':
				type = SparqlTokenType.COMMA;
				current++;
				break;
			case ';':
				type = SparqlTokenType.SEMICOLON;
				current++;
				break;
			case '?':
			case '$':
				type = SparqlTokenType.NONE;
				current++;
				while (current < end && is_varname_char (current[0])) {
					type = SparqlTokenType.VAR;
					current++;
				}
				break;
			case '@':
				type = SparqlTokenType.NONE;
				current++;
				if (current < end - "prefix".length && matches (current, "PREFIX")) {
					type = SparqlTokenType.ATPREFIX;
					current += "prefix".length;
				} else if (current < end - "base".length && matches (current, "BASE")) {
					type = SparqlTokenType.ATBASE;
					current += "base".length;
				}
				break;
			case '|':
				type = SparqlTokenType.NONE;
				current++;
				if (current < end) {
					switch (current[0]) {
					case '|':
						type = SparqlTokenType.OP_OR;
						current++;
						break;
					}
				}
				break;
			case '&':
				type = SparqlTokenType.NONE;
				current++;
				if (current < end) {
					switch (current[0]) {
					case '&':
						type = SparqlTokenType.OP_AND;
						current++;
						break;
					}
				}
				break;
			case '=':
				type = SparqlTokenType.OP_EQ;
				current++;
				break;
			case '<':
				type = SparqlTokenType.OP_LT;
				current++;
				if (current < end) {
					// check whether token is an IRI
					while (current < end && current[0] != '>') {
						if (current[0] >= 0x00 && current[0] < 0x20) {
							// control character, not an IRI
							break;
						}
						switch (current[0]) {
						case '<':
						case '>':
						case '"':
						case ' ':
						case '{':
						case '}':
						case '|':
						case '^':
						case '`':
						case '\\':
							// not an IRI
							break;
						default:
							current++;
							continue;
						}
						break;
					}
					if (current < end && current[0] == '>') {
						type = SparqlTokenType.IRI_REF;
						current++;
						break;
					} else {
						current = begin + 1;
					}
					switch (current[0]) {
					case '=':
						type = SparqlTokenType.OP_LE;
						current++;
						break;
					}
				}
				break;
			case '>':
				type = SparqlTokenType.OP_GT;
				current++;
				if (current < end && current[0] == '=') {
					type = SparqlTokenType.OP_GE;
					current++;
				}
				break;
			case '!':
				type = SparqlTokenType.OP_NEG;
				current++;
				if (current < end && current[0] == '=') {
					type = SparqlTokenType.OP_NE;
					current++;
				}
				break;
			case '+':
				type = SparqlTokenType.PLUS;
				current++;
				break;
			case '-':
				type = SparqlTokenType.MINUS;
				current++;
				break;
			case '*':
				type = SparqlTokenType.STAR;
				current++;
				break;
			case '/':
				type = SparqlTokenType.DIV;
				current++;
				break;
			case '\'':
			case '"':
				if (current < end - 6 && begin[1] == begin[0] && begin[2] == begin[0]) {
					if (begin[0] == '\'') {
						type = SparqlTokenType.STRING_LITERAL_LONG1;
					} else {
						type = SparqlTokenType.STRING_LITERAL_LONG2;
					}

					token_length_in_chars = 6;
					current += 3;
					while (current < end - 4) {
						if (current[0] == begin[0] && current[1] == begin[0] && current[2] == begin[0]) {
							break;
						} else if (current[0] == '\n') {
							current++;
							line++;
							column = 1;
							token_length_in_chars = 3;
						} else if ((uchar) current[0] <= 0x7f) {
							// ASCII
							current++;
							token_length_in_chars++;
						} else {
							unichar u = ((string) current).get_char_validated ((long) (end - current));
							if (u != (unichar) (-1)) {
								current += u.to_utf8 (null);
								token_length_in_chars++;
							} else {
								throw new Sparql.Error.PARSE ("%d.%d: invalid UTF-8 character", line, column + token_length_in_chars);
							}
						}
					}
					if (current[0] == begin[0] && current[1] == begin[0] && current[2] == begin[0]) {
						current += 3;
					} else {
						throw new Sparql.Error.PARSE ("%d.%d: syntax error, expected \"\"\"", line, column + token_length_in_chars);
					}
					break;
				}

				if (begin[0] == '\'') {
					type = SparqlTokenType.STRING_LITERAL1;
				} else {
					type = SparqlTokenType.STRING_LITERAL2;
				}

				token_length_in_chars = 2;
				current++;
				while (current < end && current[0] != begin[0]) {
					if (current[0] == '\\') {
						current++;
						token_length_in_chars++;
						if (current >= end) {
							break;
						}

						switch (current[0]) {
						case '\'':
						case '"':
						case '\\':
						case 'b':
						case 'f':
						case 'n':
						case 'r':
						case 't':
							current++;
							token_length_in_chars++;
							break;
						case 'u':
							for (int i = 0; i < 4; i++) {
								if (current + i + 1 >= end || !current[i + 1].isxdigit ()) {
									throw new Sparql.Error.PARSE ("%d.%d: invalid escape sequence", line, column + token_length_in_chars);
								}
							}
							current += 5;
							token_length_in_chars += 5;
							break;
						default:
							throw new Sparql.Error.PARSE ("%d.%d: invalid escape sequence", line, column + token_length_in_chars);
						}
					} else if (current[0] == '\n') {
						break;
					} else if ((uchar) current[0] <= 0x7f) {
						// ASCII
						current++;
						token_length_in_chars++;
					} else {
						unichar u = ((string) current).get_char_validated ((long) (end - current));
						if (u != (unichar) (-1)) {
							current += u.to_utf8 (null);
							token_length_in_chars++;
						} else {
							current++;
							throw new Sparql.Error.PARSE ("%d.%d: invalid UTF-8 character", line, column + token_length_in_chars);
						}
					}
				}
				if (current < end && current[0] != '\n') {
					current++;
				} else {
					throw new Sparql.Error.PARSE ("%d.%d: syntax error, expected %c", line, column + token_length_in_chars, begin[0]);
				}
				break;
			case '^':
				type = SparqlTokenType.NONE;
				if (current < end - 2 && current[0] == current[1]) {
					type = SparqlTokenType.DOUBLE_CIRCUMFLEX;
					current += 2;
				} else {
					throw new Sparql.Error.PARSE ("%d.%d: syntax error, unexpected character", line, column);
				}
				break;
			case '_':
				type = SparqlTokenType.BLANK_NODE;
				current++;
				break;
			default:
				unichar u = ((string) current).get_char_validated ((long) (end - current));
				if (u != (unichar) (-1)) {
					throw new Sparql.Error.PARSE ("%d.%d: syntax error, unexpected character", line, column);
				} else {
					throw new Sparql.Error.PARSE ("%d.%d: invalid UTF-8 character", line, column);
				}
			}
		}

		if (token_length_in_chars < 0) {
			column += (int) (current - begin);
		} else {
			column += token_length_in_chars;
		}

		token_end = SourceLocation (current, line, column - 1);

		return type;
	}

	bool matches (char* begin, string keyword) {
		char* keyword_array = (char*) keyword;
		for (int i = 0; keyword_array[i] != 0; i++) {
			if (begin[i].toupper () != keyword_array[i]) {
				return false;
			}
		}
		return true;
	}

	bool whitespace () {
		bool found = false;
		while (current < end && current[0].isspace ()) {
			if (current[0] == '\n') {
				line++;
				column = 0;
			}
			found = true;
			current++;
			column++;
		}
		return found;
	}

	bool comment () {
		if (current >= end || current[0] != '#') {
			return false;
		}

		// single-line comment
		// skip until end of line or end of file
		while (current < end && current[0] != '\n') {
			current++;
		}

		return true;
	}

	void space () {
		while (whitespace () || comment ()) {
		}
	}
}

/**
 * Represents a position in a source file.
 */
public struct Tracker.SourceLocation {
	public char* pos;
	public int line;
	public int column;

	public SourceLocation (char* pos, int line, int column) {
		this.pos = pos;
		this.line = line;
		this.column = column;
	}
}

public enum Tracker.SparqlTokenType {
	NONE,
	A,
	ABS,
	AS,
	ASC,
	ASK,
	ATBASE,
	ATPREFIX,
	AVG,
	BASE,
	BLANK_NODE,
	BIND,
	BOUND,
	BY,
	CEIL,
	CLOSE_BRACE,
	CLOSE_BRACKET,
	CLOSE_PARENS,
	COALESCE,
	COLON,
	CONCAT,
	CONTAINS,
	COMMA,
	CONSTRUCT,
	COUNT,
	DATA,
	DATATYPE,
	DAY,
	DECIMAL,
	DELETE,
	DESC,
	DESCRIBE,
	DISTINCT,
	DIV,
	DOT,
	DOUBLE,
	DOUBLE_CIRCUMFLEX,
	DROP,
	ENCODE_FOR_URI,
	EOF,
	EXISTS,
	FALSE,
	FILTER,
	FLOOR,
	FROM,
	GRAPH,
	GROUP,
	GROUP_CONCAT,
	HAVING,
	HOURS,
	IF,
	INSERT,
	INTEGER,
	INTO,
	IRI_REF,
	ISBLANK,
	ISIRI,
	ISLITERAL,
	ISURI,
	LANG,
	LANGMATCHES,
	LCASE,
	LIMIT,
	MAX,
	MD5,
	MIN,
	MINUS,
	MINUTES,
	MONTH,
	NAMED,
	NOT,
	NOW,
	NULL, /* Non-standard in SPARQL spec */
	OFFSET,
	OP_AND,
	OP_EQ,
	OP_GE,
	OP_GT,
	OP_LE,
	OP_LT,
	OP_NE,
	OP_NEG,
	OP_OR,
	OP_IN,
	OPEN_BRACE,
	OPEN_BRACKET,
	OPEN_PARENS,
	OPTIONAL,
	OR,
	ORDER,
	PLUS,
	PN_PREFIX,
	PREFIX,
	RAND,
	REDUCED,
	REGEX,
	REPLACE,
	ROUND,
	SAMETERM,
	SECONDS,
	SELECT,
	SEMICOLON,
	SHA1,
	SHA256,
	SHA512,
	SILENT,
	STAR,
	STR,
	STRAFTER,
	STRBEFORE,
	STRENDS,
	STRING_LITERAL1,
	STRING_LITERAL2,
	STRING_LITERAL_LONG1,
	STRING_LITERAL_LONG2,
	STRLEN,
	STRSTARTS,
	SUBSTR,
	SUM,
	TRUE,
	UCASE,
	UNION,
	VAR,
	WHERE,
	WITH,
	YEAR;

	public unowned string to_string () {
		switch (this) {
		case A: return "`a'";
		case ABS: return "`ABS'";
		case AS: return "`AS'";
		case ASC: return "`ASC'";
		case ASK: return "`ASK'";
		case ATBASE: return "`@base'";
		case ATPREFIX: return "`@prefix'";
		case AVG: return "`AVG'";
		case BASE: return "`BASE'";
		case BLANK_NODE: return "blank node";
		case BIND: return "`BIND'";
		case BOUND: return "`BOUND'";
		case BY: return "`BY'";
		case CEIL: return "`CEIL'";
		case CLOSE_BRACE: return "`}'";
		case CLOSE_BRACKET: return "`]'";
		case CLOSE_PARENS: return "`)'";
		case COALESCE: return "`COALESCE'";
		case COLON: return "`:'";
		case CONCAT: return "`CONCAT'";
		case CONTAINS: return "`CONTAINS'";
		case COMMA: return "`,'";
		case CONSTRUCT: return "`CONSTRUCT'";
		case COUNT: return "`COUNT'";
		case DATATYPE: return "`DATATYPE'";
		case DAY: return "`DAY'";
		case DECIMAL: return "`DECIMAL'";
		case DELETE: return "`DELETE'";
		case DESC: return "`DESC'";
		case DESCRIBE: return "`DESCRIBE'";
		case DISTINCT: return "`DISTINCT'";
		case DOUBLE: return "`DOUBLE'";
		case DOUBLE_CIRCUMFLEX: return "`^^'";
		case DROP: return "`DROP'";
		case ENCODE_FOR_URI: return "`ENCODE_FOR_URI'";
		case EOF: return "end of file";
		case EXISTS: return "`EXISTS'";
		case FALSE: return "`false'";
		case FILTER: return "`FILTER'";
		case FLOOR: return "`FLOOR'";
		case FROM: return "`FROM'";
		case GRAPH: return "`GRAPH'";
		case GROUP: return "`GROUP'";
		case GROUP_CONCAT: return "`GROUP_CONCAT'";
		case HAVING: return "`HAVING'";
		case HOURS: return "`HOURS'";
		case IF: return "`IF'";
		case INSERT: return "`INSERT'";
		case INTEGER: return "`INTEGER'";
		case INTO: return "`INTO'";
		case IRI_REF: return "IRI reference";
		case ISBLANK: return "`ISBLANK'";
		case ISIRI: return "`ISIRI'";
		case ISLITERAL: return "`ISLITERAL'";
		case ISURI: return "`ISURI'";
		case LANG: return "`LANG'";
		case LANGMATCHES: return "`LANGMATCHES'";
		case LCASE: return "`LCASE'";
		case LIMIT: return "`LIMIT'";
		case MAX: return "`MAX'";
		case MD5: return "`MD5'";
		case MIN: return "`MIN'";
		case MINUS: return "`-'";
		case MINUTES: return "`MINUTES'";
		case MONTH: return "`MONTH'";
		case NAMED: return "`NAMED'";
		case NOT: return "`NOT'";
		case NOW: return "`NOW'";
		case OFFSET: return "`OFFSET'";
		case OP_AND: return "`&&'";
		case OP_EQ: return "`='";
		case OP_GE: return "`>='";
		case OP_GT: return "`>'";
		case OP_LE: return "`<='";
		case OP_LT: return "`<'";
		case OP_NE: return "`!='";
		case OP_NEG: return "`!'";
		case OP_OR: return "`||'";
		case OP_IN: return "`IN'";
		case OPEN_BRACE: return "`{'";
		case OPEN_BRACKET: return "`['";
		case OPEN_PARENS: return "`('";
		case OPTIONAL: return "`OPTIONAL'";
		case OR: return "`OR'";
		case ORDER: return "`ORDER'";
		case PLUS: return "`+'";
		case PN_PREFIX: return "prefixed name";
		case PREFIX: return "`PREFIX'";
		case RAND: return "`RAND'";
		case REDUCED: return "`REDUCED'";
		case REGEX: return "`REGEX'";
		case REPLACE: return "`REPLACE'";
		case ROUND: return "`ROUND'";
		case SAMETERM: return "`SAMETERM'";
		case SECONDS: return "`SECONDS'";
		case SELECT: return "`SELECT'";
		case SEMICOLON: return "`;'";
		case SHA1: return "`SHA1'";
		case SHA256: return "`SHA256'";
		case SHA512: return "`SHA512'";
		case SILENT: return "`SILENT'";
		case STAR: return "`*'";
		case STR: return "`STR'";
		case STRAFTER: return "`STRAFTER'";
		case STRBEFORE: return "`STRBEFORE'";
		case STRENDS: return "`STRENDS'";
		case STRING_LITERAL1: return "string literal";
		case STRING_LITERAL2: return "string literal";
		case STRING_LITERAL_LONG1: return "string literal";
		case STRING_LITERAL_LONG2: return "string literal";
		case STRLEN: return "`STRLEN'";
		case STRSTARTS: return "`STRSTARTS'";
		case SUBSTR: return "`SUBSTR'";
		case SUM: return "`SUM'";
		case TRUE: return "`true'";
		case UCASE: return "`UCASE'";
		case UNION: return "`UNION'";
		case VAR: return "variable";
		case WHERE: return "`WHERE'";
		case WITH: return "`WITH'";
		case YEAR: return "`YEAR'";
		default: return "unknown token";
		}
	}
}

