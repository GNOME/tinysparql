/*
 * Copyright (C) 2008-2010, Nokia
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

public errordomain Tracker.SparqlError {
	PARSE,
	UNKNOWN_CLASS,
	UNKNOWN_PROPERTY,
	TYPE,
	INTERNAL,
	UNSUPPORTED
}

namespace Tracker.Sparql {
	enum VariableState {
		NONE,
		BOUND,
		OPTIONAL
	}

	// Represents a SQL table
	class DataTable : Object {
		public string sql_db_tablename; // as in db schema
		public string sql_query_tablename; // temp. name, generated
		public PredicateVariable predicate_variable;
	}

	abstract class DataBinding : Object {
		public PropertyType data_type;
		public DataTable table;
		public string sql_db_column_name;
		public string sql_expression {
			get {
				if (this._sql_expression == null) {
					this._sql_expression = "\"%s\".\"%s\"".printf (table.sql_query_tablename, sql_db_column_name);
				}
				return this._sql_expression;
			}
			set {
				this._sql_expression = value;
			}
		}
		string? _sql_expression;
		public string get_extra_sql_expression (string suffix) {
			return "\"%s\".\"%s:%s\"".printf (table.sql_query_tablename, sql_db_column_name, suffix);
		}
	}

	// Represents a mapping of a SPARQL literal to a SQL table and column
	class LiteralBinding : DataBinding {
		public bool is_fts_match;
		public string literal;
	}

	// Represents a mapping of a SPARQL variable to a SQL table and column
	class VariableBinding : DataBinding {
		public weak Variable variable;
		// Specified whether SQL column may contain NULL entries
		public bool maybe_null;
		public bool in_simple_optional;
		public Class? type;
	}

	class VariableBindingList : Object {
		public List<VariableBinding> list;
	}

	class Variable : Object {
		public string name { get; private set; }
		public string sql_expression { get; private set; }
		public VariableBinding binding;
		string sql_identifier;

		public Variable (string name, string sql_identifier) {
			this.name = name;
			this.sql_identifier = sql_identifier;
			this.sql_expression = "\"%s\"".printf (sql_identifier);
		}

		public string get_extra_sql_expression (string suffix) {
			return "\"%s:%s\"".printf (sql_identifier, suffix);
		}
	}

	class Context {
		public Context? parent_context;
		// All SPARQL variables within a subgraph pattern (used by UNION)
		// value is VariableState
		public HashTable<Variable,int> var_set;

		public HashTable<string,Variable> var_map;
		// All selected SPARQL variables (used by compositional subqueries)
		public HashTable<Variable,int> select_var_set;

		// Variables used as predicates
		public HashTable<Variable,PredicateVariable> predicate_variable_map;

		// Keep track of used sql identifiers to avoid using the same for multiple SPARQL variables
		public HashTable<string,bool> used_sql_identifiers;

		public bool scalar_subquery;

		public Context (Context? parent_context = null) {
			this.parent_context = parent_context;
			this.var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);

			if (parent_context == null) {
				select_var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);
				var_map = new HashTable<string,Variable>.full (str_hash, str_equal, g_free, g_object_unref);
				predicate_variable_map = new HashTable<Variable,PredicateVariable>.full (direct_hash, direct_equal, g_object_unref, g_object_unref);
				used_sql_identifiers = new HashTable<string,bool>.full (str_hash, str_equal, g_free, null);
			} else {
				select_var_set = parent_context.select_var_set;
				var_map = parent_context.var_map;
				predicate_variable_map = parent_context.predicate_variable_map;
				used_sql_identifiers = parent_context.used_sql_identifiers;
			}
		}

		public Context.subquery (Context parent_context) {
			this.parent_context = parent_context;
			this.var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);

			select_var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);
			var_map = parent_context.var_map;
			predicate_variable_map = new HashTable<Variable,PredicateVariable>.full (direct_hash, direct_equal, g_object_unref, g_object_unref);
			used_sql_identifiers = new HashTable<string,bool>.full (str_hash, str_equal, g_free, null);
			scalar_subquery = true;
		}

		internal unowned Variable get_variable (string name) {
			unowned Variable result = this.var_map.lookup (name);
			if (result == null) {
				// use lowercase as SQLite is never case sensitive (not conforming to SQL)
				string sql_identifier = "%s_u".printf (name).down ();

				// ensure SQL identifier is unique to avoid conflicts between
				// case sensitive SPARQL and case insensitive SQLite
				for (int i = 1; this.used_sql_identifiers.lookup (sql_identifier); i++) {
					sql_identifier = "%s_%d_u".printf (name, i).down ();
				}
				this.used_sql_identifiers.insert (sql_identifier, true);

				var variable = new Variable (name, sql_identifier);
				this.var_map.insert (name, variable);

				result = variable;
			}
			return result;
		}
	}

	class SelectContext : Context {
		public PropertyType type;

		public SelectContext (Context? parent_context = null) {
			base (parent_context);
		}

		public SelectContext.subquery (Context parent_context) {
			base.subquery (parent_context);
		}
	}
}

public class Tracker.Sparql.Query : Object {
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

	const string FN_NS = "http://www.w3.org/2005/xpath-functions#";

	string query_string;
	bool update_extensions;

	internal Expression expression;
	internal Pattern pattern;

	string current_graph;
	string current_subject;
	bool current_subject_is_var;
	string current_predicate;
	bool current_predicate_is_var;

	// SILENT => ignore (non-syntax) errors
	bool silent;

	HashTable<string,string> prefix_map;

	// All SPARQL literals
	internal List<LiteralBinding> bindings;

	internal Context context;

	bool delete_statements;

	int bnodeid = 0;
	// base UUID used for blank nodes
	uchar[] base_uuid;
	HashTable<string,string> blank_nodes;

	public Query (string query) {
		tokens = new TokenInfo[BUFFER_SIZE];
		prefix_map = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);

		base_uuid = new uchar[16];
		uuid_generate (base_uuid);

		this.query_string = query;

		expression = new Expression (this);
		pattern = new Pattern (this);
	}

	public Query.update (string query) {
		this (query);
		this.update_extensions = true;
	}

	string get_uuid_for_name (uchar[] base_uuid, string name) {
		var checksum = new Checksum (ChecksumType.SHA1);
		// base UUID, unique per file
		checksum.update (base_uuid, 16);

		// node ID
		checksum.update ((uchar[]) name, -1);

		string sha1 = checksum.get_string ();

		// generate name based uuid
		return "urn:uuid:%.8s-%.4s-%.4s-%.4s-%.12s".printf (
			sha1, sha1.offset (8), sha1.offset (12), sha1.offset (16), sha1.offset (20));
	}

	internal string generate_bnodeid (string? user_bnodeid) {
		// user_bnodeid is NULL for anonymous nodes
		if (user_bnodeid == null) {
			return ":%d".printf (++bnodeid);
		} else {
			string uri = null;

			if (blank_nodes != null) {
				uri = blank_nodes.lookup (user_bnodeid);
				if (uri != null) {
					return uri;
				}
			}

			uri = get_uuid_for_name (base_uuid, user_bnodeid);

			if (blank_nodes != null) {
				while (Data.query_resource_id (uri) > 0) {
					// uri collision, generate new UUID
					uchar[] new_base_uuid = new uchar[16];
					uuid_generate (new_base_uuid);
					uri = get_uuid_for_name (new_base_uuid, user_bnodeid);
				}

				blank_nodes.insert (user_bnodeid, uri);
			}

			return uri;
		}
	}

	internal bool next () throws SparqlError {
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

	internal SparqlTokenType current () {
		return tokens[index].type;
	}

	internal SparqlTokenType last () {
		int last_index = (index + BUFFER_SIZE - 1) % BUFFER_SIZE;
		return tokens[last_index].type;
	}

	internal bool accept (SparqlTokenType type) throws SparqlError {
		if (current () == type) {
			next ();
			return true;
		}
		return false;
	}

	internal SparqlError get_error (string msg) {
		return new SparqlError.PARSE ("%d.%d: syntax error, %s".printf (tokens[index].begin.line, tokens[index].begin.column, msg));
	}

	internal SparqlError get_internal_error (string msg) {
		return new SparqlError.INTERNAL ("%d.%d: %s".printf (tokens[index].begin.line, tokens[index].begin.column, msg));
	}

	internal bool expect (SparqlTokenType type) throws SparqlError {
		if (accept (type)) {
			return true;
		}

		throw get_error ("expected %s".printf (type.to_string ()));
	}

	internal SourceLocation get_location () {
		return tokens[index].begin;
	}

	internal void set_location (SourceLocation location) {
		scanner.seek (location);
		size = 0;
		index = 0;
		try {
			next ();
		} catch (SparqlError e) {
			// this should never happen as this is the second time we scan this token
			critical ("internal error: next in set_location failed");
		}
	}

	internal string get_last_string (int strip = 0) {
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

	void prepare_execute () throws DBInterfaceError, SparqlError, DateError {
		assert (!update_extensions);

		scanner = new SparqlScanner ((char*) query_string, (long) query_string.size ());
		next ();

		// declare fn prefix for XPath functions
		prefix_map.insert ("fn", FN_NS);

		foreach (Namespace ns in Ontologies.get_namespaces ()) {
			if (ns.prefix == null) {
				critical ("Namespace does not specify a prefix: %s", ns.uri);
				continue;
			}
			prefix_map.insert (ns.prefix, ns.uri);
		}

		parse_prologue ();
	}

	public DBResultSet? execute () throws DBInterfaceError, SparqlError, DateError {

		prepare_execute ();

		switch (current ()) {
		case SparqlTokenType.SELECT:
			return execute_select ();
		case SparqlTokenType.CONSTRUCT:
			throw get_internal_error ("CONSTRUCT is not supported");
		case SparqlTokenType.DESCRIBE:
			throw get_internal_error ("DESCRIBE is not supported");
		case SparqlTokenType.ASK:
			return execute_ask ();
		case SparqlTokenType.INSERT:
		case SparqlTokenType.DELETE:
		case SparqlTokenType.DROP:
			throw get_error ("INSERT and DELETE are not supported in query mode");
		default:
			throw get_error ("expected SELECT or ASK");
		}
	}


	public DBCursor? execute_cursor () throws DBInterfaceError, SparqlError, DateError {

		prepare_execute ();

		switch (current ()) {
		case SparqlTokenType.SELECT:
			return execute_select_cursor ();
		case SparqlTokenType.CONSTRUCT:
			throw get_internal_error ("CONSTRUCT is not supported");
		case SparqlTokenType.DESCRIBE:
			throw get_internal_error ("DESCRIBE is not supported");
		case SparqlTokenType.ASK:
			return execute_ask_cursor ();
		case SparqlTokenType.INSERT:
		case SparqlTokenType.DELETE:
		case SparqlTokenType.DROP:
			throw get_error ("INSERT and DELETE are not supported in query mode");
		default:
			throw get_error ("expected SELECT or ASK");
		}
	}

	public PtrArray? execute_update (bool blank) throws DataError, DBInterfaceError, SparqlError, DateError {
		assert (update_extensions);

		scanner = new SparqlScanner ((char*) query_string, (long) query_string.size ());
		next ();

		// declare fn prefix for XPath functions
		prefix_map.insert ("fn", FN_NS);

		foreach (Namespace ns in Ontologies.get_namespaces ()) {
			if (ns.prefix == null) {
				critical ("Namespace does not specify a prefix: %s", ns.uri);
				continue;
			}
			prefix_map.insert (ns.prefix, ns.uri);
		}

		parse_prologue ();

		PtrArray blank_nodes = null;
		if (blank) {
			blank_nodes = new PtrArray ();
		}

		// SPARQL update supports multiple operations in a single query

		while (current () != SparqlTokenType.EOF) {
			switch (current ()) {
			case SparqlTokenType.WITH:
			case SparqlTokenType.INSERT:
			case SparqlTokenType.DELETE:
				PtrArray* ptr = execute_insert_or_delete (blank);
				if (ptr != null) {
					blank_nodes.add (ptr);
				}
				break;
			case SparqlTokenType.DROP:
				execute_drop_graph ();
				break;
			case SparqlTokenType.SELECT:
			case SparqlTokenType.CONSTRUCT:
			case SparqlTokenType.DESCRIBE:
			case SparqlTokenType.ASK:
				throw get_error ("SELECT, CONSTRUCT, DESCRIBE, and ASK are not supported in update mode");
			default:
				throw get_error ("expected INSERT or DELETE");
			}
		}

		return blank_nodes;
	}

	DBStatement prepare_for_exec (string sql) throws DBInterfaceError, SparqlError, DateError {
		var iface = DBManager.get_db_interface ();
		var stmt = iface.create_statement ("%s", sql);

		// set literals specified in query
		int i = 0;
		foreach (LiteralBinding binding in bindings) {
			if (binding.data_type == PropertyType.BOOLEAN) {
				if (binding.literal == "true" || binding.literal == "1") {
					stmt.bind_int (i, 1);
				} else if (binding.literal == "false" || binding.literal == "0") {
					stmt.bind_int (i, 0);
				} else {
					throw new SparqlError.TYPE ("`%s' is not a valid boolean".printf (binding.literal));
				}
			} else if (binding.data_type == PropertyType.DATETIME) {
				stmt.bind_int (i, string_to_date (binding.literal, null));
			} else if (binding.data_type == PropertyType.INTEGER) {
				stmt.bind_int (i, binding.literal.to_int ());
			} else {
				stmt.bind_text (i, binding.literal);
			}
			i++;
		}

		return stmt;
	}

	DBResultSet? exec_sql (string sql) throws DBInterfaceError, SparqlError, DateError {
		var stmt = prepare_for_exec (sql);

		return stmt.execute ();
	}

	DBCursor? exec_sql_cursor (string sql) throws DBInterfaceError, SparqlError, DateError {
		var stmt = prepare_for_exec (sql);

		return stmt.start_cursor ();
	}

	string get_select_query () throws DBInterfaceError, SparqlError, DateError {
		// SELECT query

		// build SQL
		var sql = new StringBuilder ();
		pattern.translate_select (sql);

		expect (SparqlTokenType.EOF);

		return sql.str;
	}

	DBResultSet? execute_select () throws DBInterfaceError, SparqlError, DateError {
		return exec_sql (get_select_query ());
	}

	DBCursor? execute_select_cursor () throws DBInterfaceError, SparqlError, DateError {
		return exec_sql_cursor (get_select_query ());
	}

	string get_ask_query () throws DBInterfaceError, SparqlError, DateError {
		// ASK query

		var pattern_sql = new StringBuilder ();

		// build SQL
		var sql = new StringBuilder ();
		sql.append ("SELECT ");

		expect (SparqlTokenType.ASK);

		sql.append ("COUNT(1) > 0");

		accept (SparqlTokenType.WHERE);

		context = pattern.translate_group_graph_pattern (pattern_sql);

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql.str);
		sql.append (")");

		context = context.parent_context;

		return sql.str;
	}

	DBResultSet? execute_ask () throws DBInterfaceError, SparqlError, DateError {
		return exec_sql (get_ask_query ());
	}

	DBCursor? execute_ask_cursor () throws DBInterfaceError, SparqlError, DateError {
		return exec_sql_cursor (get_ask_query ());
	}

	private void parse_from_or_into_param () throws SparqlError {
		if (accept (SparqlTokenType.IRI_REF)) {
			current_graph = get_last_string (1);
		} else if (accept (SparqlTokenType.PN_PREFIX)) {
			string ns = get_last_string ();
			expect (SparqlTokenType.COLON);
			current_graph = resolve_prefixed_name (ns, get_last_string ().substring (1));
		} else {
			expect (SparqlTokenType.COLON);
			current_graph = resolve_prefixed_name ("", get_last_string ().substring (1));
		}
	}

	PtrArray? execute_insert_or_delete (bool blank) throws DBInterfaceError, DataError, SparqlError, DateError {
		// INSERT or DELETE

		if (accept (SparqlTokenType.WITH)) {
			parse_from_or_into_param ();
		} else {
			current_graph = null;
		}

		bool delete_statements;

		if (accept (SparqlTokenType.INSERT)) {
			delete_statements = false;

			// SILENT => ignore (non-syntax) errors
			silent = accept (SparqlTokenType.SILENT);

			if (current_graph == null && accept (SparqlTokenType.INTO)) {
				parse_from_or_into_param ();
			}
		} else {
			expect (SparqlTokenType.DELETE);
			delete_statements = true;
			blank = false;

			// SILENT => ignore (non-syntax) errors
			silent = accept (SparqlTokenType.SILENT);

			if (current_graph == null && accept (SparqlTokenType.FROM)) {
				parse_from_or_into_param ();
			}
		}

		var pattern_sql = new StringBuilder ();

		var sql = new StringBuilder ();

		var template_location = get_location ();
		skip_braces ();

		if (accept (SparqlTokenType.WHERE)) {
			// graph only applies to actual insert, not to WHERE part
			var old_graph = current_graph;
			current_graph = null;

			context = pattern.translate_group_graph_pattern (pattern_sql);

			current_graph = old_graph;
		} else {
			context = new Context ();
		}

		var after_where = get_location ();

		// build SQL
		sql.append ("SELECT ");
		bool first = true;
		foreach (var variable in context.var_set.get_keys ()) {
			if (!first) {
				sql.append (", ");
			} else {
				first = false;
			}

			if (variable.binding == null) {
				throw get_error ("use of undefined variable `%s'".printf (variable.name));
			}
			Expression.append_expression_as_string (sql, variable.sql_expression, variable.binding.data_type);
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

		PtrArray update_blank_nodes = null;

		if (blank) {
			update_blank_nodes = new PtrArray ();
		}

		// iterate over all solutions
		if (result_set != null) {
			do {
				// blank nodes in construct templates are per solution

				uuid_generate (base_uuid);
				blank_nodes = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);

				// get values of all variables to be bound
				var var_value_map = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);
				int var_idx = 0;
				foreach (var variable in context.var_set.get_keys ()) {
					Value value;
					result_set._get_value (var_idx++, out value);
					var_value_map.insert (variable.name, get_string_for_value (value));
				}

				set_location (template_location);

				// iterate over each triple in the template
				parse_construct_triples_block (var_value_map);

				if (blank) {
					HashTable<string,string>* ptr = (owned) blank_nodes;
					update_blank_nodes.add (ptr);
				}

				Data.update_buffer_might_flush ();
			} while (result_set.iter_next ());
		}

		// reset location to the end of the update
		set_location (after_where);

		// ensure possible WHERE clause in next part gets the correct results
		Data.update_buffer_flush ();
		bindings = null;

		context = context.parent_context;

		return update_blank_nodes;
	}

	void execute_drop_graph () throws DBInterfaceError, DataError, SparqlError {
		expect (SparqlTokenType.DROP);
		expect (SparqlTokenType.GRAPH);

		bool is_var;
		string url = pattern.parse_var_or_term (null, out is_var);

		Data.delete_resource_description (url, url);

		// ensure possible WHERE clause in next part gets the correct results
		Data.update_buffer_flush ();
	}

	internal string resolve_prefixed_name (string prefix, string local_name) throws SparqlError {
		string ns = prefix_map.lookup (prefix);
		if (ns == null) {
			throw get_error ("use of undefined prefix `%s'".printf (prefix));
		}
		return ns + local_name;
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
				throw get_error ("unexpected end of query, expected }");
			} else {
				// ignore everything else
				next ();
			}
		}
	}

	void parse_construct_triples_block (HashTable<string,string> var_value_map) throws SparqlError, DataError, DateError {
		expect (SparqlTokenType.OPEN_BRACE);

		while (current () != SparqlTokenType.CLOSE_BRACE) {
			if (accept (SparqlTokenType.GRAPH)) {
				var old_graph = current_graph;
				current_graph = parse_construct_var_or_term (var_value_map);

				expect (SparqlTokenType.OPEN_BRACE);

				while (current () != SparqlTokenType.CLOSE_BRACE) {
					current_subject = parse_construct_var_or_term (var_value_map);
					parse_construct_property_list_not_empty (var_value_map);
					if (!accept (SparqlTokenType.DOT)) {
						// no triples following
						break;
					}
				}

				expect (SparqlTokenType.CLOSE_BRACE);

				current_graph = old_graph;
			} else {
				current_subject = parse_construct_var_or_term (var_value_map);
				parse_construct_property_list_not_empty (var_value_map);
				if (!accept (SparqlTokenType.DOT) && current () != SparqlTokenType.GRAPH) {
					// neither GRAPH nor triples following
					break;
				}
			}
		}

		expect (SparqlTokenType.CLOSE_BRACE);
	}

	bool anon_blank_node_open = false;

	string parse_construct_var_or_term (HashTable<string,string> var_value_map) throws SparqlError, DataError, DateError {
		string result = "";
		if (current () == SparqlTokenType.VAR) {
			next ();
			result = var_value_map.lookup (get_last_string ().substring (1));
			if (result == null) {
				throw get_error ("use of undefined variable `%s'".printf (get_last_string ().substring (1)));
			}
		} else if (current () == SparqlTokenType.IRI_REF) {
			next ();
			result = get_last_string (1);
		} else if (current () == SparqlTokenType.PN_PREFIX) {
			// prefixed name with namespace foo:bar
			next ();
			string ns = get_last_string ();
			expect (SparqlTokenType.COLON);
			result = resolve_prefixed_name (ns, get_last_string ().substring (1));
		} else if (current () == SparqlTokenType.COLON) {
			// prefixed name without namespace :bar
			next ();
			result = resolve_prefixed_name ("", get_last_string ().substring (1));
		} else if (accept (SparqlTokenType.BLANK_NODE)) {
			// _:foo
			expect (SparqlTokenType.COLON);
			result = generate_bnodeid (get_last_string ().substring (1));
		} else if (current () == SparqlTokenType.MINUS) {
			next ();
			if (current () == SparqlTokenType.INTEGER ||
			    current () == SparqlTokenType.DECIMAL ||
			    current () == SparqlTokenType.DOUBLE) {
				next ();
				result = "-" + get_last_string ();
			} else {
				throw get_error ("expected variable or term");
			}
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
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL2) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG1) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG2) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.OPEN_BRACKET) {

			if (anon_blank_node_open) {
				throw get_error ("no support for nested anonymous blank nodes");
			}

			anon_blank_node_open = true;
			next ();

			result = generate_bnodeid (null);

			string old_subject = current_subject;
			bool old_subject_is_var = current_subject_is_var;

			current_subject = result;
			parse_construct_property_list_not_empty (var_value_map);
			expect (SparqlTokenType.CLOSE_BRACKET);
			anon_blank_node_open = false;

			current_subject = old_subject;
			current_subject_is_var = old_subject_is_var;
		} else {
			throw get_error ("expected variable or term");
		}
		return result;
	}

	void parse_construct_property_list_not_empty (HashTable<string,string> var_value_map) throws SparqlError, DataError, DateError {
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
				current_predicate = resolve_prefixed_name (ns, get_last_string ().substring (1));
			} else if (current () == SparqlTokenType.COLON) {
				next ();
				current_predicate = resolve_prefixed_name ("", get_last_string ().substring (1));
			} else if (current () == SparqlTokenType.A) {
				next ();
				current_predicate = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
			} else {
				throw get_error ("expected non-empty property list");
			}
			parse_construct_object_list (var_value_map);

			current_predicate = old_predicate;

			if (accept (SparqlTokenType.SEMICOLON)) {
				continue;
			}
			break;
		}
	}

	void parse_construct_object_list (HashTable<string,string> var_value_map) throws SparqlError, DataError, DateError {
		while (true) {
			parse_construct_object (var_value_map);
			if (accept (SparqlTokenType.COMMA)) {
				continue;
			}
			break;
		}
	}

	void parse_construct_object (HashTable<string,string> var_value_map) throws SparqlError, DataError, DateError {
		string object = parse_construct_var_or_term (var_value_map);
		try {
			if (delete_statements) {
				// delete triple from database
				Data.delete_statement (current_graph, current_subject, current_predicate, object);
			} else {
				// insert triple into database
				Data.insert_statement (current_graph, current_subject, current_predicate, object);
			}
		} catch (DataError e) {
			if (!silent) {
				throw e;
			}
		} catch (DateError e) {
			if (!silent) {
				throw e;
			}
		}
	}

	static string? get_string_for_value (Value value)
	{
		if (value.type () == typeof (int)) {
			return value.get_int ().to_string ();
		} else if (value.type () == typeof (int64)) {
			return value.get_int64 ().to_string ();
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

