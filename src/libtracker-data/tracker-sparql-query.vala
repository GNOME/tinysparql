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
		public int index { get; private set; }
		public string sql_expression { get; private set; }
		public VariableBinding binding;
		string sql_identifier;

		public Variable (string name, int index) {
			this.name = name;
			this.index = index;
			this.sql_identifier = "%d_u".printf (index);
			this.sql_expression = "\"%s\"".printf (sql_identifier);
		}

		public string get_extra_sql_expression (string suffix) {
			return "\"%s:%s\"".printf (sql_identifier, suffix);
		}

		public static bool equal (Variable a, Variable b) {
			return a.index == b.index;
		}

		public static uint hash (Variable variable) {
			return (uint) variable.index;
		}
	}

	class Context {
		public weak Query query;

		public Context? parent_context;
		// All SPARQL variables within a subgraph pattern (used by UNION)
		// value is VariableState
		public HashTable<Variable,int> var_set;

		public HashTable<string,Variable> var_map;
		// All selected SPARQL variables (used by compositional subqueries)
		public HashTable<Variable,int> select_var_set;

		// Variables used as predicates
		public HashTable<Variable,PredicateVariable> predicate_variable_map;

		public bool scalar_subquery;

		public Context (Query query, Context? parent_context = null) {
			this.query = query;
			this.parent_context = parent_context;
			this.var_set = new HashTable<Variable,int>.full (Variable.hash, Variable.equal, g_object_unref, null);

			if (parent_context == null) {
				select_var_set = new HashTable<Variable,int>.full (Variable.hash, Variable.equal, g_object_unref, null);
				var_map = new HashTable<string,Variable>.full (str_hash, str_equal, g_free, g_object_unref);
				predicate_variable_map = new HashTable<Variable,PredicateVariable>.full (Variable.hash, Variable.equal, g_object_unref, g_object_unref);
			} else {
				select_var_set = parent_context.select_var_set;
				var_map = parent_context.var_map;
				predicate_variable_map = parent_context.predicate_variable_map;
			}
		}

		public Context.subquery (Query query, Context parent_context) {
			this.query = query;
			this.parent_context = parent_context;
			this.var_set = new HashTable<Variable,int>.full (Variable.hash, Variable.equal, g_object_unref, null);

			select_var_set = new HashTable<Variable,int>.full (Variable.hash, Variable.equal, g_object_unref, null);
			var_map = parent_context.var_map;
			predicate_variable_map = new HashTable<Variable,PredicateVariable>.full (Variable.hash, Variable.equal, g_object_unref, g_object_unref);
			scalar_subquery = true;
		}

		internal unowned Variable get_variable (string name) {
			unowned Variable result = this.var_map.lookup (name);
			if (result == null) {
				var variable = new Variable (name, ++query.last_var_index);
				this.var_map.insert (name, variable);

				result = variable;
			}
			return result;
		}
	}

	class SelectContext : Context {
		public PropertyType type;
		public PropertyType[] types = {};
		public string[] variable_names = {};

		public SelectContext (Query query, Context? parent_context = null) {
			base (query, parent_context);
		}

		public SelectContext.subquery (Query query, Context parent_context) {
			base.subquery (query, parent_context);
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
	bool update_statements;

	int bnodeid = 0;
	// base UUID used for blank nodes
	uchar[] base_uuid;
	HashTable<string,string> blank_nodes;

	// Keep track of used SQL identifiers for SPARQL variables
	public int last_var_index;

	public bool no_cache { get; set; }

	public Query (string query) {
		no_cache = false; /* Start with false, expression sets it */
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
			sha1, sha1.substring (8), sha1.substring (12), sha1.substring (16), sha1.substring (20));
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

	internal bool next () throws Sparql.Error {
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

	internal bool accept (SparqlTokenType type) throws Sparql.Error {
		if (current () == type) {
			next ();
			return true;
		}
		return false;
	}

	internal Sparql.Error get_error (string msg) {
		return new Sparql.Error.PARSE ("%d.%d: syntax error, %s".printf (tokens[index].begin.line, tokens[index].begin.column, msg));
	}

	internal Sparql.Error get_internal_error (string msg) {
		return new Sparql.Error.INTERNAL ("%d.%d: %s".printf (tokens[index].begin.line, tokens[index].begin.column, msg));
	}

	internal bool expect (SparqlTokenType type) throws Sparql.Error {
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
		} catch (Sparql.Error e) {
			// this should never happen as this is the second time we scan this token
			critical ("internal error: next in set_location failed");
		}
	}

	internal string get_last_string (int strip = 0) {
		int last_index = (index + BUFFER_SIZE - 1) % BUFFER_SIZE;
		// do not switch to substring for performance reasons until we require Vala 0.11.6
		return ((string) (tokens[last_index].begin.pos + strip)).ndup ((tokens[last_index].end.pos - tokens[last_index].begin.pos - 2 * strip));
	}

	void parse_prologue () throws Sparql.Error {
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

	void prepare_execute () throws DBInterfaceError, Sparql.Error, DateError {
		assert (!update_extensions);

		scanner = new SparqlScanner ((char*) query_string, (long) query_string.length);
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


	public DBCursor? execute_cursor (bool threadsafe) throws DBInterfaceError, Sparql.Error, DateError {

		prepare_execute ();

		switch (current ()) {
		case SparqlTokenType.SELECT:
			return execute_select_cursor (threadsafe);
		case SparqlTokenType.CONSTRUCT:
			throw get_internal_error ("CONSTRUCT is not supported");
		case SparqlTokenType.DESCRIBE:
			throw get_internal_error ("DESCRIBE is not supported");
		case SparqlTokenType.ASK:
			return execute_ask_cursor (threadsafe);
		case SparqlTokenType.INSERT:
		case SparqlTokenType.DELETE:
		case SparqlTokenType.DROP:
			throw get_error ("INSERT and DELETE are not supported in query mode");
		default:
			throw get_error ("expected SELECT or ASK");
		}
	}

	public Variant? execute_update (bool blank) throws GLib.Error {
		Variant result = null;
		assert (update_extensions);

		scanner = new SparqlScanner ((char*) query_string, (long) query_string.length);
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

		// SPARQL update supports multiple operations in a single query
		VariantBuilder? ublank_nodes = null;

		if (blank) {
			ublank_nodes = new VariantBuilder ((VariantType) "aaa{ss}");
		}

		while (current () != SparqlTokenType.EOF) {
			switch (current ()) {
			case SparqlTokenType.WITH:
			case SparqlTokenType.INSERT:
			case SparqlTokenType.DELETE:
				if (blank) {
					ublank_nodes.open ((VariantType) "aa{ss}");
					execute_insert_or_delete (ublank_nodes);
					ublank_nodes.close ();
				} else {
					execute_insert_or_delete (null);
				}
				break;
			case SparqlTokenType.DROP:
				throw get_internal_error ("DROP GRAPH is not supported");
			case SparqlTokenType.SELECT:
			case SparqlTokenType.CONSTRUCT:
			case SparqlTokenType.DESCRIBE:
			case SparqlTokenType.ASK:
				throw get_error ("SELECT, CONSTRUCT, DESCRIBE, and ASK are not supported in update mode");
			default:
				throw get_error ("expected INSERT or DELETE");
			}

			// semicolon is used to separate multiple operations in the current SPARQL Update draft
			// keep it optional for now to reatin backward compatibility
			accept (SparqlTokenType.SEMICOLON);
		}

		if (blank) {
			result = ublank_nodes.end ();
		}

		return result;
	}

	DBStatement prepare_for_exec (string sql) throws DBInterfaceError, Sparql.Error, DateError {
		var iface = DBManager.get_db_interface ();
		var stmt = iface.create_statement (no_cache ? DBStatementCacheType.NONE : DBStatementCacheType.SELECT, "%s", sql);

		// set literals specified in query
		int i = 0;
		foreach (LiteralBinding binding in bindings) {
			if (binding.data_type == PropertyType.BOOLEAN) {
				if (binding.literal == "true" || binding.literal == "1") {
					stmt.bind_int (i, 1);
				} else if (binding.literal == "false" || binding.literal == "0") {
					stmt.bind_int (i, 0);
				} else {
					throw new Sparql.Error.TYPE ("`%s' is not a valid boolean".printf (binding.literal));
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

	DBCursor? exec_sql_cursor (string sql, PropertyType[]? types, string[]? variable_names, bool threadsafe) throws DBInterfaceError, Sparql.Error, DateError {
		var stmt = prepare_for_exec (sql);

		return stmt.start_sparql_cursor (types, variable_names, threadsafe);
	}

	string get_select_query (out SelectContext context) throws DBInterfaceError, Sparql.Error, DateError {
		// SELECT query

		// build SQL
		var sql = new StringBuilder ();
		context = pattern.translate_select (sql);

		expect (SparqlTokenType.EOF);

		return sql.str;
	}

	DBCursor? execute_select_cursor (bool threadsafe) throws DBInterfaceError, Sparql.Error, DateError {
		SelectContext context;
		string sql = get_select_query (out context);

		return exec_sql_cursor (sql, context.types, context.variable_names, true);
	}

	string get_ask_query () throws DBInterfaceError, Sparql.Error, DateError {
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

	DBCursor? execute_ask_cursor (bool threadsafe) throws DBInterfaceError, Sparql.Error, DateError {
		return exec_sql_cursor (get_ask_query (), new PropertyType[] { PropertyType.BOOLEAN }, new string[] { "result" }, true);
	}

	private void parse_from_or_into_param () throws Sparql.Error {
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

	void execute_insert_or_delete (VariantBuilder? update_blank_nodes) throws GLib.Error {
		bool blank = true;

		// INSERT or DELETE

		if (accept (SparqlTokenType.WITH)) {
			parse_from_or_into_param ();
		} else {
			current_graph = null;
		}

		bool delete_statements;
		bool update_statements;

		if (accept (SparqlTokenType.INSERT)) {
			delete_statements = false;
			update_statements = false;

			// SILENT => ignore (non-syntax) errors

			if (accept (SparqlTokenType.OR)) {
				expect (SparqlTokenType.REPLACE);
				update_statements = true;
			}

			if (!update_statements) {
				silent = accept (SparqlTokenType.SILENT);
			}

			if (current_graph == null && accept (SparqlTokenType.INTO)) {
				parse_from_or_into_param ();
			}
		} else {
			expect (SparqlTokenType.DELETE);
			delete_statements = true;
			update_statements = false;
			blank = false;

			// SILENT => ignore (non-syntax) errors
			silent = accept (SparqlTokenType.SILENT);

			if (current_graph == null && accept (SparqlTokenType.FROM)) {
				parse_from_or_into_param ();
			}
		}

		// INSERT/DELETE DATA are simpler variants that don't support variables
		bool data = (current_graph == null && accept (SparqlTokenType.DATA));

		var pattern_sql = new StringBuilder ();

		var sql = new StringBuilder ();

		var template_location = get_location ();

		if (!data) {
			skip_braces ();

			if (accept (SparqlTokenType.WHERE)) {
				pattern.current_graph = current_graph;
				context = pattern.translate_group_graph_pattern (pattern_sql);
				pattern.current_graph = null;
			} else {
				context = new Context (this);

				pattern_sql.append ("SELECT 1");
			}
		} else {
			// WHERE pattern not supported for INSERT/DELETE DATA

			context = new Context (this);

			pattern_sql.append ("SELECT 1");
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
		}

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql.str);
		sql.append (")");

		var cursor = exec_sql_cursor (sql.str, null, null, false);

		this.delete_statements = delete_statements;
		this.update_statements = update_statements;

		// iterate over all solutions
		while (cursor.next ()) {
			// blank nodes in construct templates are per solution

			uuid_generate (base_uuid);
			blank_nodes = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);

			// get values of all variables to be bound
			var var_value_map = new HashTable<string,string>.full (str_hash, str_equal, g_free, g_free);
			int var_idx = 0;
			foreach (var variable in context.var_set.get_keys ()) {
				var_value_map.insert (variable.name, cursor.get_string (var_idx++));
			}

			set_location (template_location);

			// iterate over each triple in the template
			parse_construct_triples_block (var_value_map);

			if (blank && update_blank_nodes != null) {
				update_blank_nodes.add_value (blank_nodes);
			}

			Data.update_buffer_might_flush ();
		}

		if (!data) {
			// reset location to the end of the update
			set_location (after_where);
		}

		// ensure possible WHERE clause in next part gets the correct results
		Data.update_buffer_flush ();
		bindings = null;

		context = context.parent_context;
	}

	internal string resolve_prefixed_name (string prefix, string local_name) throws Sparql.Error {
		string ns = prefix_map.lookup (prefix);
		if (ns == null) {
			throw get_error ("use of undefined prefix `%s'".printf (prefix));
		}
		return ns + local_name;
	}

	void skip_braces () throws Sparql.Error {
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

	void parse_construct_triples_block (HashTable<string,string> var_value_map) throws Sparql.Error, DateError {
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

	string? parse_construct_var_or_term (HashTable<string,string> var_value_map) throws Sparql.Error, DateError {
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

	void parse_construct_property_list_not_empty (HashTable<string,string> var_value_map) throws Sparql.Error, DateError {
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

	void parse_construct_object_list (HashTable<string,string> var_value_map) throws Sparql.Error, DateError {
		while (true) {
			parse_construct_object (var_value_map);
			if (accept (SparqlTokenType.COMMA)) {
				continue;
			}
			break;
		}
	}

	void parse_construct_object (HashTable<string,string> var_value_map) throws Sparql.Error, DateError {
		string object = parse_construct_var_or_term (var_value_map);
		if (current_subject == null || current_predicate == null || object == null) {
			// the SPARQL specification says that triples containing unbound variables
			// should be excluded from the output RDF graph of CONSTRUCT
			return;
		}
		try {
			if (update_statements) {
				// update triple in database
				Data.update_statement (current_graph, current_subject, current_predicate, object);
			} else if (delete_statements) {
				// delete triple from database
				Data.delete_statement (current_graph, current_subject, current_predicate, object);
			} else {
				// insert triple into database
				Data.insert_statement (current_graph, current_subject, current_predicate, object);
			}
		} catch (Sparql.Error e) {
			if (!silent) {
				throw e;
			}
		} catch (DateError e) {
			if (!silent) {
				throw new Sparql.Error.TYPE (e.message);
			}
		}
	}

	[CCode (cname = "uuid_generate")]
	public extern static void uuid_generate ([CCode (array_length = false)] uchar[] uuid);
}

