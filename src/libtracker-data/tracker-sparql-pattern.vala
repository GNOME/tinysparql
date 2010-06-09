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
	// Represents a variable used as a predicate
	class PredicateVariable : Object {
		public string? subject;
		public string? object;
		public bool return_graph;

		public Class? domain;

		public string get_sql_query (Query query) throws SparqlError {
			try {
				var sql = new StringBuilder ();

				if (subject != null) {
					// single subject
					var subject_id = Data.query_resource_id (subject);

					DBResultSet result_set = null;
					if (subject_id > 0) {
						var iface = DBManager.get_db_interface ();
						var stmt = iface.create_statement ("SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
						stmt.bind_int (0, subject_id);
						result_set = stmt.execute ();
					}

					if (result_set != null) {
						bool first = true;
						do {
							Value value;
							result_set._get_value (0, out value);
							var domain = Ontologies.get_class_by_uri (value.get_string ());

							foreach (Property prop in Ontologies.get_properties ()) {
								if (prop.domain == domain) {
									if (first) {
										first = false;
									} else {
										sql.append (" UNION ALL ");
									}
									sql.append_printf ("SELECT ID, (SELECT ID FROM Resource WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

									Expression.append_expression_as_string (sql, "\"%s\"".printf (prop.name), prop.data_type);

									sql.append (" AS \"object\"");
									if (return_graph) {
										sql.append_printf (", \"%s:graph\" AS \"graph\"", prop.name);
									}
									sql.append_printf (" FROM \"%s\"", prop.table_name);

									sql.append (" WHERE ID = ?");

									var binding = new LiteralBinding ();
									binding.literal = subject_id.to_string ();
									binding.data_type = PropertyType.INTEGER;
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
					var stmt = iface.create_statement ("SELECT (SELECT Uri FROM Resource WHERE ID = \"rdf:type\") FROM \"rdfs:Resource_rdf:type\" WHERE ID = ?");
					stmt.bind_int (0, object_id);
					var result_set = stmt.execute ();

					bool first = true;
					if (result_set != null) {
						do {
							Value value;
							result_set._get_value (0, out value);
							var range = Ontologies.get_class_by_uri (value.get_string ());

							foreach (Property prop in Ontologies.get_properties ()) {
								if (prop.range == range) {
									if (first) {
										first = false;
									} else {
										sql.append (" UNION ALL ");
									}
									sql.append_printf ("SELECT ID, (SELECT ID FROM Resource WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

									Expression.append_expression_as_string (sql, "\"%s\"".printf (prop.name), prop.data_type);

									sql.append (" AS \"object\"");
									if (return_graph) {
										sql.append_printf (", \"%s:graph\" AS \"graph\"", prop.name);
									}
									sql.append_printf (" FROM \"%s\"", prop.table_name);
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
					foreach (Property prop in Ontologies.get_properties ()) {
						if (prop.domain == domain) {
							if (first) {
								first = false;
							} else {
								sql.append (" UNION ALL ");
							}
							sql.append_printf ("SELECT ID, (SELECT ID FROM Resource WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

							Expression.append_expression_as_string (sql, "\"%s\"".printf (prop.name), prop.data_type);

							sql.append (" AS \"object\" FROM ");
							sql.append_printf ("\"%s\"", prop.table_name);
						}
					}
				} else {
					// UNION over all properties would exceed SQLite limits
					throw query.get_internal_error ("Unrestricted predicate variables not supported");
				}
				return sql.str;
			} catch (DBInterfaceError e) {
				throw new SparqlError.INTERNAL (e.message);
			}
		}
	}
}

class Tracker.Sparql.Pattern : Object {
	weak Query query;
	weak Expression expression;

	int counter;

	int next_table_index;

	string current_graph;
	bool current_graph_is_var;
	string current_subject;
	bool current_subject_is_var;
	string current_predicate;
	bool current_predicate_is_var;

	public Pattern (Query query) {
		this.query = query;
		this.expression = query.expression;
	}

	Context context {
		get { return query.context; }
		set { query.context = value; }
	}

	inline bool next () throws SparqlError {
		return query.next ();
	}

	inline SparqlTokenType current () {
		return query.current ();
	}

	inline bool accept (SparqlTokenType type) throws SparqlError {
		return query.accept (type);
	}

	SparqlError get_error (string msg) {
		return query.get_error (msg);
	}

	bool expect (SparqlTokenType type) throws SparqlError {
		return query.expect (type);
	}

	SourceLocation get_location () {
		return query.get_location ();
	}

	void set_location (SourceLocation location) {
		query.set_location (location);
	}

	string get_last_string (int strip = 0) {
		return query.get_last_string (strip);
	}

	class TripleContext : Context {
		// SQL tables
		public List<DataTable> tables;
		public HashTable<string,DataTable> table_map;
		// SPARQL literals
		public List<LiteralBinding> bindings;
		// SPARQL variables
		public List<Variable> variables;
		public HashTable<Variable,VariableBindingList> var_bindings;

		public TripleContext (Context parent_context) {
			base (parent_context);

			tables = new List<DataTable> ();
			table_map = new HashTable<string,DataTable>.full (str_hash, str_equal, g_free, g_object_unref);

			variables = new List<Variable> ();
			var_bindings = new HashTable<Variable,VariableBindingList>.full (direct_hash, direct_equal, g_object_unref, g_object_unref);

			bindings = new List<LiteralBinding> ();
		}
	}

	TripleContext? triple_context;

	internal SelectContext translate_select (StringBuilder sql, bool subquery = false, bool scalar_subquery = false) throws SparqlError {
		SelectContext result;
		if (scalar_subquery) {
			result = new SelectContext.subquery (context);
		} else {
			result = new SelectContext (context);
		}
		context = result;
		var type = PropertyType.UNKNOWN;

		var pattern_sql = new StringBuilder ();
		var old_bindings = (owned) query.bindings;

		sql.append ("SELECT ");

		expect (SparqlTokenType.SELECT);

		if (accept (SparqlTokenType.DISTINCT)) {
			sql.append ("DISTINCT ");
		} else if (accept (SparqlTokenType.REDUCED)) {
		}

		// skip select variables (processed later)
		var select_variables_location = get_location ();
		expression.skip_select_variables ();

		if (accept (SparqlTokenType.FROM)) {
			accept (SparqlTokenType.NAMED);
			expect (SparqlTokenType.IRI_REF);
		}

		accept (SparqlTokenType.WHERE);

		var pattern = translate_group_graph_pattern (pattern_sql);
		foreach (var key in pattern.var_set.get_keys ()) {
			context.var_set.insert (key, VariableState.BOUND);
		}

		// process select variables
		var after_where = get_location ();
		set_location (select_variables_location);

		// report use of undefined variables
		foreach (var variable in context.var_set.get_keys ()) {
			if (variable.binding == null) {
				throw get_error ("use of undefined variable `%s'".printf (variable.name));
			}
		}

		var where_bindings = (owned) query.bindings;
		query.bindings = (owned) old_bindings;

		bool first = true;
		if (accept (SparqlTokenType.STAR)) {
			foreach (var variable in context.var_set.get_keys ()) {
				if (!first) {
					sql.append (", ");
				} else {
					first = false;
				}
				if (subquery) {
					// don't convert to string in subqueries
					sql.append (variable.sql_expression);
				} else {
					Expression.append_expression_as_string (sql, variable.sql_expression, variable.binding.data_type);
				}
			}
		} else {
			while (true) {
				if (!first) {
					sql.append (", ");
				} else {
					first = false;
				}

				type = expression.translate_select_expression (sql, subquery);

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

		// literals in select expressions need to be bound before literals in the where clause
		foreach (var binding in where_bindings) {
			query.bindings.append (binding);
		}

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql.str);
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
				expression.translate_expression (sql);
			} while (current () != SparqlTokenType.ORDER && current () != SparqlTokenType.LIMIT && current () != SparqlTokenType.OFFSET && current () != SparqlTokenType.CLOSE_BRACE && current () != SparqlTokenType.CLOSE_PARENS && current () != SparqlTokenType.EOF);
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
				expression.translate_order_condition (sql);
			} while (current () != SparqlTokenType.LIMIT && current () != SparqlTokenType.OFFSET && current () != SparqlTokenType.CLOSE_BRACE && current () != SparqlTokenType.CLOSE_PARENS && current () != SparqlTokenType.EOF);
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
			binding.data_type = PropertyType.INTEGER;
			query.bindings.append (binding);

			if (offset >= 0) {
				sql.append (" OFFSET ?");

				binding = new LiteralBinding ();
				binding.literal = offset.to_string ();
				binding.data_type = PropertyType.INTEGER;
				query.bindings.append (binding);
			}
		} else if (offset >= 0) {
			sql.append (" LIMIT -1 OFFSET ?");

			var binding = new LiteralBinding ();
			binding.literal = offset.to_string ();
			binding.data_type = PropertyType.INTEGER;
			query.bindings.append (binding);
		}

		context = context.parent_context;

		result.type = type;

		return result;
	}

	internal string parse_var_or_term (StringBuilder? sql, out bool is_var) throws SparqlError {
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
			result = query.resolve_prefixed_name (ns, get_last_string ().substring (1));
		} else if (current () == SparqlTokenType.COLON) {
			// prefixed name without namespace :bar
			next ();
			result = query.resolve_prefixed_name ("", get_last_string ().substring (1));
		} else if (accept (SparqlTokenType.BLANK_NODE)) {
			// _:foo
			expect (SparqlTokenType.COLON);
			result = query.generate_bnodeid (get_last_string ().substring (1));
		} else if (current () == SparqlTokenType.STRING_LITERAL1) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL2) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG1) {
			result = expression.parse_string_literal ();
		} else if (current () == SparqlTokenType.STRING_LITERAL_LONG2) {
			result = expression.parse_string_literal ();
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
		} else if (current () == SparqlTokenType.OPEN_BRACKET) {
			next ();

			result = query.generate_bnodeid (null);

			string old_subject = current_subject;
			bool old_subject_is_var = current_subject_is_var;

			current_subject = result;
			current_subject_is_var = true;
			parse_property_list_not_empty (sql);
			expect (SparqlTokenType.CLOSE_BRACKET);

			current_subject = old_subject;
			current_subject_is_var = old_subject_is_var;

			is_var = true;
		} else {
			throw get_error ("expected variable or term");
		}
		return result;
	}

	void parse_object_list (StringBuilder sql, bool in_simple_optional = false) throws SparqlError {
		while (true) {
			parse_object (sql, in_simple_optional);
			if (accept (SparqlTokenType.COMMA)) {
				continue;
			}
			break;
		}
	}

	void parse_property_list_not_empty (StringBuilder sql, bool in_simple_optional = false) throws SparqlError {
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
				current_predicate = query.resolve_prefixed_name (ns, get_last_string ().substring (1));
			} else if (current () == SparqlTokenType.COLON) {
				next ();
				current_predicate = query.resolve_prefixed_name ("", get_last_string ().substring (1));
			} else if (current () == SparqlTokenType.A) {
				next ();
				current_predicate = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
			} else {
				throw get_error ("expected non-empty property list");
			}
			parse_object_list (sql, in_simple_optional);

			current_predicate = old_predicate;
			current_predicate_is_var = old_predicate_is_var;

			if (accept (SparqlTokenType.SEMICOLON)) {
				if (current () == SparqlTokenType.DOT) {
					// semicolon before dot is allowed in both, SPARQL and Turtle
					break;
				}
				continue;
			}
			break;
		}
	}

	void translate_filter (StringBuilder sql) throws SparqlError {
		expect (SparqlTokenType.FILTER);
		expression.translate_constraint (sql);
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
		case SparqlTokenType.ISBLANK:
		case SparqlTokenType.ISLITERAL:
		case SparqlTokenType.REGEX:
			next ();
			break;
		default:
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
				throw get_error ("unexpected end of query, expected )");
			} else {
				// ignore everything else
				next ();
			}
		}
	}

	void start_triples_block (StringBuilder sql) throws SparqlError {
		context = triple_context = new TripleContext (context);

		sql.append ("SELECT ");
	}

	void end_triples_block (StringBuilder sql, ref bool first_where, bool in_group_graph_pattern) throws SparqlError {
		// remove last comma and space
		sql.truncate (sql.len - 2);

		sql.append (" FROM ");
		bool first = true;
		foreach (DataTable table in triple_context.tables) {
			if (!first) {
				sql.append (", ");
			} else {
				first = false;
			}
			if (table.sql_db_tablename != null) {
				sql.append_printf ("\"%s\"", table.sql_db_tablename);
			} else {
				sql.append_printf ("(%s)", table.predicate_variable.get_sql_query (query));
			}
			sql.append_printf (" AS \"%s\"", table.sql_query_tablename);
		}

		foreach (var variable in triple_context.variables) {
			bool maybe_null = true;
			bool in_simple_optional = false;
			string last_name = null;
			foreach (VariableBinding binding in triple_context.var_bindings.lookup (variable).list) {
				string name;
				if (binding.table != null) {
					name = binding.sql_expression;
				} else {
					// simple optional with inverse functional property
					// always first in loop as variable is required to be unbound
					name = variable.sql_expression;
				}
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
				in_simple_optional = binding.in_simple_optional;
			}

			if (maybe_null && !in_simple_optional) {
				// ensure that variable is bound in case it could return NULL in SQL
				// assuming SPARQL variable is not optional
				if (!first_where) {
					sql.append (" AND ");
				} else {
					sql.append (" WHERE ");
					first_where = false;
				}
				sql.append_printf ("%s IS NOT NULL", variable.sql_expression);
			}
		}
		foreach (LiteralBinding binding in triple_context.bindings) {
			if (!first_where) {
				sql.append (" AND ");
			} else {
				sql.append (" WHERE ");
				first_where = false;
			}
			sql.append (binding.sql_expression);
			if (binding.is_fts_match) {
				// parameters do not work with fts MATCH
				string escaped_literal = string.joinv ("''", binding.literal.split ("'"));
				sql.append_printf (" MATCH '%s'", escaped_literal);
			} else {
				sql.append (" = ");
				if (binding.data_type == PropertyType.RESOURCE) {
					sql.append ("(SELECT ID FROM Resource WHERE Uri = ?)");
				} else {
					sql.append ("?");
				}
				query.bindings.append (binding);
			}
		}

		if (in_group_graph_pattern) {
			sql.append (")");
		}

		foreach (var v in context.var_set.get_keys ()) {
			context.parent_context.var_set.insert (v, VariableState.BOUND);
		}

		triple_context = null;
		context = context.parent_context;
	}

	void parse_triples (StringBuilder sql, long group_graph_pattern_start, ref bool in_triples_block, ref bool first_where, ref bool in_group_graph_pattern, bool found_simple_optional) throws SparqlError {
		while (true) {
			if (current () != SparqlTokenType.VAR &&
			    current () != SparqlTokenType.IRI_REF &&
			    current () != SparqlTokenType.PN_PREFIX &&
			    current () != SparqlTokenType.COLON &&
			    current () != SparqlTokenType.OPEN_BRACKET) {
				break;
			}
			if (in_triples_block && !in_group_graph_pattern && found_simple_optional) {
				// if there is a regular triple pattern after a simple optional
				// we need to use a separate triple block to avoid possible conflicts
				// due to not using a JOIN for the simple optional
				end_triples_block (sql, ref first_where, in_group_graph_pattern);
				in_triples_block = false;
			}
			if (!in_triples_block) {
				if (in_group_graph_pattern) {
					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					sql.append (") NATURAL INNER JOIN (");
				}
				in_triples_block = true;
				first_where = true;
				start_triples_block (sql);
			}

			current_subject = parse_var_or_term (sql, out current_subject_is_var);
			parse_property_list_not_empty (sql);

			if (!accept (SparqlTokenType.DOT)) {
				break;
			}
		}
	}

	bool is_subclass (Class class1, Class class2) {
		if (class1 == class2) {
			return true;
		}
		foreach (var superclass in class1.get_super_classes ()) {
			if (is_subclass (superclass, class2)) {
				return true;
			}
		}
		return false;
	}

	bool is_simple_optional () {
		var optional_start = get_location ();
		try {
			// check that we have { ?v foo:bar ?o }
			// where ?v is an already BOUND variable
			//       foo:bar is a single-valued property
			//               that is known to be in domain of ?v
			//       ?o has not been used before
			// or
			// where ?v has not been used before
			//       foo:bar is an inverse functional property
			//       ?o is an already ?BOUND variable

			expect (SparqlTokenType.OPEN_BRACE);

			// check subject
			if (!accept (SparqlTokenType.VAR)) {
				return false;
			}
			var left_variable = context.get_variable (get_last_string ().substring (1));
			var left_variable_state = context.var_set.lookup (left_variable);

			// check predicate
			string predicate;
			if (accept (SparqlTokenType.IRI_REF)) {
				predicate = get_last_string (1);
			} else if (accept (SparqlTokenType.PN_PREFIX)) {
				string ns = get_last_string ();
				expect (SparqlTokenType.COLON);
				predicate = query.resolve_prefixed_name (ns, get_last_string ().substring (1));
			} else if (accept (SparqlTokenType.COLON)) {
				predicate = query.resolve_prefixed_name ("", get_last_string ().substring (1));
			} else {
				return false;
			}
			var prop = Ontologies.get_property_by_uri (predicate);
			if (prop == null) {
				return false;
			}

			// check object
			if (!accept (SparqlTokenType.VAR)) {
				return false;
			}
			var right_variable = context.get_variable (get_last_string ().substring (1));
			var right_variable_state = context.var_set.lookup (right_variable);

			// optional .
			accept (SparqlTokenType.DOT);

			// check it is only one triple pattern
			if (!accept (SparqlTokenType.CLOSE_BRACE)) {
				return false;
			}

			if (left_variable_state == VariableState.BOUND && !prop.multiple_values && right_variable_state == 0) {
				bool in_domain = false;
				foreach (VariableBinding binding in triple_context.var_bindings.lookup (left_variable).list) {
					if (binding.type != null && is_subclass (binding.type, prop.domain)) {
						in_domain = true;
						break;
					}
				}

				if (in_domain) {
					// first valid case described in above comment
					return true;
				}
			} else if (left_variable_state == 0 && prop.is_inverse_functional_property && right_variable_state == VariableState.BOUND) {
				// second valid case described in above comment
				return true;
			}

			// no match
			return false;
		} catch (SparqlError e) {
			return false;
		} finally {
			// in any case, go back to the start of the optional
			set_location (optional_start);
		}
	}

	internal Context translate_group_graph_pattern (StringBuilder sql) throws SparqlError {
		expect (SparqlTokenType.OPEN_BRACE);

		if (current () == SparqlTokenType.SELECT) {
			var result = translate_select (sql, true);
			context = result;

			// only export selected variables
			context.var_set = context.select_var_set;
			context.select_var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);

			expect (SparqlTokenType.CLOSE_BRACE);

			context = context.parent_context;
			return result;
		}

		var result = new Context (context);
		context = result;

		SourceLocation[] filters = { };

		bool in_triples_block = false;
		bool in_group_graph_pattern = false;
		bool first_where = true;
		bool found_simple_optional = false;
		long group_graph_pattern_start = sql.len;

		// optional TriplesBlock
		parse_triples (sql, group_graph_pattern_start, ref in_triples_block, ref first_where, ref in_group_graph_pattern, found_simple_optional);

		while (true) {
			// check whether we have GraphPatternNotTriples | Filter
			if (accept (SparqlTokenType.OPTIONAL)) {
				if (!in_group_graph_pattern && is_simple_optional ()) {
					// perform join-less optional (like non-optional except for the IS NOT NULL check)
					found_simple_optional = true;
					expect (SparqlTokenType.OPEN_BRACE);

					current_subject = parse_var_or_term (sql, out current_subject_is_var);
					parse_property_list_not_empty (sql, true);

					accept (SparqlTokenType.DOT);
					expect (SparqlTokenType.CLOSE_BRACE);
				} else {
					if (!in_triples_block && !in_group_graph_pattern) {
						// expand { OPTIONAL { ... } } into { { } OPTIONAL { ... } }
						// empty graph pattern => return one result without bound variables
						sql.append ("SELECT 1");
					} else if (in_triples_block) {
						end_triples_block (sql, ref first_where, in_group_graph_pattern);
						in_triples_block = false;
					}
					if (!in_group_graph_pattern) {
						in_group_graph_pattern = true;
					}

					var select = new StringBuilder ("SELECT ");

					int left_index = ++next_table_index;
					int right_index = ++next_table_index;

					sql.append_printf (") AS t%d_g LEFT JOIN (", left_index);

					context = translate_group_graph_pattern (sql);

					sql.append_printf (") AS t%d_g", right_index);

					bool first = true;
					bool first_common = true;
					foreach (var v in context.var_set.get_keys ()) {
						if (first) {
							first = false;
						} else {
							select.append (", ");
						}

						var old_state = context.parent_context.var_set.lookup (v);
						if (old_state == 0) {
							// first used in optional part
							context.parent_context.var_set.insert (v, VariableState.OPTIONAL);
							select.append_printf ("t%d_g.%s", right_index, v.sql_expression);
						} else {
							if (first_common) {
								sql.append (" ON ");
								first_common = false;
							} else {
								sql.append (" AND ");
							}

							if (old_state == VariableState.BOUND) {
								// variable definitely bound in non-optional part
								sql.append_printf ("t%d_g.%s = t%d_g.%s", left_index, v.sql_expression, right_index, v.sql_expression);
								select.append_printf ("t%d_g.%s", left_index, v.sql_expression);
							} else if (old_state == VariableState.OPTIONAL) {
								// variable maybe bound in non-optional part
								sql.append_printf ("(t%d_g.%s IS NULL OR t%d_g.%s = t%d_g.%s)", left_index, v.sql_expression, left_index, v.sql_expression, right_index, v.sql_expression);
								select.append_printf ("COALESCE (t%d_g.%s, t%d_g.%s) AS %s", left_index, v.sql_expression, right_index, v.sql_expression, v.sql_expression);
							}
						}
					}
					foreach (var v in context.parent_context.var_set.get_keys ()) {
						if (context.var_set.lookup (v) == 0) {
							// only used in non-optional part
							if (first) {
								first = false;
							} else {
								select.append (", ");
							}

							select.append_printf ("t%d_g.%s", left_index, v.sql_expression);
						}
					}
					if (first) {
						// no variables used at all
						select.append ("1");
					}

					context = context.parent_context;

					select.append (" FROM (");
					sql.insert (group_graph_pattern_start, select.str);

					// surround with SELECT * FROM (...) to avoid ambiguous column names
					// in SQL generated for FILTER (triggered by using table aliases for join sources)
					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					sql.append (")");
				}
			} else if (accept (SparqlTokenType.GRAPH)) {
				var old_graph = current_graph;
				var old_graph_is_var = current_graph_is_var;
				current_graph = parse_var_or_term (sql, out current_graph_is_var);

				if (!in_triples_block && !in_group_graph_pattern) {
					in_group_graph_pattern = true;

					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					translate_group_or_union_graph_pattern (sql);
					sql.append (")");
				} else {
					if (in_triples_block) {
						end_triples_block (sql, ref first_where, in_group_graph_pattern);
						in_triples_block = false;
					}
					if (!in_group_graph_pattern) {
						in_group_graph_pattern = true;
					}

					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					sql.append (") NATURAL INNER JOIN (");
					translate_group_or_union_graph_pattern (sql);
					sql.append (")");
				}

				current_graph = old_graph;
				current_graph_is_var = old_graph_is_var;
			} else if (current () == SparqlTokenType.OPEN_BRACE) {
				if (!in_triples_block && !in_group_graph_pattern) {
					in_group_graph_pattern = true;

					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					translate_group_or_union_graph_pattern (sql);
					sql.append (")");
				} else {
					if (in_triples_block) {
						end_triples_block (sql, ref first_where, in_group_graph_pattern);
						in_triples_block = false;
					}
					if (!in_group_graph_pattern) {
						in_group_graph_pattern = true;
					}

					sql.insert (group_graph_pattern_start, "SELECT * FROM (");
					sql.append (") NATURAL INNER JOIN (");
					translate_group_or_union_graph_pattern (sql);
					sql.append (")");
				}
			} else if (current () == SparqlTokenType.FILTER) {
				filters += get_location ();
				skip_filter ();
			} else {
				break;
			}

			accept (SparqlTokenType.DOT);

			// optional TriplesBlock
			parse_triples (sql, group_graph_pattern_start, ref in_triples_block, ref first_where, ref in_group_graph_pattern, found_simple_optional);
		}

		expect (SparqlTokenType.CLOSE_BRACE);

		if (!in_triples_block && !in_group_graph_pattern) {
			// empty graph pattern => return one result without bound variables
			sql.append ("SELECT 1");
		} else if (in_triples_block) {
			end_triples_block (sql, ref first_where, in_group_graph_pattern);
			in_triples_block = false;
		}

		if (in_group_graph_pattern) {
			first_where = true;
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
				translate_filter (sql);
			}

			set_location (end);
		}

		context = context.parent_context;
		return result;
	}

	void translate_group_or_union_graph_pattern (StringBuilder sql) throws SparqlError {
		Variable[] all_vars = { };
		HashTable<Variable,int> all_var_set = new HashTable<Variable,int>.full (direct_hash, direct_equal, g_object_unref, null);

		Context[] contexts = { };
		long[] offsets = { };

		do {
			offsets += sql.len;
			contexts += translate_group_graph_pattern (sql);
		} while (accept (SparqlTokenType.UNION));

		if (contexts.length > 1) {
			// union graph pattern

			// create union of all variables
			foreach (var sub_context in contexts) {
				foreach (var v in sub_context.var_set.get_keys ()) {
					if (all_var_set.lookup (v) == 0) {
						all_vars += v;
						all_var_set.insert (v, VariableState.BOUND);
						context.var_set.insert (v, VariableState.BOUND);
					}
				}
			}

			long extra_offset = 0;
			for (int i = 0; i < contexts.length; i++) {
				var projection = new StringBuilder ();
				if (i > 0) {
					projection.append (") UNION ALL ");
				}
				projection.append ("SELECT ");
				foreach (var v in all_vars) {
					if (contexts[i].var_set.lookup (v) == 0) {
						// variable not used in this subgraph
						// use NULL
						projection.append ("NULL AS ");
					}
					projection.append_printf ("%s, ", v.sql_expression);
				}
				// delete last comma and space
				projection.truncate (projection.len - 2);
				projection.append (" FROM (");

				sql.insert (offsets[i] + extra_offset, projection.str);
				extra_offset += projection.len;
			}
			sql.append (")");
		} else {
			foreach (var key in contexts[0].var_set.get_keys ()) {
				context.var_set.insert (key, VariableState.BOUND);
			}
		}
	}

	VariableBindingList? get_variable_binding_list (Variable variable) {
		VariableBindingList binding_list = null;
		if (triple_context != null) {
			binding_list = triple_context.var_bindings.lookup (variable);
		}
		if (binding_list == null && variable.binding != null) {
			// might be in scalar subquery: check variables of outer queries
			var current_context = context;
			while (current_context != null) {
				// only allow access to variables of immediate parent context of the subquery
				// allowing access to other variables leads to invalid SQL or wrong results
				if (current_context.scalar_subquery && current_context.parent_context.var_set.lookup (variable) != 0) {
					// capture outer variable
					var binding = new VariableBinding ();
					binding.data_type = variable.binding.data_type;
					binding.variable = context.get_variable (variable.name);
					binding.type = variable.binding.type;
					binding.sql_expression = variable.sql_expression;
					binding_list = new VariableBindingList ();
					if (triple_context != null) {
						triple_context.variables.append (variable);
						triple_context.var_bindings.insert (variable, binding_list);
					}

					context.var_set.insert (variable, VariableState.BOUND);
					binding_list.list.append (binding);
					break;
				}
				current_context = current_context.parent_context;
			}
		}
		return binding_list;
	}

	internal void add_variable_binding (StringBuilder sql, VariableBinding binding, VariableState variable_state) {
		var binding_list = get_variable_binding_list (binding.variable);
		if (binding_list == null) {
			binding_list = new VariableBindingList ();
			if (triple_context != null) {
				triple_context.variables.append (binding.variable);
				triple_context.var_bindings.insert (binding.variable, binding_list);
			}

			sql.append_printf ("%s AS %s, ",
				binding.sql_expression,
				binding.variable.sql_expression);

			if (binding.data_type == PropertyType.DATETIME) {
				sql.append_printf ("%s AS %s, ",
					binding.get_extra_sql_expression ("localDate"),
					binding.variable.get_extra_sql_expression ("localDate"));
				sql.append_printf ("%s AS %s, ",
					binding.get_extra_sql_expression ("localTime"),
					binding.variable.get_extra_sql_expression ("localTime"));
			}

			context.var_set.insert (binding.variable, variable_state);
		}
		binding_list.list.append (binding);
		if (binding.variable.binding == null) {
			binding.variable.binding = binding;
		}
	}

	void parse_object (StringBuilder sql, bool in_simple_optional = false) throws SparqlError {
		bool object_is_var;
		string object = parse_var_or_term (sql, out object_is_var);

		string db_table;
		bool rdftype = false;
		bool share_table = true;
		bool is_fts_match = false;

		bool newtable;
		DataTable table;
		Property prop = null;

		Class subject_type = null;

		if (!current_predicate_is_var) {
			prop = Ontologies.get_property_by_uri (current_predicate);

			if (current_predicate == "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
			    && !object_is_var) {
				// rdf:type query
				rdftype = true;
				var cl = Ontologies.get_class_by_uri (object);
				if (cl == null) {
					throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (object));
				}
				db_table = cl.name;
				subject_type = cl;
			} else if (prop == null) {
				if (current_predicate == "http://www.tracker-project.org/ontologies/fts#match") {
					// fts:match
					db_table = "fts";
					share_table = false;
					is_fts_match = true;
				} else {
					throw new SparqlError.UNKNOWN_PROPERTY ("Unknown property `%s'".printf (current_predicate));
				}
			} else {
				if (current_predicate == "http://www.w3.org/2000/01/rdf-schema#domain"
				    && current_subject_is_var
				    && !object_is_var) {
					// rdfs:domain
					var domain = Ontologies.get_class_by_uri (object);
					if (domain == null) {
						throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (object));
					}
					var pv = context.predicate_variable_map.lookup (context.get_variable (current_subject));
					if (pv == null) {
						pv = new PredicateVariable ();
						context.predicate_variable_map.insert (context.get_variable (current_subject), pv);
					}
					pv.domain = domain;
				}

				db_table = prop.table_name;
				if (prop.multiple_values) {
					// we can never share the table with multiple triples
					// for multi value properties as a property may consist of multiple rows
					share_table = false;
				}
				subject_type = prop.domain;

				if (in_simple_optional && context.var_set.lookup (context.get_variable (current_subject)) == 0) {
					// use subselect instead of join in simple optional where the subject is the unbound variable
					// this can only happen with inverse functional properties
					var binding = new VariableBinding ();
					binding.data_type = PropertyType.RESOURCE;
					binding.variable = context.get_variable (current_subject);

					assert (triple_context.var_bindings.lookup (binding.variable) == null);
					var binding_list = new VariableBindingList ();
					triple_context.variables.append (binding.variable);
					triple_context.var_bindings.insert (binding.variable, binding_list);

					// need to use table and column name for object, can't refer to variable in nested select
					var object_binding = triple_context.var_bindings.lookup (context.get_variable (object)).list.data;

					sql.append_printf ("(SELECT ID FROM \"%s\" WHERE \"%s\" = %s) AS %s, ",
						db_table,
						prop.name,
						object_binding.sql_expression,
						binding.variable.sql_expression);

					context.var_set.insert (binding.variable, VariableState.OPTIONAL);
					binding_list.list.append (binding);

					assert (binding.variable.binding == null);
					binding.variable.binding = binding;

					return;
				}
			}
			table = get_table (current_subject, db_table, share_table, out newtable);
		} else {
			// variable in predicate
			newtable = true;
			table = new DataTable ();
			table.predicate_variable = context.predicate_variable_map.lookup (context.get_variable (current_predicate));
			if (table.predicate_variable == null) {
				table.predicate_variable = new PredicateVariable ();
				context.predicate_variable_map.insert (context.get_variable (current_predicate), table.predicate_variable);
			}
			if (!current_subject_is_var) {
				// single subject
				table.predicate_variable.subject = current_subject;
			}
			if (!object_is_var) {
				// single object
				table.predicate_variable.object = object;
			}
			if (current_graph != null) {
				table.predicate_variable.return_graph = true;
			}
			table.sql_query_tablename = current_predicate + (++counter).to_string ();
			triple_context.tables.append (table);

			// add to variable list
			var binding = new VariableBinding ();
			binding.data_type = PropertyType.RESOURCE;
			binding.variable = context.get_variable (current_predicate);
			binding.table = table;
			binding.sql_db_column_name = "predicate";

			add_variable_binding (sql, binding, VariableState.BOUND);
		}

		if (newtable) {
			if (current_subject_is_var) {
				var binding = new VariableBinding ();
				binding.data_type = PropertyType.RESOURCE;
				binding.variable = context.get_variable (current_subject);
				binding.table = table;
				binding.type = subject_type;
				if (is_fts_match) {
					binding.sql_db_column_name = "rowid";
				} else {
					binding.sql_db_column_name = "ID";
				}

				add_variable_binding (sql, binding, VariableState.BOUND);
			} else {
				var binding = new LiteralBinding ();
				binding.data_type = PropertyType.RESOURCE;
				binding.literal = current_subject;
				// binding.data_type = triple.subject.type;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				triple_context.bindings.append (binding);
			}
		}

		if (!rdftype) {
			if (object_is_var) {
				var binding = new VariableBinding ();
				binding.variable = context.get_variable (object);
				binding.table = table;
				if (prop != null) {

					binding.type = prop.range;

					binding.data_type = prop.data_type;
					binding.sql_db_column_name = prop.name;
					if (!prop.multiple_values) {
						// for single value properties, row may have NULL
						// in any column except the ID column
						binding.maybe_null = true;
						binding.in_simple_optional = in_simple_optional;
					}
				} else {
					// variable as predicate
					binding.sql_db_column_name = "object";
					binding.maybe_null = true;
				}

				VariableState state;
				if (in_simple_optional) {
					state = VariableState.OPTIONAL;
				} else {
					state = VariableState.BOUND;
				}

				add_variable_binding (sql, binding, state);
			} else if (is_fts_match) {
				var binding = new LiteralBinding ();
				binding.is_fts_match = true;
				binding.literal = object;
				// binding.data_type = triple.object.type;
				binding.table = table;
				binding.sql_db_column_name = "fts";
				triple_context.bindings.append (binding);

				sql.append_printf ("rank(\"%s\".\"fts\") AS \"%s_u_rank\", ",
					binding.table.sql_query_tablename,
					context.get_variable (current_subject).name);
				sql.append_printf ("offsets(\"%s\".\"fts\") AS \"%s_u_offsets\", ",
					binding.table.sql_query_tablename,
					context.get_variable (current_subject).name);
			} else {
				var binding = new LiteralBinding ();
				binding.literal = object;
				// binding.data_type = triple.object.type;
				binding.table = table;
				if (prop != null) {
					binding.data_type = prop.data_type;
					binding.sql_db_column_name = prop.name;
				} else {
					// variable as predicate
					binding.sql_db_column_name = "object";
				}
				triple_context.bindings.append (binding);
			}

			if (current_graph != null) {
				if (current_graph_is_var) {
					var binding = new VariableBinding ();
					binding.variable = context.get_variable (current_graph);
					binding.table = table;
					binding.data_type = PropertyType.RESOURCE;

					if (prop != null) {
						binding.sql_db_column_name = prop.name + ":graph";
					} else {
						// variable as predicate
						binding.sql_db_column_name = "graph";
					}

					binding.maybe_null = true;
					binding.in_simple_optional = in_simple_optional;

					VariableState state;
					if (in_simple_optional) {
						state = VariableState.OPTIONAL;
					} else {
						state = VariableState.BOUND;
					}

					add_variable_binding (sql, binding, state);
				} else {
					var binding = new LiteralBinding ();
					binding.literal = current_graph;
					binding.table = table;
					binding.data_type = PropertyType.RESOURCE;

					if (prop != null) {
						binding.sql_db_column_name = prop.name + ":graph";
					} else {
						// variable as predicate
						binding.sql_db_column_name = "graph";
					}

					triple_context.bindings.append (binding);
				}
			}
		}

		if (!current_subject_is_var &&
		    !current_predicate_is_var &&
		    !object_is_var) {
			// no variables involved, add dummy expression to SQL
			sql.append ("1, ");
		}
	}

	DataTable get_table (string subject, string db_table, bool share_table, out bool newtable) {
		string tablestring = "%s.%s".printf (subject, db_table);
		DataTable table = null;
		newtable = false;
		if (share_table) {
			table = triple_context.table_map.lookup (tablestring);
		}
		if (table == null) {
			newtable = true;
			table = new DataTable ();
			table.sql_db_tablename = db_table;
			table.sql_query_tablename = db_table + (++counter).to_string ();
			triple_context.tables.append (table);
			triple_context.table_map.insert (tablestring, table);
		}
		return table;
	}
}
