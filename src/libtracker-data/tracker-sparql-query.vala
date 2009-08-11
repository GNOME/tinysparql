/*
 * Copyright (C) 2008-2009, Nokia
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

public errordomain Tracker.SparqlError {
	PARSE,
	UNKNOWN_CLASS,
	UNKNOWN_PROPERTY,
	TYPE,
	INTERNAL,
	UNSUPPORTED
}

public class Tracker.SparqlQuery : Object {
	enum LiteralType {
		STRING,
		INTEGER
	}

	// Represents a SQL table
	class DataTable : Object {
		public string sql_db_tablename; // as in db schema
		public string sql_query_tablename; // temp. name, generated
		public PredicateVariable predicate_variable;
	}

	abstract class DataBinding : Object {
		public bool is_uri;
		public bool is_boolean;
		public bool is_datetime;
		public DataTable table;
		public string sql_db_column_name;
	}

	// Represents a mapping of a SPARQL literal to a SQL table and column
	class LiteralBinding : DataBinding {
		public bool is_fts_match;
		public string literal;
		public LiteralType literal_type;
	}

	// Represents a mapping of a SPARQL variable to a SQL table and column
	class VariableBinding : DataBinding {
		public string variable;
		// Specified whether SQL column may contain NULL entries
		public bool maybe_null;
		public string type;
	}

	class VariableBindingList : Object {
		public List<VariableBinding> list;
	}

	// Represents a variable used as a predicate
	class PredicateVariable : Object {
		public string? subject;
		public string? object;

		public Class? domain;

		public string get_sql_query (SparqlQuery query) throws Error {
			var sql = new StringBuilder ();

			if (subject != null) {
				// single subject
				var subject_id = Data.query_resource_id (subject);

				DBResultSet result_set = null;
				if (subject_id > 0) {
					var iface = DBManager.get_db_interface ();
					var stmt = iface.create_statement ("SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
					stmt.bind_int (0, subject_id);
					result_set = stmt.execute ();
				}

				if (result_set != null) {
					bool first = true;
					do {
						Value value;
						result_set._get_value (0, out value);
						var domain = Ontology.get_class_by_uri (value.get_string ());

						foreach (Property prop in Ontology.get_properties ()) {
							if (prop.domain == domain) {
								if (first) {
									first = false;
								} else {
									sql.append (" UNION ALL ");
								}
								sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

								if (prop.data_type == PropertyType.RESOURCE) {
									sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
								} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
									sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
								} else if (prop.data_type == PropertyType.BOOLEAN) {
									sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
								} else if (prop.data_type == PropertyType.DATETIME) {
									sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%SZ\", \"%s\", \"unixepoch\")", prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.name);
								}

								sql.append (" AS \"object\" FROM ");
								if (prop.multiple_values) {
									sql.append_printf ("\"%s_%s\"", prop.domain.name, prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.domain.name);
								}

								sql.append (" WHERE ID = ?");

								var binding = new LiteralBinding ();
								binding.literal = subject_id.to_string ();
								binding.literal_type = LiteralType.INTEGER;
								query.bindings.append (binding);
							}
						}
					} while (result_set.iter_next ());
				} else {
					/* no match */
					sql.append ("SELECT NULL AS ID, NULL AS \"predicate\", NULL AS \"object\"");
				}
			} else if (object != null) {
				// single object
				var object_id = Data.query_resource_id (object);

				var iface = DBManager.get_db_interface ();
				var stmt = iface.create_statement ("SELECT (SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
				stmt.bind_int (0, object_id);
				var result_set = stmt.execute ();

				bool first = true;
				if (result_set != null) {
					do {
						Value value;
						result_set._get_value (0, out value);
						var range = Ontology.get_class_by_uri (value.get_string ());

						foreach (Property prop in Ontology.get_properties ()) {
							if (prop.range == range) {
								if (first) {
									first = false;
								} else {
									sql.append (" UNION ALL ");
								}
								sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

								if (prop.data_type == PropertyType.RESOURCE) {
									sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
								} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
									sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
								} else if (prop.data_type == PropertyType.BOOLEAN) {
									sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
								} else if (prop.data_type == PropertyType.DATETIME) {
									sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%SZ\", \"%s\", \"unixepoch\")", prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.name);
								}

								sql.append (" AS \"object\" FROM ");
								if (prop.multiple_values) {
									sql.append_printf ("\"%s_%s\"", prop.domain.name, prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.domain.name);
								}
							}
						}
					} while (result_set.iter_next ());
				} else {
					/* no match */
					sql.append ("SELECT NULL AS ID, NULL AS \"predicate\", NULL AS \"object\"");
				}
			} else if (domain != null) {
				// any subject, predicates limited to a specific domain
				bool first = true;
				foreach (Property prop in Ontology.get_properties ()) {
					if (prop.domain == domain) {
						if (first) {
							first = false;
						} else {
							sql.append (" UNION ALL ");
						}
						sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

						if (prop.data_type == PropertyType.RESOURCE) {
							sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
						} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
							sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
						} else if (prop.data_type == PropertyType.BOOLEAN) {
							sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
						} else if (prop.data_type == PropertyType.DATETIME) {
							sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%SZ\", \"%s\", \"unixepoch\")", prop.name);
						} else {
							sql.append_printf ("\"%s\"", prop.name);
						}

						sql.append (" AS \"object\" FROM ");
						if (prop.multiple_values) {
							sql.append_printf ("\"%s_%s\"", prop.domain.name, prop.name);
						} else {
							sql.append_printf ("\"%s\"", prop.domain.name);
						}
					}
				}
			} else {
				// UNION over all properties would exceed SQLite limits
				throw new SparqlError.INTERNAL ("Unrestricted predicate variables not supported");
			}
			return sql.str;
		}
	}

	SparqlScanner scanner;

	// token buffer
	TokenInfo[] tokens;
	// index of current token in buffer
	int index;
	// number of tokens in buffer
	int size;

	const int BUFFER_SIZE = 32;

	struct TokenInfo {
		public SparqlTokenType type;
		public SourceLocation begin;
		public SourceLocation end;
	}

	string query_string;
	bool update_extensions;

	string current_subject;
	bool current_subject_is_var;
	string current_predicate;
	bool current_predicate_is_var;

	HashTable<string,string> prefix_map;

	StringBuilder pattern_sql;

	// All SQL tables
	List<DataTable> tables;
	HashTable<string,DataTable> table_map;

	// All SPARQL literals
	List<LiteralBinding> bindings;
	List<LiteralBinding> pattern_bindings;

	// All SPARQL variables
	HashTable<string,VariableBinding> var_map;
	List<string> pattern_variables;
	HashTable<string,VariableBindingList> pattern_var_map;

	// Variables used as predicates
	HashTable<string,PredicateVariable> predicate_variable_map;

	bool delete_statements;

	int counter;

	int bnodeid = 0;
	// base UUID used for blank nodes
	uchar[] base_uuid;

	string error_message;

	public SparqlQuery (string query) {
		tokens = new TokenInfo[BUFFER_SIZE];
		prefix_map = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);

		this.query_string = query;
	}

	public SparqlQuery.update (string query) {
		this (query);
		this.update_extensions = true;
	}

