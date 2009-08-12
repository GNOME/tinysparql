/* rasqal.vapi
 *
 * Copyright (C) 2008-2009  Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * Author:
 * 	Jürg Billeter <j@bitron.ch>
 */

[CCode (cheader_filename = "raptor.h")]
namespace Raptor {
	[Compact]
	[CCode (cname = "raptor_locator")]
	public class Locator {
		public weak string file;
		public int line;
		public int column;
		public int byte;
	}

	[Compact]
	[CCode (cname = "raptor_uri")]
	public class Uri {
		[CCode (cname = "raptor_new_uri")]
		public Uri (string uri_string);
		public unowned string as_string ();
	}

	[CCode (cname = "raptor_message_handler", instance_pos = 0)]
	public delegate void MessageHandler (Locator locator, string message);
}

[CCode (cheader_filename = "rasqal.h")]
namespace Rasqal {
	[Compact]
	[CCode (cname = "rasqal_graph_pattern")]
	public class GraphPattern {
		public enum Operator {
			BASIC,
			OPTIONAL,
			UNION,
			GROUP,
			GRAPH,
			FILTER
		}

		public weak Expression get_filter_expression ();
		public Operator get_operator ();
		public weak GraphPattern get_sub_graph_pattern (int idx);
		public weak Triple get_triple (int idx);
		public void print (GLib.FileStream fh);
	}

	[CCode (cname = "rasqal_op", cprefix = "RASQAL_EXPR_")]
	public enum Op {
		AND,
		OR,
		EQ,
		NEQ,
		LT,
		GT,
		LE,
		GE,
		UMINUS,
		PLUS,
		MINUS,
		STAR,
		SLASH,
		REM,
		STR_EQ,
		STR_NEQ,
		STR_MATCH,
		STR_NMATCH,
		TILDE,
		BANG,
		LITERAL,
		FUNCTION,
		BOUND,
		STR,
		LANG,
		DATATYPE,
		ISURI,
		ISBLANK,
		ISLITERAL,
		CAST,
		ORDER_COND_ASC,
		ORDER_COND_DESC,
		LANGMATCHES,
		REGEX,
		GROUP_COND_ASC,
		GROUP_COND_DESC,
		COUNT,
		VARSTAR,
		SAMETERM,
		SUM,
		AVG,
		MIN,
		MAX,
		GROUP_CONCAT
	}

	[Compact]
	[CCode (cname = "rasqal_expression", free_function = "rasqal_free_expression")]
	public class Expression {
		public Op op;
		public Expression? arg1;
		public Expression? arg2;
		public Expression? arg3;
		public Literal? literal;
	}

	[CCode (cname = "rasqal_generate_bnodeid_handler", instance_pos = 1.1)]
	public delegate string GenerateBnodeidHandler (Rasqal.Query query, string? user_bnodeid);

	[Compact]
	[CCode (cname = "rasqal_literal", free_function = "rasqal_free_literal")]
	public class Literal {
		[CCode (cname = "rasqal_literal_type", cprefix = "RASQAL_LITERAL_")]
		public enum Type {
			BLANK,
			URI,
			STRING,
			BOOLEAN,
			INTEGER,
			DOUBLE,
			FLOAT,
			DECIMAL,
			DATETIME,
			PATTERN,
			QNAME,
			VARIABLE
		}

		public Type type;

		public weak string? as_string ();
		public weak Variable? as_variable ();
	}

	[Compact]
	[CCode (cname = "rasqal_prefix", free_function = "rasqal_free_prefix")]
	public class Prefix {
		[CCode (cname = "rasqal_new_prefix")]
		public Prefix (World world, owned string prefix, owned Raptor.Uri uri);
	}

	[Compact]
	[CCode (cname = "rasqal_query", free_function = "rasqal_free_query")]
	public class Query {
		[CCode (cname = "rasqal_new_query")]
		public Query (World world, string? name, string? uri);
		public void add_prefix (owned Prefix prefix);
		public void declare_prefixes ();
		public weak Triple get_construct_triple (int idx);
		public bool get_distinct ();
		public int get_limit ();
		public int get_offset ();
		public QueryVerb get_verb ();
		public weak DataGraph? get_data_graph (int idx);
		public weak Expression? get_group_condition (int idx);
		public weak Expression? get_order_condition (int idx);
		public weak GraphPattern get_query_graph_pattern ();
		public weak Variable? get_variable (int idx);
		public int prepare (string? query_string, string? base_uri);
		public void print (GLib.FileStream fh);
		public void set_error_handler ([CCode (delegate_target_pos = 0.9)] Raptor.MessageHandler handler);
		public void set_fatal_error_handler ([CCode (delegate_target_pos = 0.9)] Raptor.MessageHandler handler);
		public void set_generate_bnodeid_handler ([CCode (delegate_target_pos = 0.9)] GenerateBnodeidHandler handler);
		public void set_warning_handler ([CCode (delegate_target_pos = 0.9)] Raptor.MessageHandler handler);
		public unowned Query next ();
	}

	public enum QueryVerb {
		SELECT,
		CONSTRUCT,
		DESCRIBE,
		ASK,
		DELETE,
		INSERT,
		DROP
	}

	[Compact]
	[CCode (cname = "rasqal_triple", free_function = "rasqal_free_triple")]
	public class Triple {
		public Literal subject;
		public Literal predicate;
		public Literal object;
		public Literal origin;

		public void print (GLib.FileStream fh);
	}

	[Compact]
	[CCode (cname = "rasqal_variable", free_function = "rasqal_free_variable")]
	public class Variable {
		public weak string? name;
		public Expression? expression;
	}

	[Compact]
	[CCode (cname = "rasqal_data_graph", free_function = "rasqal_free_data_graph")]
	public class DataGraph {
		public Raptor.Uri uri;
		public Raptor.Uri name_uri;
	}

	[Compact]
	[CCode (cname = "rasqal_world", free_function = "rasqal_free_world")]
	public class World {
		[CCode (cname = "rasqal_new_world")]
		public World ();
		public void open ();
	}
}

