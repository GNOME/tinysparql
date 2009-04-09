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
	INTERNAL
}

public class Tracker.SparqlQuery : Object {
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
		public Rasqal.Literal.Type literal_type;
	}

	// Represents a mapping of a SPARQL variable to a SQL table and column
	class VariableBinding : DataBinding {
		public string variable;
		// Specified whether SQL column may contain NULL entries
		public bool maybe_null;
	}

	class VariableBindingList : Object {
		public List<VariableBinding> list;
	}

	// Represents a variable used as a predicate
	class PredicateVariable : Object {
		public string? subject;
		public string? object;

		public Class? domain;

		public string get_sql_query () throws Error {
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
									sql.append (" UNION ");
								}
								sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

								if (prop.data_type == PropertyType.RESOURCE) {
									sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
								} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
									sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
								} else if (prop.data_type == PropertyType.BOOLEAN) {
									sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
								} else if (prop.data_type == PropertyType.DATETIME) {
									sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%S\", \"%s\", \"unixepoch\")", prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.name);
								}

								sql.append (" AS \"object\" FROM ");
								if (prop.multiple_values) {
									sql.append_printf ("\"%s_%s\"", prop.domain.name, prop.name);
								} else {
									sql.append_printf ("\"%s\"", prop.domain.name);
								}

								sql.append_printf (" WHERE ID = %d", subject_id);
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
									sql.append (" UNION ");
								}
								sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

								if (prop.data_type == PropertyType.RESOURCE) {
									sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
								} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
									sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
								} else if (prop.data_type == PropertyType.BOOLEAN) {
									sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
								} else if (prop.data_type == PropertyType.DATETIME) {
									sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%S\", \"%s\", \"unixepoch\")", prop.name);
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
				}
			} else if (domain != null) {
				// any subject, predicates limited to a specific domain
				bool first = true;
				foreach (Property prop in Ontology.get_properties ()) {
					if (prop.domain == domain) {
						if (first) {
							first = false;
						} else {
							sql.append (" UNION ");
						}
						sql.append_printf ("SELECT ID, (SELECT ID FROM \"rdfs:Resource\" WHERE Uri = '%s') AS \"predicate\", ", prop.uri);

						if (prop.data_type == PropertyType.RESOURCE) {
							sql.append_printf ("(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")", prop.name);
						} else if (prop.data_type == PropertyType.INTEGER || prop.data_type == PropertyType.DOUBLE) {
							sql.append_printf ("CAST (\"%s\" AS TEXT)", prop.name);
						} else if (prop.data_type == PropertyType.BOOLEAN) {
							sql.append_printf ("CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END", prop.name);
						} else if (prop.data_type == PropertyType.DATETIME) {
							sql.append_printf ("strftime (\"%%Y-%%m-%%dT%%H:%%M:%%S\", \"%s\", \"unixepoch\")", prop.name);
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

	string query_string;
	bool update_extensions;

	StringBuilder pattern_sql = new StringBuilder ();

	// All SQL tables
	List<DataTable> tables;
	HashTable<string,DataTable> table_map;

	// All SPARQL literals
	List<LiteralBinding> bindings;
	List<LiteralBinding> pattern_bindings;

	// All SPARQL variables
	HashTable<string,VariableBinding> var_map = new HashTable<string,VariableBinding>.full (str_hash, str_equal, g_free, g_object_unref);
	List<string> pattern_variables;
	HashTable<string,VariableBindingList> pattern_var_map;

	// Variables used as predicates
	HashTable<string,PredicateVariable> predicate_variable_map = new HashTable<string,VariableBinding>.full (str_hash, str_equal, g_free, g_object_unref);

	int counter;

	int bnodeid = 0;
	// base UUID used for blank nodes
	uchar[] base_uuid;

	string error_message;

	public SparqlQuery (string query) {
		this.query_string = query;
	}

	public SparqlQuery.update (string query) {
		this (query);
		this.update_extensions = true;
	}

	string get_sql_for_literal (Rasqal.Literal literal) {
		assert (literal.type == Rasqal.Literal.Type.VARIABLE);

		string variable_name = literal.as_variable ().name;

		return "\"%s\"".printf (variable_name);
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

	string generate_bnodeid_handler (Rasqal.Query? query, string? user_bnodeid) {
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

	void error_handler (Raptor.Locator? locator, string message) {
		if (error_message == null) {
			// return first, not last, error message
			error_message = message;
		}
	}

	public DBResultSet? execute () throws Error {
		var world = new Rasqal.World ();
		world.open ();

		// use LAQRS - extension to SPARQL - to support aggregation
		var query = new Rasqal.Query (world, "laqrs", null);

		foreach (Namespace ns in Ontology.get_namespaces ()) {
			query.add_prefix (new Rasqal.Prefix (world, ns.prefix, ns.uri));
		}

		query.declare_prefixes ();

		base_uuid = new uchar[16];
		uuid_generate (base_uuid);
		query.set_generate_bnodeid_handler (generate_bnodeid_handler);

		query.set_warning_handler (error_handler);
		query.set_error_handler (error_handler);
		query.set_fatal_error_handler (error_handler);

		query.prepare (this.query_string, null);
		if (error_message != null) {
			throw new SparqlError.PARSE (error_message);
		}

		if (!update_extensions) {
			if (query.get_verb () == Rasqal.QueryVerb.SELECT) {
				return execute_select (query);
			} else if (query.get_verb () == Rasqal.QueryVerb.CONSTRUCT) {
				throw new SparqlError.INTERNAL ("CONSTRUCT is not supported");
			} else if (query.get_verb () == Rasqal.QueryVerb.DESCRIBE) {
				throw new SparqlError.INTERNAL ("DESCRIBE is not supported");
			} else if (query.get_verb () == Rasqal.QueryVerb.ASK) {
				throw new SparqlError.INTERNAL ("ASK is not supported");
			} else {
				throw new SparqlError.PARSE ("DELETE and INSERT are not supported in query mode");
			}
		} else {
			if (query.get_verb () == Rasqal.QueryVerb.INSERT) {
				execute_insert (query);
				return null;
			} else if (query.get_verb () == Rasqal.QueryVerb.DELETE) {
				execute_delete (query);
				return null;
			} else {
				throw new SparqlError.PARSE ("SELECT, CONSTRUCT, DESCRIBE, and ASK are not supported in update mode");
			}
		}
	}

	string get_sql_for_variable (string variable_name) {
		var binding = var_map.lookup (variable_name);
		assert (binding != null);
		if (binding.is_uri) {
			return "(SELECT Uri FROM \"rdfs:Resource\" WHERE ID = \"%s\")".printf (variable_name);
		} else if (binding.is_boolean) {
			return "(CASE \"%s\" WHEN 1 THEN 'true' WHEN 0 THEN 'false' ELSE NULL END)".printf (variable_name);
		} else if (binding.is_datetime) {
			return "strftime (\"%%Y-%%m-%%dT%%H:%%M:%%S\", \"%s\", \"unixepoch\")".printf (variable_name);
		} else {
			return "\"%s\"".printf (variable_name);
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
			} else if (binding.literal_type == Rasqal.Literal.Type.INTEGER) {
				stmt.bind_int (i, binding.literal.to_int ());
			} else {
				stmt.bind_text (i, binding.literal);
			}
			i++;
		}

		return stmt.execute ();
	}

	DBResultSet? execute_select (Rasqal.Query query) throws Error {
		// SELECT query

		// process WHERE clause
		visit_graph_pattern (query.get_query_graph_pattern ());

		// build SQL
		var sql = new StringBuilder ();

		sql.append ("SELECT ");
		if (query.get_distinct ()) {
			sql.append ("DISTINCT ");
		}
		bool first = true;
		for (int var_idx = 0; true; var_idx++) {
			weak Rasqal.Variable variable = query.get_variable (var_idx);
			if (variable == null) {
				break;
			}

			if (!first) {
				sql.append (", ");
			} else {
				first = false;
			}
			
			if (variable.expression != null) {
				// LAQRS aggregate expression
				sql.append (get_sql_for_expression (variable.expression));
			} else {
				sql.append (get_sql_for_variable (variable.name));
			}
		}

		// select from results of WHERE clause
		sql.append (" FROM (");
		sql.append (pattern_sql.str);
		sql.append (")");

		// GROUP BY (SPARQL extension, LAQRS)
		first = true;
		for (int group_idx = 0; true; group_idx++) {
			weak Rasqal.Expression group = query.get_group_condition (group_idx);
			if (group == null) {
				break;
			}

			if (!first) {
				sql.append (", ");
			} else {
				sql.append (" GROUP BY ");
				first = false;
			}
			assert (group.op == Rasqal.Op.GROUP_COND_ASC || group.op == Rasqal.Op.GROUP_COND_DESC);
			assert (group.arg1.op == Rasqal.Op.LITERAL);
			assert (group.arg1.literal.type == Rasqal.Literal.Type.VARIABLE);
			string variable_name = group.arg1.literal.as_variable ().name;

			sql.append (get_sql_for_variable (variable_name));

			if (group.op == Rasqal.Op.GROUP_COND_DESC) {
				sql.append (" DESC");
			}
		}

		// ORDER BY
		first = true;
		for (int order_idx = 0; true; order_idx++) {
			weak Rasqal.Expression order = query.get_order_condition (order_idx);
			if (order == null) {
				break;
			}

			if (!first) {
				sql.append (", ");
			} else {
				sql.append (" ORDER BY ");
				first = false;
			}
			assert (order.op == Rasqal.Op.ORDER_COND_ASC || order.op == Rasqal.Op.ORDER_COND_DESC);
			assert (order.arg1.op == Rasqal.Op.LITERAL);
			assert (order.arg1.literal.type == Rasqal.Literal.Type.VARIABLE);
			string variable_name = order.arg1.literal.as_variable ().name;

			sql.append (get_sql_for_variable (variable_name));

			if (order.op == Rasqal.Op.ORDER_COND_DESC) {
				sql.append (" DESC");
			}
		}

		// LIMIT and OFFSET
		if (query.get_limit () >= 0) {
			sql.append_printf (" LIMIT %d", query.get_limit ());
			if (query.get_offset () >= 0) {
				sql.append_printf (" OFFSET %d", query.get_offset ());
			}
		}

		return exec_sql (sql.str);
	}

	void execute_insert (Rasqal.Query query) throws Error {
		execute_update (query, false);
	}

	void execute_delete (Rasqal.Query query) throws Error {
		execute_update (query, true);
	}

	void execute_update (Rasqal.Query query, bool delete_statements) throws Error {
		// INSERT or DELETE

		var sql = new StringBuilder ();

		// process WHERE clause
		if (query.get_query_graph_pattern () != null) {
			visit_graph_pattern (query.get_query_graph_pattern ());

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

			// select from results of WHERE clause
			sql.append (" FROM (");
			sql.append (pattern_sql.str);
			sql.append (")");
		} else {
			sql.append ("SELECT 1");
		}

		var result_set = exec_sql (sql.str);

		// all updates should be committed in one transaction
		Data.begin_transaction ();

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

				// iterate over each triple in the template
				for (int triple_idx = 0; true; triple_idx++) {
					weak Rasqal.Triple triple = query.get_construct_triple (triple_idx);
					if (triple == null) {
						break;
					}
					
					string subject, predicate, object;

					if (triple.subject.type == Rasqal.Literal.Type.VARIABLE) {
						subject = var_value_map.lookup (triple.subject.as_variable ().name);
					} else {
						subject = get_string_from_literal (triple.subject);
					}

					if (triple.predicate.type == Rasqal.Literal.Type.VARIABLE) {
						predicate = var_value_map.lookup (triple.predicate.as_variable ().name);
					} else {
						predicate = get_string_from_literal (triple.predicate);
					}

					if (triple.object.type == Rasqal.Literal.Type.VARIABLE) {
						object = var_value_map.lookup (triple.object.as_variable ().name);
					} else {
						object = get_string_from_literal (triple.object);
					}

					if (delete_statements) {
						// delete triple from database
						Data.delete_statement (subject, predicate, object);
					} else {
						// insert triple into database
						Data.insert_statement (subject, predicate, object);
					}
				}
			} while (result_set.iter_next ());
		}

		Data.commit_transaction ();
	}

	void visit_graph_pattern (Rasqal.GraphPattern graph_pattern) throws SparqlError {
		bool first_where = true;
		if (graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.BASIC) {
			tables = new List<DataTable> ();
			table_map = new HashTable<string,DataTable>.full (str_hash, str_equal, g_free, g_object_unref);

			pattern_variables = new List<string> ();
			pattern_var_map = new HashTable<string,VariableBindingList>.full (str_hash, str_equal, g_free, g_object_unref);

			pattern_bindings = new List<LiteralBinding> ();

			pattern_sql.append ("SELECT ");

			for (int triple_idx = 0; true; triple_idx++) {
				weak Rasqal.Triple triple = graph_pattern.get_triple (triple_idx);
				if (triple == null) {
					break;
				}

				visit_triple (triple);
			}

			// remove last comma and space
			pattern_sql.truncate (pattern_sql.len - 2);

			pattern_sql.append (" FROM ");
			bool first = true;
			foreach (DataTable table in tables) {
				if (!first) {
					pattern_sql.append (", ");
				} else {
					first = false;
				}
				if (table.sql_db_tablename != null) {
					pattern_sql.append_printf ("\"%s\"", table.sql_db_tablename);
				} else {
					pattern_sql.append_printf ("(%s)", table.predicate_variable.get_sql_query ());
				}
				pattern_sql.append_printf (" AS \"%s\"", table.sql_query_tablename);
			}

			foreach (string variable in pattern_variables) {
				bool maybe_null = true;
				string last_name = null;
				foreach (VariableBinding binding in pattern_var_map.lookup (variable).list) {
					string name = "\"%s\".\"%s\"".printf (binding.table.sql_query_tablename, binding.sql_db_column_name);
					if (last_name != null) {
						if (!first_where) {
							pattern_sql.append (" AND ");
						} else {
							pattern_sql.append (" WHERE ");
							first_where = false;
						}
						pattern_sql.append (last_name);
						pattern_sql.append (" = ");
						pattern_sql.append (name);
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
						pattern_sql.append (" AND ");
					} else {
						pattern_sql.append (" WHERE ");
						first_where = false;
					}
					pattern_sql.append_printf ("%s IS NOT NULL", variable);
				}
			}
			foreach (LiteralBinding binding in pattern_bindings) {
				if (!first_where) {
					pattern_sql.append (" AND ");
				} else {
					pattern_sql.append (" WHERE ");
					first_where = false;
				}
				pattern_sql.append ("\"");
				pattern_sql.append (binding.table.sql_query_tablename);
				pattern_sql.append ("\".\"");
				pattern_sql.append (binding.sql_db_column_name);
				pattern_sql.append ("\"");
				if (binding.is_fts_match) {
					pattern_sql.append (" IN (");

					// include matches from fulltext search
					first = true;
					foreach (int match_id in Data.search_get_matches (binding.literal)) {
						if (!first) {
							pattern_sql.append (",");
						} else {
							first = false;
						}
						pattern_sql.append_printf ("%d", match_id);
					}

					pattern_sql.append (")");
				} else {
					pattern_sql.append (" = ");
					if (binding.is_uri) {
						pattern_sql.append ("(SELECT ID FROM \"rdfs:Resource\" WHERE Uri = ?)");
					} else {
						pattern_sql.append ("?");
					}
				}
			}

			tables = null;
			table_map = null;
			pattern_variables = null;
			pattern_var_map = null;
			pattern_bindings = null;
		} else if (graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.GROUP
		           || graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.OPTIONAL) {
			pattern_sql.append ("SELECT * FROM (");
			long subgraph_start = pattern_sql.len;
			int subgraph_idx = 0;
			for (int pattern_idx = 0; true; pattern_idx++) {
				weak Rasqal.GraphPattern sub_graph_pattern = graph_pattern.get_sub_graph_pattern (pattern_idx);
				if (sub_graph_pattern == null) {
					break;
				}

				if (sub_graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.FILTER) {
					// ignore filters, processed later
					continue;
				}

				if (subgraph_idx > 1) {
					// additional (SELECT * FROM ...) necessary
					// when using more than two subgraphs to
					// work around SQLite bug with NATURAL JOINs
					pattern_sql.insert (subgraph_start, "(SELECT * FROM ");
					pattern_sql.append (")");
				}

				if (subgraph_idx > 0) {
					if (sub_graph_pattern.get_operator () == Rasqal.GraphPattern.Operator.OPTIONAL) {
						pattern_sql.append (" NATURAL LEFT JOIN ");
					} else {
						pattern_sql.append (" NATURAL CROSS JOIN ");
					}
				}
				pattern_sql.append ("(");
				visit_graph_pattern (sub_graph_pattern);
				pattern_sql.append (")");

				// differs from pattern_idx due to filters
				subgraph_idx++;
			}
			pattern_sql.append (")");
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
					pattern_sql.append (" UNION ");
				}
				visit_graph_pattern (sub_graph_pattern);
			}
		}

		// process filters
		for (int pattern_idx = 0; true; pattern_idx++) {
			weak Rasqal.GraphPattern sub_graph_pattern = graph_pattern.get_sub_graph_pattern (pattern_idx);
			if (sub_graph_pattern == null) {
				break;
			}

			if (sub_graph_pattern.get_operator () != Rasqal.GraphPattern.Operator.FILTER) {
				// ignore non-filter subgraphs
				continue;
			}

			weak Rasqal.Expression filter = sub_graph_pattern.get_filter_expression ();

			if (!first_where) {
				pattern_sql.append (" AND ");
			} else {
				pattern_sql.append (" WHERE ");
				first_where = false;
			}

			visit_filter (filter);
		}
	}

	string get_string_from_literal (Rasqal.Literal lit, bool is_subject = false) {
		if (lit.type == Rasqal.Literal.Type.BLANK) {
			if (!is_subject && lit.as_string ().has_prefix (":")) {
				// anonymous blank node, libtracker-data will
				// generate appropriate uri
				return lit.as_string ();
			} else {
				return generate_bnodeid_handler (null, lit.as_string ());
			}
		} else {
			return lit.as_string ();
		}
	}

	void visit_triple (Rasqal.Triple triple) throws SparqlError {
		string subject;
		if (triple.subject.type == Rasqal.Literal.Type.VARIABLE) {
			subject = "?" + triple.subject.as_variable ().name;
		} else {
			subject = get_string_from_literal (triple.subject, true);
		}

		string db_table;
		bool rdftype = false;
		bool share_table = true;

		bool newtable;
		DataTable table;
		Property prop = null;

		if (triple.predicate.type == Rasqal.Literal.Type.URI) {
			prop = Ontology.get_property_by_uri (triple.predicate.as_string ());

			if (triple.predicate.as_string () == "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
			    && triple.object.type == Rasqal.Literal.Type.URI) {
				// rdf:type query
				rdftype = true;
				var cl = Ontology.get_class_by_uri (triple.object.as_string ());
				if (cl == null) {
					throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (triple.object.as_string ()));
				}
				db_table = cl.name;
			} else if (prop == null) {
				if (triple.predicate.as_string () == "http://www.tracker-project.org/ontologies/fts#match") {
					// fts:match
					db_table = "rdfs:Resource";
				} else {
					throw new SparqlError.UNKNOWN_PROPERTY ("Unknown property `%s'".printf (triple.predicate.as_string ()));
				}
			} else {
				if (triple.predicate.as_string () == "http://www.w3.org/2000/01/rdf-schema#domain"
				    && triple.subject.type == Rasqal.Literal.Type.VARIABLE
				    && triple.object.type == Rasqal.Literal.Type.URI) {
					// rdfs:domain
					var domain = Ontology.get_class_by_uri (triple.object.as_string ());
					if (domain == null) {
						throw new SparqlError.UNKNOWN_CLASS ("Unknown class `%s'".printf (triple.object.as_string ()));
					}
					var pv = predicate_variable_map.lookup (triple.subject.as_variable ().name);
					if (pv == null) {
						pv = new PredicateVariable ();
						predicate_variable_map.insert (triple.subject.as_variable ().name, pv);
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
			table = get_table (subject, db_table, share_table, out newtable);
		} else {
			// variable in predicate
			newtable = true;
			table = new DataTable ();
			table.predicate_variable = predicate_variable_map.lookup (triple.predicate.as_variable ().name);
			if (table.predicate_variable == null) {
				table.predicate_variable = new PredicateVariable ();
				predicate_variable_map.insert (triple.predicate.as_variable ().name, table.predicate_variable);
			}
			if (triple.subject.type == Rasqal.Literal.Type.URI) {
				// single subject
				table.predicate_variable.subject = subject;
			}
			if (triple.object.type == Rasqal.Literal.Type.URI) {
				// single object
				table.predicate_variable.object = get_string_from_literal (triple.object);
			}
			table.sql_query_tablename = triple.predicate.as_variable ().name + (++counter).to_string ();
			tables.append (table);

			// add to variable list
			var binding = new VariableBinding ();
			binding.is_uri = true;
			binding.variable = triple.predicate.as_variable ().name;
			binding.table = table;
			binding.sql_db_column_name = "predicate";
			var binding_list = pattern_var_map.lookup (binding.variable);
			if (binding_list == null) {
				binding_list = new VariableBindingList ();
				pattern_variables.append (binding.variable);
				pattern_var_map.insert (binding.variable, binding_list);

				pattern_sql.append_printf ("\"%s\".\"%s\" AS \"%s\", ",
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
			if (triple.subject.type == Rasqal.Literal.Type.VARIABLE) {
				var binding = new VariableBinding ();
				binding.is_uri = true;
				binding.variable = triple.subject.as_variable ().name;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				var binding_list = pattern_var_map.lookup (binding.variable);
				if (binding_list == null) {
					binding_list = new VariableBindingList ();
					pattern_variables.append (binding.variable);
					pattern_var_map.insert (binding.variable, binding_list);

					pattern_sql.append_printf ("\"%s\".\"%s\" AS \"%s\", ",
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
				binding.literal = get_string_from_literal (triple.subject);
				binding.literal_type = triple.subject.type;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				pattern_bindings.append (binding);
				bindings.append (binding);
			}
		}
		
		if (!rdftype) {
			if (triple.object.type == Rasqal.Literal.Type.VARIABLE) {
				var binding = new VariableBinding ();
				binding.variable = triple.object.as_variable ().name;
				binding.table = table;
				if (prop != null) {
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

					pattern_sql.append_printf ("\"%s\".\"%s\" AS %s, ",
						binding.table.sql_query_tablename,
						binding.sql_db_column_name,
						binding.variable);
				}
				binding_list.list.append (binding);
				if (var_map.lookup (binding.variable) == null) {
					var_map.insert (binding.variable, binding);
				}
			} else if (triple.predicate.as_string () == "http://www.tracker-project.org/ontologies/fts#match") {
				var binding = new LiteralBinding ();
				binding.is_fts_match = true;
				binding.literal = triple.object.as_string ();
				binding.literal_type = triple.object.type;
				binding.table = table;
				binding.sql_db_column_name = "ID";
				pattern_bindings.append (binding);
			} else {
				var binding = new LiteralBinding ();
				binding.literal = triple.object.as_string ();
				binding.literal_type = triple.object.type;
				binding.table = table;
				if (prop != null) {
					if (prop.data_type == PropertyType.RESOURCE) {
						binding.is_uri = true;
						binding.literal = get_string_from_literal (triple.object);
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

	string sql_operator (Rasqal.Op op) {
		switch (op) {
		case Rasqal.Op.AND: return " AND ";
		case Rasqal.Op.OR: return " OR ";
		case Rasqal.Op.EQ: return " = ";
		case Rasqal.Op.NEQ: return " <> ";
		case Rasqal.Op.LT: return " < ";
		case Rasqal.Op.GT: return " > ";
		case Rasqal.Op.LE: return " <= ";
		case Rasqal.Op.GE: return " >= ";
		case Rasqal.Op.PLUS: return " + ";
		case Rasqal.Op.MINUS: return " - ";
		case Rasqal.Op.STAR: return " * ";
		case Rasqal.Op.SLASH: return " / ";
		case Rasqal.Op.REM: return " % ";
		case Rasqal.Op.STR_EQ: return " = ";
		case Rasqal.Op.STR_NEQ: return " <> ";
		}
		return "";
	}

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

	void visit_filter (Rasqal.Expression expr, bool is_datetime = false) {
		switch (expr.op) {
		case Rasqal.Op.AND:
		case Rasqal.Op.OR:
		case Rasqal.Op.EQ:
		case Rasqal.Op.NEQ:
		case Rasqal.Op.LT:
		case Rasqal.Op.GT:
		case Rasqal.Op.LE:
		case Rasqal.Op.GE:
		case Rasqal.Op.PLUS:
		case Rasqal.Op.MINUS:
		case Rasqal.Op.STAR:
		case Rasqal.Op.SLASH:
		case Rasqal.Op.REM:
		case Rasqal.Op.STR_EQ:
		case Rasqal.Op.STR_NEQ:
			pattern_sql.append ("(");
			visit_filter (expr.arg1, is_datetime_variable (expr.arg2));
			pattern_sql.append (sql_operator (expr.op));
			visit_filter (expr.arg2, is_datetime_variable (expr.arg1));
			pattern_sql.append (")");
			break;
		case Rasqal.Op.UMINUS:
			pattern_sql.append ("-(");
			visit_filter (expr.arg1);
			pattern_sql.append (")");
			break;
		case Rasqal.Op.BANG:
			pattern_sql.append ("NOT (");
			visit_filter (expr.arg1);
			pattern_sql.append (")");
			break;
		case Rasqal.Op.LITERAL:
			if (expr.literal.type == Rasqal.Literal.Type.VARIABLE) {
				string variable_name = expr.literal.as_variable ().name;
				pattern_sql.append (variable_name);
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
		case Rasqal.Op.BOUND:
			pattern_sql.append ("(");
			visit_filter (expr.arg1);
			pattern_sql.append (") IS NOT NULL");
			break;
		case Rasqal.Op.REGEX:
			pattern_sql.append ("SparqlRegex(");
			visit_filter (expr.arg1);
			pattern_sql.append (", ");
			visit_filter (expr.arg2);
			pattern_sql.append (", ");
			if (expr.arg3 != null) {
				visit_filter (expr.arg3);
			} else {
				pattern_sql.append ("''");
			}
			pattern_sql.append (")");
			break;
		}
	}

	static string? get_string_for_value (Value value)
	{
		switch (value.type ()) {
		case typeof (int):
			return value.get_int ().to_string ();
		case typeof (double):
			return value.get_double ().to_string ();
		case typeof (string):
			return value.get_string ();
		default:
			return null;
		}
	}

	[CCode (cname = "uuid_generate")]
	public extern static void uuid_generate ([CCode (array_length = false)] uchar[] uuid);
}