#if 0
	string get_sql_for_literal (Rasqal.Literal literal) {
		assert (literal.type == Rasqal.Literal.Type.VARIABLE);

		string variable_name = literal.as_variable ().name;
		return "\"%s_u\"".printf (variable_name);
	}

	string get_sql_for_expression (Rasqal.Expression expr) {
		if (expr.op == Rasqal.Op.COUNT) {
			return "COUNT(%s)".printf (get_sql_for_expression (expr.arg1));
		} else if (expr.op == Rasqal.Op.SUM) {
			return "SUM(%s)".printf (get_sql_for_expression (expr.arg1));
		} else if (expr.op == Rasqal.Op.AVG) {
			return "AVG(%s)".printf (get_sql_for_expression (expr.arg1));
		} else if (expr.op == Rasqal.Op.MIN) {
			return "MIN(%s)".printf (get_sql_for_expression (expr.arg1));
		} else if (expr.op == Rasqal.Op.MAX) {
			return "MAX(%s)".printf (get_sql_for_expression (expr.arg1));
		} else if (expr.op == Rasqal.Op.VARSTAR) {
			return "*";
		} else if (expr.op == Rasqal.Op.LITERAL) {
			return get_sql_for_literal (expr.literal);
		}
		return "NULL";
	}
#endif

	string generate_bnodeid (string? user_bnodeid) {
		// user_bnodeid is NULL for anonymous nodes
		if (user_bnodeid == null) {
			return ":%d".printf (++bnodeid);
		} else {
			var checksum = new Checksum (ChecksumType.SHA1);
			// base UUID, unique per file
			checksum.update (base_uuid, 16);
			// node ID
			checksum.update ((uchar[]) user_bnodeid, -1);

			string sha1 = checksum.get_string ();

			// generate name based uuid
			return "urn:uuid:%.8s-%.4s-%.4s-%.4s-%.12s".printf (
				sha1, sha1.offset (8), sha1.offset (12), sha1.offset (16), sha1.offset (20));
		}
	}

#if 0
	void error_handler (Raptor.Locator? locator, string message) {
		if (error_message == null) {
			// return first, not last, error message
			error_message = message;
		}
	}
#endif

	inline bool next () {
		index = (index + 1) % BUFFER_SIZE;
		size--;
		if (size <= 0) {
			SourceLocation begin, end;
			SparqlTokenType type = scanner.read_token (out begin, out end);
			tokens[index].type = type;
			tokens[index].begin = begin;
			tokens[index].end = end;
			size = 1;
		}
		return (tokens[index].type != SparqlTokenType.EOF);
	}

	inline SparqlTokenType current () {
		return tokens[index].type;
	}

	inline SparqlTokenType last () {
		int last_index = (index + BUFFER_SIZE - 1) % BUFFER_SIZE;
		return tokens[last_index].type;
	}

	inline bool accept (SparqlTokenType type) {
		if (current () == type) {
			next ();
			return true;
		}
		return false;
	}

	bool expect (SparqlTokenType type) throws SparqlError {
		if (accept (type)) {
			return true;
		}

		throw new SparqlError.PARSE ("expected %s", type.to_string ());
	}

	inline SourceLocation get_location () {
		return tokens[index].begin;
	}

	void set_location (SourceLocation location) {
		scanner.seek (location);
		size = 0;
		index = 0;
		next ();
	}

	string get_current_string () {
		return ((string) tokens[index].begin.pos).ndup ((tokens[index].end.pos - tokens[index].begin.pos));
	}

	string get_last_string (int strip = 0) {
		int last_index = (index + BUFFER_SIZE - 1) % BUFFER_SIZE;
		return ((string) (tokens[last_index].begin.pos + strip)).ndup ((tokens[last_index].end.pos - tokens[last_index].begin.pos - 2 * strip));
	}

	void parse_prologue () throws SparqlError {
		if (accept (SparqlTokenType.BASE)) {
			expect (SparqlTokenType.IRI_REF);
		}
		while (accept (SparqlTokenType.PREFIX)) {
			string ns = "";
			if (accept (SparqlTokenType.PN_PREFIX)) {
				ns = get_last_string ();
			}
			expect (SparqlTokenType.COLON);
			expect (SparqlTokenType.IRI_REF);
			string uri = get_last_string (1);
			prefix_map.insert (ns, uri);
		}
	}

	public DBResultSet? execute () throws Error {
		scanner = new SparqlScanner ((char*) query_string, (long) query_string.size ());
		next ();

		foreach (Namespace ns in Ontology.get_namespaces ()) {
			prefix_map.insert (ns.prefix, ns.uri);
		}

		parse_prologue ();

		if (!update_extensions) {
			if (current () == SparqlTokenType.SELECT) {
				return execute_select ();
			} else if (current () == SparqlTokenType.CONSTRUCT) {
				throw new SparqlError.INTERNAL ("CONSTRUCT is not supported");
			} else if (current () == SparqlTokenType.DESCRIBE) {
				throw new SparqlError.INTERNAL ("DESCRIBE is not supported");
			} else if (current () == SparqlTokenType.ASK) {
				return execute_ask ();
			} else {
				throw new SparqlError.PARSE ("DELETE and INSERT are not supported in query mode");
			}
		} else {
			// SPARQL update supports multiple operations in a single query

			// all updates should be committed in one transaction
			Data.begin_transaction ();

			try {
				while (current () != SparqlTokenType.EOF) {
					if (current () == SparqlTokenType.INSERT) {
						execute_insert ();
					} else if (current () == SparqlTokenType.DELETE) {
						execute_delete ();
					} else if (current () == SparqlTokenType.DROP) {
						execute_drop_graph ();
					} else {
						throw new SparqlError.PARSE ("SELECT, CONSTRUCT, DESCRIBE, and ASK are not supported in update mode");
					}
				}
			} finally {
				Data.commit_transaction ();
			}

			return null;
		}
	}

	string get_sql_for_variable (string variable_name) {
		var binding = var_map.lookup (variable_name);
		assert (binding != null);
		if (binding.is_uri) {
			return "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s_u\")".printf (variable_name);
		} else if (binding.is_boolean) {
			return "(CASE \"%s_u\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END)".printf (variable_name);
		} else if (binding.is_datetime) {
			return "strftime (\"%%Y-%%m-%%dT%%H:%%M:%%SZ\", \"%s_u\", \"unixepoch\")".printf (variable_name);
		} else {
			return "\"%s_u\"".printf (variable_name);
		}
	}

	DBResultSet? exec_sql (string sql) throws Error {
		var iface = DBManager.get_db_interface ();
		var stmt = iface.create_statement ("%s", sql);

		// set literals specified in query
		int i = 0;
		foreach (LiteralBinding binding in bindings) {
			if (binding.is_boolean) {
				if (binding.literal == "true" || binding.literal == "1") {
					stmt.bind_int (i, 1);
				} else if (binding.literal == "false" || binding.literal == "0") {
					stmt.bind_int (i, 0);
				} else {
					throw new SparqlError.TYPE ("`%s' is not a valid boolean".printf (binding.literal));
				}
			} else if (binding.is_datetime) {
				stmt.bind_int (i, string_to_date (binding.literal));
			} else if (binding.literal_type == LiteralType.INTEGER) {
				stmt.bind_int (i, binding.literal.to_int ());
			} else {
				stmt.bind_text (i, binding.literal);
			}
			i++;
		}

		return stmt.execute ();
	}

	void skip_select_variables () {
		while (true) {
			switch (current ()) {
			case SparqlTokenType.FROM:
			case SparqlTokenType.WHERE:
			case SparqlTokenType.OPEN_BRACE:
			case SparqlTokenType.GROUP:
			case SparqlTokenType.ORDER:
			case SparqlTokenType.LIMIT:
			case SparqlTokenType.OFFSET:
			case SparqlTokenType.EOF:
				break;
			default:
				next ();
				continue;
			}
			break;
		}
	}

	DBResultSet? execute_select () throws Error {
		// SELECT query

		pattern_sql = new StringBuilder ();
		var_map = new HashTable<string,VariableBinding>.full (str_hash, str_equal, g_free, g_object_unref);
		predicate_variable_map = new HashTable<string,PredicateVariable>.full (str_hash, str_equal, g_free, g_object_unref);

		// build SQL
		var sql = new StringBuilder ();
		sql.append ("SELECT ");

		expect (SparqlTokenType.SELECT);

		if (accept (SparqlTokenType.DISTINCT)) {
			sql.append ("DISTINCT ");
		} else if (accept (SparqlTokenType.REDUCED)) {
		}

		// skip select variables (processed later)
		var select_variables_location = get_location ();
		skip_select_variables ();

		if (accept (SparqlTokenType.FROM)) {
			accept (SparqlTokenType.NAMED);
			expect (SparqlTokenType.IRI_REF);
		}

		accept (SparqlTokenType.WHERE);

		translate_group_graph_pattern (pattern_sql);
		string pattern_sql_string = pattern_sql.str;
		pattern_sql.truncate (0);

		// process select variables
		var after_where = get_location ();
		set_location (select_variables_location);

		bool first = true;
		if (accept (SparqlTokenType.STAR)) {
			foreach (string variable_name in var_map.get_keys ()) {
				if (!first) {
					pattern_sql.append (", ");
				} else {
					first = false;
				}
				pattern_sql.append (get_sql_for_variable (variable_name));
			}
		} else {
			while (true) {
				if (!first) {
					pattern_sql.append (", ");
				} else {
					first = false;
				}

				parse_primary_expression_as_string ();

				switch (current ()) {
				case SparqlTokenType.FROM:
				case SparqlTokenType.WHERE:
				case SparqlTokenType.OPEN_BRACE:
				case SparqlTokenType.GROUP:
				case SparqlTokenType.ORDER:
				case SparqlTokenType.LIMIT:
				case SparqlTokenType.OFFSET:
				case SparqlTokenType.EOF:
					break;
				default:
					continue;
				}
				break;
			}
		}
		sql.append (pattern_sql.str);
		pattern_sql.truncate (0);

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql_string);
		sql.append (")");

		set_location (after_where);

		if (accept (SparqlTokenType.GROUP)) {
			expect (SparqlTokenType.BY);
			sql.append (" GROUP BY ");
			bool first_group = true;
			do {
				if (first_group) {
					first_group = false;
				} else {
					sql.append (", ");
				}
				if (accept (SparqlTokenType.ASC)) {
					parse_bracketted_expression_as_string ();
					sql.append (pattern_sql.str);
					sql.append (" ASC");
				} else if (accept (SparqlTokenType.DESC)) {
					parse_bracketted_expression_as_string ();
					sql.append (pattern_sql.str);
					sql.append (" DESC");
				} else {
					parse_primary_expression_as_string ();
					sql.append (pattern_sql.str);
				}
				pattern_sql.truncate (0);
			} while (current () != SparqlTokenType.ORDER && current () != SparqlTokenType.LIMIT && current () != SparqlTokenType.OFFSET && current () != SparqlTokenType.EOF);
		}

		if (accept (SparqlTokenType.ORDER)) {
			expect (SparqlTokenType.BY);
			sql.append (" ORDER BY ");
			bool first_order = true;
			do {
				if (first_order) {
					first_order = false;
				} else {
					sql.append (", ");
				}
				if (accept (SparqlTokenType.ASC)) {
					parse_bracketted_expression_as_string ();
					sql.append (pattern_sql.str);
					sql.append (" ASC");
				} else if (accept (SparqlTokenType.DESC)) {
					parse_bracketted_expression_as_string ();
					sql.append (pattern_sql.str);
					sql.append (" DESC");
				} else {
					parse_primary_expression_as_string ();
					sql.append (pattern_sql.str);
				}
				pattern_sql.truncate (0);
			} while (current () != SparqlTokenType.LIMIT && current () != SparqlTokenType.OFFSET && current () != SparqlTokenType.EOF);
		}

		int limit = -1;
		int offset = -1;

		if (accept (SparqlTokenType.LIMIT)) {
			expect (SparqlTokenType.INTEGER);
			limit = get_last_string ().to_int ();
			if (accept (SparqlTokenType.OFFSET)) {
				expect (SparqlTokenType.INTEGER);
				offset = get_last_string ().to_int ();
			}
		} else if (accept (SparqlTokenType.OFFSET)) {
			expect (SparqlTokenType.INTEGER);
			offset = get_last_string ().to_int ();
			if (accept (SparqlTokenType.LIMIT)) {
				expect (SparqlTokenType.INTEGER);
				limit = get_last_string ().to_int ();
			}
		}

		// LIMIT and OFFSET
		if (limit >= 0) {
			sql.append (" LIMIT ?");

			var binding = new LiteralBinding ();
			binding.literal = limit.to_string ();
			binding.literal_type = LiteralType.INTEGER;
			bindings.append (binding);

			if (offset >= 0) {
				sql.append (" OFFSET ?");

				binding = new LiteralBinding ();
				binding.literal = offset.to_string ();
				binding.literal_type = LiteralType.INTEGER;
				bindings.append (binding);
			}
		} else if (offset >= 0) {
			sql.append (" LIMIT -1 OFFSET ?");

			var binding = new LiteralBinding ();
			binding.literal = offset.to_string ();
			binding.literal_type = LiteralType.INTEGER;
			bindings.append (binding);
		}

		return exec_sql (sql.str);
	}

	DBResultSet? execute_ask () throws Error {
		// ASK query

		pattern_sql = new StringBuilder ();
		var_map = new HashTable<string,VariableBinding>.full (str_hash, str_equal, g_free, g_object_unref);
		predicate_variable_map = new HashTable<string,PredicateVariable>.full (str_hash, str_equal, g_free, g_object_unref);

		// build SQL
		var sql = new StringBuilder ();
		sql.append ("SELECT ");

		expect (SparqlTokenType.ASK);

		sql.append ("COUNT(1) > 0");

		accept (SparqlTokenType.WHERE);

		translate_group_graph_pattern (pattern_sql);

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql.str);
		sql.append (")");

		pattern_sql.truncate (0);

		return exec_sql (sql.str);
	}

	void execute_insert () throws Error {
		expect (SparqlTokenType.INSERT);
		execute_update (false);
	}

	void execute_delete () throws Error {
		expect (SparqlTokenType.DELETE);
		execute_update (true);
	}

	void execute_update (bool delete_statements) throws Error {
		// INSERT or DELETE

		pattern_sql = new StringBuilder ();
		var_map = new HashTable<string,VariableBinding>.full (str_hash, str_equal, g_free, g_object_unref);
		predicate_variable_map = new HashTable<string,PredicateVariable>.full (str_hash, str_equal, g_free, g_object_unref);

		var sql = new StringBuilder ();

		var template_location = get_location ();
		skip_braces ();

		if (accept (SparqlTokenType.WHERE)) {
			translate_group_graph_pattern (pattern_sql);
		}

		// build SQL
		sql.append ("SELECT ");
		bool first = true;
		foreach (VariableBinding binding in var_map.get_values ()) {
			if (!first) {
				sql.append (", ");
			} else {
				first = false;
			}

			sql.append (get_sql_for_variable (binding.variable));
		}

		if (first) {
			sql.append ("1");
		} else {
			// select from results of WHERE clause
			sql.append (" FROM (");
			sql.append (pattern_sql.str);
			sql.append (")");
		}

		var result_set = exec_sql (sql.str);

		this.delete_statements = delete_statements;

		// iterate over all solutions
		if (result_set != null) {
			do {
				// get values of all variables to be bound
				var var_value_map = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);
				int var_idx = 0;
				foreach (string var_name in var_map.get_keys ()) {
					Value value;
					result_set._get_value (var_idx++, out value);
					var_value_map.insert (var_name, get_string_for_value (value));
				}

				set_location (template_location);

				// iterate over each triple in the template
				parse_construct_triples_block (var_value_map);
			} while (result_set.iter_next ());
		}
	}

	void execute_drop_graph () throws Error {
		expect (SparqlTokenType.DROP);
		expect (SparqlTokenType.GRAPH);

		bool is_var;
		string uri = parse_var_or_term (out is_var);

		Data.delete_resource_description (uri);
	}

	string parse_var_or_term (out bool is_var) throws SparqlError {
		string result = "";
		is_var = false;
		if (current () == SparqlTokenType.VAR) {
			is_var = true;
			next ();
			result = get_last_string ().substring (1);
		} else if (current () == SparqlTokenType.IRI_REF) {
			next ();
			result = get_last_string (1);
		} else if (current () == SparqlTokenType.PN_PREFIX) {
			// prefixed name with namespace foo:bar
			next ();
			string ns = get_last_string ();
			expect (SparqlTokenType.COLON);
			result = prefix_map.lookup (ns) + get_last_string ().substring (1);
		} else if (current () == SparqlTokenType.COLON) {
			// prefixed name without namespace :bar
			next ();
			result = prefix_map.lookup ("") + get_last_string ().substring (1);
		} else if (current () == SparqlTokenType.STRING_LITERAL1) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL2) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG1) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG2) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.INTEGER) {
			next ();
		} else if (current () == SparqlTokenType.DECIMAL) {
			next ();
		} else if (current () == SparqlTokenType.DOUBLE) {
			next ();
		} else if (current () == SparqlTokenType.TRUE) {
			next ();
			result = "true";
		} else if (current () == SparqlTokenType.FALSE) {
			next ();
			result = "false";
		} else if (current () == SparqlTokenType.OPEN_BRACKET) {
			next ();

			result = generate_bnodeid (null);

			string old_subject = current_subject;
			bool old_subject_is_var = current_subject_is_var;

			current_subject = result;
			current_subject_is_var = true;
			parse_property_list_not_empty ();
			expect (SparqlTokenType.CLOSE_BRACKET);

			current_subject = old_subject;
			current_subject_is_var = old_subject_is_var;

			is_var = true;
		} else {
			// TODO error
		}
		return result;
	}

	void parse_object_list () throws SparqlError {
		while (true) {
			parse_object ();
			if (accept (SparqlTokenType.COMMA)) {
				continue;
			}
			break;
		}
	}

	void parse_property_list_not_empty () throws SparqlError {
		while (true) {
			var old_predicate = current_predicate;
			var old_predicate_is_var = current_predicate_is_var;

			current_predicate = null;
			current_predicate_is_var = false;
			if (current () == SparqlTokenType.VAR) {
				current_predicate_is_var = true;
				next ();
				current_predicate = get_last_string ().substring (1);
			} else if (current () == SparqlTokenType.IRI_REF) {
				next ();
				current_predicate = get_last_string (1);
			} else if (current () == SparqlTokenType.PN_PREFIX) {
				next ();
				string ns = get_last_string ();
				expect (SparqlTokenType.COLON);
				current_predicate = prefix_map.lookup (ns) + get_last_string ().substring (1);
			} else if (current () == SparqlTokenType.COLON) {
				next ();
				current_predicate = prefix_map.lookup ("") + get_last_string ().substring (1);
			} else if (current () == SparqlTokenType.A) {
				next ();
				current_predicate = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
			} else {
				// TODO error
			}
			parse_object_list ();

			current_predicate = old_predicate;
			current_predicate_is_var = old_predicate_is_var;

			if (accept (SparqlTokenType.SEMICOLON)) {
				continue;
			}
			break;
		}
	}


	void translate_graph_pattern_not_triples (StringBuilder sql, int group_graph_pattern_start) throws SparqlError {
		if (current () == SparqlTokenType.OPTIONAL) {
			translate_optional_graph_pattern (sql, group_graph_pattern_start);
		} else if (current () == SparqlTokenType.OPEN_BRACE) {
			translate_group_or_union_graph_pattern (sql, group_graph_pattern_start);
		} else if (current () == SparqlTokenType.GRAPH) {
			// translate_graph_graph_pattern ();
		}
	}

	void parse_bound_call () throws SparqlError {
		expect (SparqlTokenType.BOUND);
		expect (SparqlTokenType.OPEN_PARENS);
		pattern_sql.append ("(");
		parse_expression ();
		pattern_sql.append (") IS NOT NULL");
		expect (SparqlTokenType.CLOSE_PARENS);
	}

	void parse_regex () throws SparqlError {
		expect (SparqlTokenType.REGEX);
		expect (SparqlTokenType.OPEN_PARENS);
		pattern_sql.append ("SparqlRegex(");
		parse_expression ();
		pattern_sql.append (", ");
		expect (SparqlTokenType.COMMA);
		parse_expression ();
		pattern_sql.append (", ");
		if (accept (SparqlTokenType.COMMA)) {
			parse_expression ();
		} else {
			pattern_sql.append ("''");
		}
		pattern_sql.append (")");
		expect (SparqlTokenType.CLOSE_PARENS);
	}

	string parse_string_literal () throws SparqlError {
		next ();
		switch (last ()) {
		case SparqlTokenType.STRING_LITERAL1:
		case SparqlTokenType.STRING_LITERAL2:
			var sb = new StringBuilder ();

			string s = get_last_string (1);
			string* p = s;
			string* end = p + s.size ();
			while ((long) p < (long) end) {
				string* q = Posix.strchr (p, '\\');
				if (q == null) {
					sb.append_len (p, (long) (end - p));
					p = end;
				} else {
					sb.append_len (p, (long) (q - p));
					p = q + 1;
					switch (((char*) p)[0]) {
					case '\'':
					case '"':
					case '\\':
						sb.append_c (((char*) p)[0]);
						break;
					case 'b':
						sb.append_c ('\b');
						break;
					case 'f':
						sb.append_c ('\f');
						break;
					case 'n':
						sb.append_c ('\n');
						break;
					case 'r':
						sb.append_c ('\r');
						break;
					case 't':
						sb.append_c ('\t');
						break;
					}
					p++;
				}
			}
			return sb.str;
		case SparqlTokenType.STRING_LITERAL_LONG1:
		case SparqlTokenType.STRING_LITERAL_LONG2:
			return get_last_string (3);
		default:
			assert_not_reached ();
		}
	}

	void parse_primary_expression_as_string () throws SparqlError {
		if (current () == SparqlTokenType.VAR) {
			next ();
			pattern_sql.append (get_sql_for_variable (get_last_string ().substring (1)));
		} else {
			parse_primary_expression ();
		}
	}

	void parse_primary_expression () throws SparqlError {
		switch (current ()) {
		case SparqlTokenType.OPEN_PARENS:
			parse_bracketted_expression ();
			break;
		case SparqlTokenType.IRI_REF:
			next ();

			pattern_sql.append ("(SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");

			var binding = new LiteralBinding ();
			binding.literal = get_last_string (1);
			bindings.append (binding);

			break;
		case SparqlTokenType.DECIMAL:
		case SparqlTokenType.DOUBLE:
			next ();

			pattern_sql.append ("?");

			var binding = new LiteralBinding ();
			binding.literal = get_last_string ();
			bindings.append (binding);

			break;
		case SparqlTokenType.TRUE:
			next ();

			pattern_sql.append ("?");

			var binding = new LiteralBinding ();
			binding.literal = "1";
			binding.literal_type = LiteralType.INTEGER;
			bindings.append (binding);

			break;
		case SparqlTokenType.FALSE:
			next ();

			pattern_sql.append ("?");

			var binding = new LiteralBinding ();
			binding.literal = "0";
			binding.literal_type = LiteralType.INTEGER;
			bindings.append (binding);

			break;
		case SparqlTokenType.STRING_LITERAL1:
		case SparqlTokenType.STRING_LITERAL2:
		case SparqlTokenType.STRING_LITERAL_LONG1:
		case SparqlTokenType.STRING_LITERAL_LONG2:
			pattern_sql.append ("?");

			var binding = new LiteralBinding ();
			binding.literal = parse_string_literal ();
			bindings.append (binding);

			break;
		case SparqlTokenType.INTEGER:
			next ();

			pattern_sql.append ("?");

			var binding = new LiteralBinding ();
			binding.literal = get_last_string ();
			binding.literal_type = LiteralType.INTEGER;
			bindings.append (binding);

			break;
		case SparqlTokenType.VAR:
			next ();
			string variable_name = get_last_string ().substring (1);
			pattern_sql.append_printf ("\"%s_u\"", variable_name);
			break;
		case SparqlTokenType.STR:
		case SparqlTokenType.LANG:
		case SparqlTokenType.LANGMATCHES:
		case SparqlTokenType.DATATYPE:
			next ();
			break;
		case SparqlTokenType.BOUND:
			parse_bound_call ();
			break;
		case SparqlTokenType.SAMETERM:
			next ();
			expect (SparqlTokenType.OPEN_PARENS);
			pattern_sql.append ("(");
			parse_expression ();
			pattern_sql.append (" = ");
			expect (SparqlTokenType.COMMA);
			parse_expression ();
			pattern_sql.append (")");
			expect (SparqlTokenType.CLOSE_PARENS);
			break;
		case SparqlTokenType.ISIRI:
		case SparqlTokenType.ISURI:
		// case SparqlTokenType.ISBLANK:
		case SparqlTokenType.ISLITERAL:
			next ();
			break;
		case SparqlTokenType.REGEX:
			parse_regex ();
			break;
		}
	}

	void parse_unary_expression () throws SparqlError {
		if (accept (SparqlTokenType.OP_NEG)) {
			pattern_sql.append ("NOT (");
			parse_primary_expression ();
			pattern_sql.append (")");
			return;
		}
		parse_primary_expression ();
	}

	void parse_multiplicative_expression () throws SparqlError {
		long begin = pattern_sql.len;
		parse_unary_expression ();
		while (true) {
			if (accept (SparqlTokenType.STAR)) {
				pattern_sql.insert (begin, "(");
				pattern_sql.append (" * ");
				parse_unary_expression ();
				pattern_sql.append (")");
			} else if (accept (SparqlTokenType.DIV)) {
				pattern_sql.insert (begin, "(");
				pattern_sql.append (" / ");
				parse_unary_expression ();
				pattern_sql.append (")");
			} else {
				break;
			}
		}
	}

	void parse_additive_expression () throws SparqlError {
		long begin = pattern_sql.len;
		parse_multiplicative_expression ();
		while (true) {
			if (accept (SparqlTokenType.PLUS)) {
				pattern_sql.insert (begin, "(");
				pattern_sql.append (" + ");
				parse_multiplicative_expression ();
				pattern_sql.append (")");
			} else if (accept (SparqlTokenType.MINUS)) {
				pattern_sql.insert (begin, "(");
				pattern_sql.append (" - ");
				parse_multiplicative_expression ();
				pattern_sql.append (")");
			} else {
				break;
			}
		}
	}

	void parse_numeric_expression () throws SparqlError {
		parse_additive_expression ();
	}

	void parse_relational_expression () throws SparqlError {
		long begin = pattern_sql.len;
		parse_numeric_expression ();
		if (accept (SparqlTokenType.OP_GE)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" >= ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		} else if (accept (SparqlTokenType.OP_EQ)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" = ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		} else if (accept (SparqlTokenType.OP_NE)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" <> ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		} else if (accept (SparqlTokenType.OP_LT)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" < ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		} else if (accept (SparqlTokenType.OP_LE)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" <= ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		} else if (accept (SparqlTokenType.OP_GT)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" > ");
			parse_numeric_expression ();
			pattern_sql.append (")");
		}
	}

	void parse_value_logical () throws SparqlError {
		parse_relational_expression ();
	}

	void parse_conditional_and_expression () throws SparqlError {
		long begin = pattern_sql.len;
		parse_value_logical ();
		while (accept (SparqlTokenType.OP_AND)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" && ");
			parse_value_logical ();
			pattern_sql.append (")");
		}
	}

	void parse_conditional_or_expression () throws SparqlError {
		long begin = pattern_sql.len;
		parse_conditional_and_expression ();
		while (accept (SparqlTokenType.OP_OR)) {
			pattern_sql.insert (begin, "(");
			pattern_sql.append (" || ");
			parse_conditional_and_expression ();
			pattern_sql.append (")");
		}
	}

	void parse_expression () throws SparqlError {
		parse_conditional_or_expression ();
	}

	void parse_bracketted_expression_as_string () throws SparqlError {
		expect (SparqlTokenType.OPEN_PARENS);
		if (current () == SparqlTokenType.VAR) {
			next ();
			pattern_sql.append (get_sql_for_variable (get_last_string ().substring (1)));
		} else {
			parse_expression ();
		}
		expect (SparqlTokenType.CLOSE_PARENS);
	}

	void parse_bracketted_expression () throws SparqlError {
		expect (SparqlTokenType.OPEN_PARENS);
		parse_expression ();
		expect (SparqlTokenType.CLOSE_PARENS);
	}

	void parse_constraint () throws SparqlError {
		switch (current ()) {
		case SparqlTokenType.STR:
		case SparqlTokenType.LANG:
		case SparqlTokenType.LANGMATCHES:
		case SparqlTokenType.DATATYPE:
		case SparqlTokenType.BOUND:
		case SparqlTokenType.SAMETERM:
		case SparqlTokenType.ISIRI:
		case SparqlTokenType.ISURI:
		// case SparqlTokenType.ISBLANK:
		case SparqlTokenType.ISLITERAL:
		case SparqlTokenType.REGEX:
			parse_primary_expression ();
			return;
		}
		parse_bracketted_expression ();
	}

	void parse_filter () throws SparqlError {
		expect (SparqlTokenType.FILTER);
		parse_constraint ();
	}

	void skip_filter () throws SparqlError {
		expect (SparqlTokenType.FILTER);

		switch (current ()) {
		case SparqlTokenType.STR:
		case SparqlTokenType.LANG:
		case SparqlTokenType.LANGMATCHES:
		case SparqlTokenType.DATATYPE:
		case SparqlTokenType.BOUND:
		case SparqlTokenType.SAMETERM:
		case SparqlTokenType.ISIRI:
		case SparqlTokenType.ISURI:
		// case SparqlTokenType.ISBLANK:
		case SparqlTokenType.ISLITERAL:
		case SparqlTokenType.REGEX:
			next ();
			break;
		}

		expect (SparqlTokenType.OPEN_PARENS);
		int n_parens = 1;
		while (n_parens > 0) {
			if (accept (SparqlTokenType.OPEN_PARENS)) {
				n_parens++;
			} else if (accept (SparqlTokenType.CLOSE_PARENS)) {
				n_parens--;
			} else if (current () == SparqlTokenType.EOF) {
				throw new SparqlError.PARSE ("unexpected end of query, expected )");
			} else {
				// ignore everything else
				next ();
			}
		}
	}

	void skip_braces () throws SparqlError {
		expect (SparqlTokenType.OPEN_BRACE);
		int n_braces = 1;
		while (n_braces > 0) {
			if (accept (SparqlTokenType.OPEN_BRACE)) {
				n_braces++;
			} else if (accept (SparqlTokenType.CLOSE_BRACE)) {
				n_braces--;
			} else if (current () == SparqlTokenType.EOF) {
				throw new SparqlError.PARSE ("unexpected end of query, expected }");
			} else {
				// ignore everything else
				next ();
			}
		}
	}

	void parse_triples_block () throws SparqlError {
		while (true) {
			current_subject = parse_var_or_term (out current_subject_is_var);
			parse_property_list_not_empty ();

			if (accept (SparqlTokenType.DOT)) {
				if (current () == SparqlTokenType.VAR ||
				    current () == SparqlTokenType.IRI_REF ||
				    current () == SparqlTokenType.PN_PREFIX ||
				    current () == SparqlTokenType.COLON ||
				    current () == SparqlTokenType.OPEN_BRACKET) {
					// optional TriplesBlock
					continue;
				}
			}
			break;
		}

		// remove last comma and space
		pattern_sql.truncate (pattern_sql.len - 2);
	}

	void parse_construct_triples_block (HashTable<string,string> var_value_map) throws SparqlError {
		expect (SparqlTokenType.OPEN_BRACE);

		do {
			current_subject = parse_construct_var_or_term (var_value_map);
			parse_construct_property_list_not_empty (var_value_map);
		} while (accept (SparqlTokenType.DOT) && current () != SparqlTokenType.CLOSE_BRACE);

		expect (SparqlTokenType.CLOSE_BRACE);
	}


	string parse_construct_var_or_term (HashTable<string,string> var_value_map) throws SparqlError {
		string result = "";
		if (current () == SparqlTokenType.VAR) {
			next ();
			result = var_value_map.lookup (get_last_string ().substring (1));
		} else if (current () == SparqlTokenType.IRI_REF) {
			next ();
			result = get_last_string (1);
		} else if (current () == SparqlTokenType.PN_PREFIX) {
			// prefixed name with namespace foo:bar
			next ();
			string ns = get_last_string ();
			expect (SparqlTokenType.COLON);
			result = prefix_map.lookup (ns) + get_last_string ().substring (1);
		} else if (current () == SparqlTokenType.COLON) {
			// prefixed name without namespace :bar
			next ();
			result = prefix_map.lookup ("") + get_last_string ().substring (1);
		} else if (current () == SparqlTokenType.INTEGER) {
			next ();
			result = get_last_string ();
		} else if (current () == SparqlTokenType.DECIMAL) {
			next ();
			result = get_last_string ();
		} else if (current () == SparqlTokenType.DOUBLE) {
			next ();
			result = get_last_string ();
		} else if (current () == SparqlTokenType.TRUE) {
			next ();
			result = "true";
		} else if (current () == SparqlTokenType.FALSE) {
			next ();
			result = "false";
		} else if (current () == SparqlTokenType.STRING_LITERAL1) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL2) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG1) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG2) {
			result = parse_string_literal ();
		} else if (current () == SparqlTokenType.OPEN_BRACKET) {
			next ();

			result = generate_bnodeid (null);

			string old_subject = current_subject;
			bool old_subject_is_var = current_subject_is_var;

			current_subject = result;
			parse_construct_property_list_not_empty (var_value_map);
			expect (SparqlTokenType.CLOSE_BRACKET);

			current_subject = old_subject;
			current_subject_is_var = old_subject_is_var;
		} else {
			// TODO error
		}
		return result;
	}

	void parse_construct_property_list_not_empty (HashTable<string,string> var_value_map) throws SparqlError {
		while (true) {
			var old_predicate = current_predicate;

			current_predicate = null;
			if (current () == SparqlTokenType.VAR) {
				current_predicate_is_var = true;
				next ();
				current_predicate = var_value_map.lookup (get_last_string ().substring (1));
			} else if (current () == SparqlTokenType.IRI_REF) {
				next ();
				current_predicate = get_last_string (1);
			} else if (current () == SparqlTokenType.PN_PREFIX) {
				next ();
				string ns = get_last_string ();
				expect (SparqlTokenType.COLON);
				current_predicate = prefix_map.lookup (ns) + get_last_string ().substring (1);
			} else if (current () == SparqlTokenType.COLON) {
				next ();
				current_predicate = prefix_map.lookup ("") + get_last_string ().substring (1);
			} else if (current () == SparqlTokenType.A) {
				next ();
				current_predicate = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
			} else {
				// TODO error
			}
			parse_construct_object_list (var_value_map);

			current_predicate = old_predicate;

			if (accept (SparqlTokenType.SEMICOLON)) {
				continue;
			}
			break;
		}
	}

	void parse_construct_object_list (HashTable<string,string> var_value_map) throws SparqlError {
		while (true) {
			parse_construct_object (var_value_map);
			if (accept (SparqlTokenType.COMMA)) {
				continue;
			}
			break;
		}
	}

	void parse_construct_object (HashTable<string,string> var_value_map) throws SparqlError {
		string object = parse_construct_var_or_term (var_value_map);
		if (delete_statements) {
			// delete triple from database
			Data.delete_statement (current_subject, current_predicate, object);
		} else {
			// insert triple into database
			Data.insert_statement (current_subject, current_predicate, object);
		}
	}

	void translate_triples_block (StringBuilder sql, ref bool first_where) throws SparqlError {
		tables = new List<DataTable> ();
		table_map = new HashTable<string,DataTable>.full (str_hash, str_equal, g_free, g_object_unref);

		pattern_variables = new List<string> ();
		pattern_var_map = new HashTable<string,VariableBindingList>.full (str_hash, str_equal, g_free, g_object_unref);

		pattern_bindings = new List<LiteralBinding> ();

		sql.append ("SELECT ");

		parse_triples_block ();

		sql.append (" FROM ");
		bool first = true;
		foreach (DataTable table in tables) {
			if (!first) {
				sql.append (", ");
			} else {
				first = false;
			}
			if (table.sql_db_tablename != null) {
				sql.append_printf ("\"%s\"", table.sql_db_tablename);
			} else {
				sql.append_printf ("(%s)", table.predicate_variable.get_sql_query (this));
			}
			sql.append_printf (" AS \"%s\"", table.sql_query_tablename);
		}

		foreach (string variable in pattern_variables) {
			bool maybe_null = true;
			string last_name = null;
			foreach (VariableBinding binding in pattern_var_map.lookup (variable).list) {
				string name = "\"%s\".\"%s\"".printf (binding.table.sql_query_tablename, binding.sql_db_column_name);
				if (last_name != null) {
					if (!first_where) {
						sql.append (" AND ");
					} else {
						sql.append (" WHERE ");
						first_where = false;
					}
					sql.append (last_name);
					sql.append (" = ");
					sql.append (name);
				}
				last_name = name;
				if (!binding.maybe_null) {
					maybe_null = false;
				}
			}

			if (maybe_null) {
				// ensure that variable is bound in case it could return NULL in SQL
				// assuming SPARQL variable is not optional
				if (!first_where) {
					sql.append (" AND ");
				} else {
					sql.append (" WHERE ");
					first_where = false;
				}
				sql.append_printf ("\"%s_u\" IS NOT NULL", variable);
			}
		}
		foreach (LiteralBinding binding in pattern_bindings) {
			if (!first_where) {
				sql.append (" AND ");
			} else {
				sql.append (" WHERE ");
				first_where = false;
			}
			sql.append ("\"");
			sql.append (binding.table.sql_query_tablename);
			sql.append ("\".\"");
			sql.append (binding.sql_db_column_name);
			sql.append ("\"");
			if (binding.is_fts_match) {
				// parameters do not work with fts MATCH
				string escaped_literal = string.joinv ("''", binding.literal.split ("'"));
				sql.append_printf (" IN (SELECT rowid FROM fts WHERE fts MATCH '%s')", escaped_literal);
			} else {
				sql.append (" = ");
				if (binding.is_uri) {
					sql.append ("(SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
				} else {
					sql.append ("?");
				}
			}
		}

		tables = null;
		table_map = null;
		pattern_variables = null;
		pattern_var_map = null;
		pattern_bindings = null;
	}

	void translate_group_graph_pattern (StringBuilder sql) throws SparqlError {
		expect (SparqlTokenType.OPEN_BRACE);

		SourceLocation[] filters = { };

		bool first_where = true;
		int group_graph_pattern_start = (int) sql.len;

		// optional TriplesBlock
		if (current () == SparqlTokenType.VAR ||
		    current () == SparqlTokenType.IRI_REF ||
		    current () == SparqlTokenType.PN_PREFIX ||
		    current () == SparqlTokenType.COLON ||
		    current () == SparqlTokenType.OPEN_BRACKET) {
			translate_triples_block (sql, ref first_where);
		}

		while (true) {
			// check whether we have GraphPatternNotTriples | Filter
			if (current () == SparqlTokenType.OPTIONAL) {
				if (group_graph_pattern_start == (int) sql.len) {
					// empty graph pattern => return one result without bound variables
					sql.append ("SELECT 1");
				}
				translate_graph_pattern_not_triples (sql, group_graph_pattern_start);
			} else if (current () == SparqlTokenType.OPEN_BRACE ||
			           current () == SparqlTokenType.GRAPH) {
				translate_graph_pattern_not_triples (sql, group_graph_pattern_start);
			} else if (current () == SparqlTokenType.FILTER) {
				filters += get_location ();
				skip_filter ();
			} else {
				break;
			}

			accept (SparqlTokenType.DOT);

			// optional TriplesBlock
			if (current () == SparqlTokenType.VAR ||
			    current () == SparqlTokenType.IRI_REF ||
			    current () == SparqlTokenType.OPEN_BRACKET) {
				translate_triples_block (sql, ref first_where);
			}
		}

		expect (SparqlTokenType.CLOSE_BRACE);

		if (group_graph_pattern_start == (int) sql.len) {
			// empty graph pattern => return one result without bound variables
			sql.append ("SELECT 1");
		}

		// handle filters last, they apply to the pattern as a whole
		if (filters.length > 0) {
			var end = get_location ();

			foreach (var filter_location in filters) {
				if (!first_where) {
					sql.append (" AND ");
				} else {
					sql.append (" WHERE ");
					first_where = false;
				}

				set_location (filter_location);
				parse_filter ();
			}

			set_location (end);
		}
	}

	void translate_group_or_union_graph_pattern (StringBuilder sql, int group_graph_pattern_start) throws SparqlError {
		translate_group_graph_pattern (sql);
		while (accept (SparqlTokenType.UNION)) {
			sql.append (" UNION ALL ");
			translate_group_graph_pattern (sql);
		}
	}

	void translate_optional_graph_pattern (StringBuilder sql, int group_graph_pattern_start) throws SparqlError {
		expect (SparqlTokenType.OPTIONAL);
		sql.insert (group_graph_pattern_start, "SELECT * FROM (");
		sql.append (") NATURAL LEFT JOIN (SELECT * FROM (");
		translate_group_graph_pattern (sql);
		sql.append ("))");
#if 0
		if (graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.BASIC) {
		} else if (graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.GROUP
		           || graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.OPTIONAL) {
		} else if (graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.UNION) {
			for (int pattern_idx = 0; true; pattern_idx++) {
				weak Rasqal.GraphPattern sub_graph_pattern = graph_pattern.get_sub_graph_pattern (pattern_idx);
				if (sub_graph_pattern == null) {
					break;
				}

				if (sub_graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.FILTER) {
					// ignore filters, processed later
					continue;
				}

				if (pattern_idx > 0) {
					pattern_sql.append (" UNION ALL ");
				}
				visit_graph_pattern (sub_graph_pattern);
			}
		}
#endif
	}

#if 0
	string get_string_from_literal (Rasqal.Literal lit, bool is_subject = false) {
		if (lit.type == Rasqal.Literal.Type.BLANK) {
			if (!is_subject && lit.as_string ().has_prefix (":")) {
				// anonymous blank node, libtracker-data will
				// generate appropriate uri
				return lit.as_string ();
			} else {
				return generate_bnodeid (lit.as_string ());
			}
		} else {
			return lit.as_string ();
		}
	}
#endif

	void parse_object () throws SparqlError {
		bool object_is_var;
		string object = parse_var_or_term (out object_is_var);

		string db_table;
		bool rdftype = false;
		bool share_table = true;

		bool newtable;
		DataTable table;
		Property prop = null;

		if (!current_predicate_is_var) {
			prop = Ontology.get_property_by_uri (current_predicate);

			if (current_predicate == "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
			    && !object_is_var) {
				// rdf:type query
				rdftype = true;
				var cl = Ontology.get_class_by_uri (object);
				if (cl == null) {
					throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (object));
				}
				db_table = cl.name;
			} else if (prop == null) {
				if (current_predicate == "http://www.tracker-project.org/ontologies/fts#match") {
					// fts:match
					db_table = "rdfs:Resource";
				} else {
					throw new SparqlError.UNKNOWN_PROPERTY ("Unknown property `%s'".printf (current_predicate));
				}
			} else {
				if (current_predicate == "http://www.w3.org/2000/01/rdf-schema#domain"
				    && current_subject_is_var
				    && !object_is_var) {
					// rdfs:domain
					var domain = Ontology.get_class_by_uri (object);
					if (domain == null) {
						throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (object));
					}
					var pv = predicate_variable_map.lookup (current_subject);
					if (pv == null) {
						pv = new PredicateVariable ();
						predicate_variable_map.insert (current_subject, pv);
					}
					pv.domain = domain;
				}

				if (prop.multiple_values) {
					db_table = "%s_%s".printf (prop.domain.name, prop.name);
					// we can never share the table with multiple triples
					// for multi value properties as a property may consist of multiple rows
					share_table = false;
				} else {
					db_table = prop.domain.name;
				}
			}
			table = get_table (current_subject, db_table, share_table, out newtable);
		} else {
			// variable in predicate
			newtable = true;
			table = new DataTable ();
			table.predicate_variable = predicate_variable_map.lookup (current_predicate);
			if (table.predicate_variable == null) {
				table.predicate_variable = new PredicateVariable ();
				predicate_variable_map.insert (current_predicate, table.predicate_variable);
			}
			if (!current_subject_is_var) {
				// single subject
				table.predicate_variable.subject = current_subject;
			}
			if (!current_subject_is_var) {
				// single object
				table.predicate_variable.object = object;
			}
			table.sql_query_tablename = current_predicate + (++counter).to_string ();
			tables.append (table);

			// add to variable list
			var binding = new VariableBinding ();
			binding.is_uri = true;
			binding.variable = current_predicate;
			binding.table = table;
			binding.sql_db_column_name = "predicate";
			var binding_list = pattern_var_map.lookup (binding.variable);
			if (binding_list == null) {
				binding_list = new VariableBindingList ();
				pattern_variables.append (binding.variable);
				pattern_var_map.insert (binding.variable, binding_list);

				pattern_sql.append_printf ("\"%s\".\"%s\" AS \"%s_u\", ",
					binding.table.sql_query_tablename,
					binding.sql_db_column_name,
					binding.variable);
			}
			binding_list.list.append (binding);
			if (var_map.lookup (binding.variable) == null) {
				var_map.insert (binding.variable, binding);
			}
		}
		
		if (newtable) {
			if (current_subject_is_var) {
				var binding = new VariableBinding ();
				binding.is_uri = true;
				binding.variable = current_subject;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				var binding_list = pattern_var_map.lookup (binding.variable);
				if (binding_list == null) {
					binding_list = new VariableBindingList ();
					pattern_variables.append (binding.variable);
					pattern_var_map.insert (binding.variable, binding_list);

					pattern_sql.append_printf ("\"%s\".\"%s\" AS \"%s_u\", ",
						binding.table.sql_query_tablename,
						binding.sql_db_column_name,
						binding.variable);
				}
				binding_list.list.append (binding);
				if (var_map.lookup (binding.variable) == null) {
					var_map.insert (binding.variable, binding);
				}
			} else {
				var binding = new LiteralBinding ();
				binding.is_uri = true;
				binding.literal = current_subject;
				// binding.literal_type = triple.subject.type;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				pattern_bindings.append (binding);
				bindings.append (binding);
			}
		}
		
		if (!rdftype) {
			if (object_is_var) {
				var binding = new VariableBinding ();
				binding.variable = object;
				binding.table = table;
				if (prop != null) {

					binding.type = prop.range.uri;

					if (prop.data_type == PropertyType.RESOURCE) {
						binding.is_uri = true;
					} else if (prop.data_type == PropertyType.BOOLEAN) {
						binding.is_boolean = true;
					} else if (prop.data_type == PropertyType.DATE) {
						binding.is_datetime = true;
					} else if (prop.data_type == PropertyType.DATETIME) {
						binding.is_datetime = true;
					}
					binding.sql_db_column_name = prop.name;
					if (!prop.multiple_values) {
						// for single value properties, row may have NULL
						// in any column except the ID column
						binding.maybe_null = true;
					}
				} else {
					// variable as predicate
					binding.sql_db_column_name = "object";
					binding.maybe_null = true;
				}

				var binding_list = pattern_var_map.lookup (binding.variable);
				if (binding_list == null) {
					binding_list = new VariableBindingList ();
					pattern_variables.append (binding.variable);
					pattern_var_map.insert (binding.variable, binding_list);

					pattern_sql.append_printf ("\"%s\".\"%s\" AS \"%s_u\", ",
						binding.table.sql_query_tablename,
						binding.sql_db_column_name,
						binding.variable);
				}
				binding_list.list.append (binding);
				if (var_map.lookup (binding.variable) == null) {
					var_map.insert (binding.variable, binding);
				}
			} else if (current_predicate == "http://www.tracker-project.org/ontologies/fts#match") {
				var binding = new LiteralBinding ();
				binding.is_fts_match = true;
				binding.literal = object;
				// binding.literal_type = triple.object.type;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				pattern_bindings.append (binding);
			} else {
				var binding = new LiteralBinding ();
				binding.literal = object;
				// binding.literal_type = triple.object.type;
				binding.table = table;
				if (prop != null) {
					if (prop.data_type == PropertyType.RESOURCE) {
						binding.is_uri = true;
						binding.literal = object;
					} else if (prop.data_type == PropertyType.BOOLEAN) {
						binding.is_boolean = true;
					} else if (prop.data_type == PropertyType.DATE) {
						binding.is_datetime = true;
					} else if (prop.data_type == PropertyType.DATETIME) {
						binding.is_datetime = true;
					}
					binding.sql_db_column_name = prop.name;
				} else {
					// variable as predicate
					binding.sql_db_column_name = "object";
				}
				pattern_bindings.append (binding);
				bindings.append (binding);
			}
		}

		if (!current_subject_is_var &&
		    !current_predicate_is_var &&
		    !object_is_var) {
			// no variables involved, add dummy expression to SQL
			pattern_sql.append ("1, ");
		}
	}

	DataTable get_table (string subject, string db_table, bool share_table, out bool newtable) {
		string tablestring = "%s.%s".printf (subject, db_table);
		DataTable table = null;
		newtable = false;
		if (share_table) {
			table = table_map.lookup (tablestring);
		}
		if (table == null) {
			newtable = true;
			table = new DataTable ();
			table.sql_db_tablename = db_table;
			table.sql_query_tablename = db_table + (++counter).to_string ();
			tables.append (table);
			table_map.insert (tablestring, table);
		}
		return table;
	}

#if 0
	bool is_datetime_variable (Rasqal.Expression expr) {
		if (expr.op == Rasqal.Op.LITERAL) {
			if (expr.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.literal.as_variable ().name;
				var binding = var_map.lookup (variable_name);
				if (binding != null && binding.is_datetime) {
					return true;
				}
			}
		}

		return false;
	}

	void visit_filter (Rasqal.Expression expr, bool is_datetime = false) throws SparqlError {
		switch (expr.op) {
		case Rasqal.Op.UMINUS:
			pattern_sql.append ("-(");
			visit_filter (expr.arg1);
			pattern_sql.append (")");
			break;
		case Rasqal.Op.ISBLANK:
			/* We don't store blank nodes as blank nodes atm, so we always 
			 * return false. */
			pattern_sql.append ("(0)");
			break;
		case Rasqal.Op.STR:
			if (expr.arg1.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.arg1.literal.as_variable ().name;
				var binding = var_map.lookup (variable_name);

				if (binding.is_uri) {
					pattern_sql.append_printf ("(SELECT \"%s\".\"Uri\" as \"STR\" FROM \"%s\" WHERE \"%s\".\"ID\" = \"%s_u\")", 
					                           binding.table.sql_db_tablename,
					                           binding.table.sql_db_tablename,
					                           binding.table.sql_db_tablename,
					                           variable_name);
				} else {
					visit_filter (expr.arg1);
				}
			} else if (expr.arg1.literal.type == Rasqal.Literal.Type.URI) {
				// Rasqal already takes care of this, but I added it here
				// for the reader of this code to understand what goes on
				// Note that rasqal will have converted a str(<urn:something>)
				// to a literal string 'urn:something'
				pattern_sql.append_printf ("'%s'", expr.arg1.literal.as_string ());
			} else {
				visit_filter (expr.arg1);
			}
			break;
		case Rasqal.Op.ISURI:
			if (expr.arg1.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.arg1.literal.as_variable ().name;
				var binding = var_map.lookup (variable_name);

				if (!binding.is_uri) {
					pattern_sql.append ("(0)");
				} else {
					pattern_sql.append ("(1)");
				}
			} else if (expr.arg1.literal.type != Rasqal.Literal.Type.URI) {
				pattern_sql.append ("(0)");
			} else {
				pattern_sql.append ("(1)");
			}
			break;
		case Rasqal.Op.DATATYPE:
			if (expr.arg1.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.arg1.literal.as_variable ().name;
				var binding = var_map.lookup (variable_name);

				if (binding.is_uri || binding.type == null) {
					throw new SparqlError.PARSE ("Invalid FILTER");
				}

				pattern_sql.append_printf ("(SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s')", binding.type);
			} else {
				throw new SparqlError.PARSE ("Invalid FILTER");
			}
			break;
		case Rasqal.Op.LITERAL:
			if (expr.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.literal.as_variable ().name;
				pattern_sql.append_printf ("\"%s_u\"", variable_name);
			} else {
				if (expr.literal.type == Rasqal.Literal.Type.URI) {
					pattern_sql.append ("(SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
				} else {
					pattern_sql.append ("?");
				}

				var binding = new LiteralBinding ();
				binding.literal = expr.literal.as_string ();
				binding.literal_type = expr.literal.type;
				binding.is_datetime = is_datetime;
				bindings.append (binding);
			}
			break;
		}
	}
#endif

	static string? get_string_for_value (Value value)
	{
		if (value.type () == typeof (int)) {
			return value.get_int ().to_string ();
		} else if (value.type () == typeof (double)) {
			return value.get_double ().to_string ();
		} else if (value.type () == typeof (string)) {
			return value.get_string ();
		} else {
			return null;
		}
	}

	[CCode (cname = "uuid_generate")]
	public extern static void uuid_generate ([CCode (array_length = false)] uchar[] uuid);
}

