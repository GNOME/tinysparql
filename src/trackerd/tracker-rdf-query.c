/* Tracker - indexer and metadata database engine
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include <string.h>

#include "tracker-rdf-query.h"


/* RDF Query Condition
<rdfq:Condition>
	<rdfq:and>
        	<rdfq:greaterThan>
            		<rdfq:Property name="File:Size" />
            		<rdf:Integer>1000000</rdf:Integer>
          	</rdfq:greaterThan>
          	<rdfq:equals>
             		<rdfq:Property name="File:Path" />
             		<rdf:String>/home/jamie</rdf:String>
           	</rdfq:equals>
	</rdfq:and>
</rdfq:Condition>
*/


/* main elements */
#define ELEMENT_RDF_CONDITION 		"rdfq:Condition"
#define ELEMENT_RDF_PROPERTY 		"rdfq:Property"

/* operators */
#define ELEMENT_RDF_AND 		"rdfq:and"
#define ELEMENT_RDF_OR	 		"rdfq:or"
#define ELEMENT_RDF_NOT 		"rdfq:not"
#define ELEMENT_RDF_EQUALS 		"rdfq:equals"
#define ELEMENT_RDF_GREATER_THAN	"rdfq:greaterThan"
#define ELEMENT_RDF_GREATER_OR_EQUAL	"rdfq:greaterOrEqual"
#define ELEMENT_RDF_LESS_THAN 		"rdfq:lessThan"
#define ELEMENT_RDF_LESS_OR_EQUAL	"rdfq:lessOrEqual"

/* extension operators - "contains" does a substring or full text match, "in_Set" does string in list match */
#define ELEMENT_RDF_CONTAINS 		"rdfq:contains"
#define ELEMENT_RDF_REGEX        	"rdfq:regex"
#define ELEMENT_RDF_STARTS_WITH 	"rdfq:startsWith"
#define ELEMENT_RDF_IN_SET		"rdfq:inSet"

/* types */
#define ELEMENT_RDF_INTEGER 		"rdf:Integer"
#define ELEMENT_RDF_DATE 		"rdf:Date"	/* format can be iso 8601 with optional timezone "yyyy-mm-ddThh:mm:ss" or "yyyy-mm-ddThh:mm:ss+hh:mm" - most other formats are supported too */
#define ELEMENT_RDF_STRING 		"rdf:String"
#define ELEMENT_RDF_FLOAT 		"rdf:Float"

#define ELEMENT_IS(name) (strcmp (element_name, (name)) == 0)


enum {
	NO_ERROR,
	PARSE_ERROR,
};


typedef enum {
	STATE_START,
	STATE_CONDITION,
	STATE_END_CONDITION,
	STATE_PROPERTY,
	STATE_AND,
	STATE_END_AND,
	STATE_OR,
	STATE_END_OR,
	STATE_NOT,
	STATE_END_NOT,
	STATE_EQUALS,
	STATE_END_EQUALS,
	STATE_GREATER_THAN,
	STATE_END_GREATER_THAN,
	STATE_GREATER_OR_EQUAL,
	STATE_END_GREATER_OR_EQUAL,
	STATE_LESS_THAN,
	STATE_END_LESS_THAN,
	STATE_LESS_OR_EQUAL,
	STATE_END_LESS_OR_EQUAL,
	STATE_CONTAINS,
	STATE_END_CONTAINS,
	STATE_REGEX,
	STATE_END_REGEX,
	STATE_STARTS_WITH,
	STATE_END_STARTS_WITH,
	STATE_IN_SET,
	STATE_END_IN_SET,
	STATE_INTEGER,
	STATE_END_INTEGER,
	STATE_STRING,
	STATE_END_STRING,
	STATE_FLOAT,
	STATE_END_FLOAT,
	STATE_DATE,
	STATE_END_DATE
} ParseState;


typedef enum {
	OP_NONE,
	OP_EQUALS,
	OP_GREATER,
	OP_GREATER_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_CONTAINS,
	OP_REGEX,
	OP_SET,
	OP_STARTS
} Operators;


typedef enum {
	LOP_NONE,
	LOP_AND,
	LOP_OR
} LogicOperators;


typedef struct {
	char 		*alias;
	char 	 	*field_name;
	char	 	*meta_field;
	char	 	*table_name;
	char	 	*id_field;
	DataTypes	data_type;
	gboolean	multiple_values;
	gboolean 	is_select;
	gboolean 	is_condition;

} FieldData;


typedef struct {
	GMarkupParseContext 	*context;
	GMarkupParser       	*parser;
	GSList 			*stack;
	GSList 			*fields;
	gboolean		query_okay;
	int			statement_count;
	LogicOperators		current_logic_operator;
	Operators		current_operator;
	char 			*current_field;
	char			*current_value;
	DBConnection		*db_con;
	GString			*sql_select;
	GString			*sql_from;
	GString			*sql_where;
	GString			*sql_order;
	char			*service;
} ParserData;


static GQuark error_quark;


static void start_element_handler (GMarkupParseContext *context,
				   const gchar *element_name,
				   const gchar **attribute_names,
				   const gchar **attribute_values,
				   gpointer user_data,
				   GError **error);

static void end_element_handler (GMarkupParseContext *context,
				 const gchar *element_name,
				 gpointer user_data,
				 GError **error);

static void text_handler (GMarkupParseContext *context,
		     const gchar *text,
		     gsize text_len,
		     gpointer user_data,
		     GError **error);

static void error_handler (GMarkupParseContext *context,
		      GError *error,
		      gpointer user_data);


static gboolean
is_operator (ParseState state)
{
	return state == STATE_EQUALS || state == STATE_GREATER_THAN || state == STATE_LESS_THAN ||
			state == STATE_CONTAINS || state == STATE_IN_SET || STATE_LESS_OR_EQUAL ||
			STATE_GREATER_OR_EQUAL || state == STATE_STARTS_WITH || state == STATE_REGEX;

}


static gboolean
is_end_operator (ParseState state)
{
	return state == STATE_END_EQUALS || state == STATE_END_GREATER_THAN || state == STATE_END_LESS_THAN ||
			state == STATE_END_CONTAINS || state == STATE_END_IN_SET || STATE_END_LESS_OR_EQUAL ||
			STATE_END_GREATER_OR_EQUAL || state == STATE_END_STARTS_WITH || state == STATE_REGEX;

}


static gboolean
is_logic (ParseState state)
{
	return state == STATE_AND || state == STATE_OR || state == STATE_NOT;
}


static gboolean
is_end_logic (ParseState state)
{
	return state == STATE_END_AND || state == STATE_END_NOT || state == STATE_END_OR;
}


static void
set_error (GError              **err,
           GMarkupParseContext  *context,
           int                   error_code,
           const char           *format,
           ...)
{
	int	line, ch;
	va_list args;
	char    *str;

	g_markup_parse_context_get_position (context, &line, &ch);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_set_error (err, error_quark, error_code, "Line %d character %d: %s", line, ch, str);

	g_free (str);
}


static gboolean
set_error_on_fail (gboolean condition, GMarkupParseContext *context, const char *msg, GError **err)
{
	if (!condition) {
		set_error (err, context, 1, msg);
		return TRUE;
	}

	return FALSE;
}


static const char *
get_attribute_value (const char *name,
		     const char **names,
		     const char **values)
{
	int i;

	i = 0;

	while (names[i]) {
		if (strcmp (name, names[i]) == 0) {
			return values[i];
		}
		i++;
	}

	return NULL;
}


static const char *
get_attribute_value_required (GMarkupParseContext *context,
			      const char          *tag,
			      const char          *name,
			      const char          **names,
			      const char          **values,
			      GError              **error)
{
	const char *value;

	value = get_attribute_value (name, names, values);

	if (!value) {
		set_error (error, context, PARSE_ERROR,
			   "%s must have \"%s\" attribute",
			   tag, name);
	}

	return value;
}


static void
push_stack (ParserData *data, ParseState  state)
{
	data->stack = g_slist_prepend (data->stack, GINT_TO_POINTER (state));
}


static void
pop_stack (ParserData *data)
{
	g_return_if_fail (data->stack != NULL);

	data->stack = g_slist_remove (data->stack, data->stack->data);
}


static ParseState
peek_state (ParserData *data)
{
	g_return_val_if_fail (data->stack != NULL, STATE_START);

	return GPOINTER_TO_INT (data->stack->data);
}


static void
pop_stack_until (ParserData *data, ParseState state)
{
	while (data->stack != NULL) {

		if (state == peek_state (data)) {
			pop_stack (data);
			break;
		}

		pop_stack (data);
	}
}


static void
free_metadata_field (FieldData *field_data)
{
	g_return_if_fail (field_data);

	if (field_data->alias) {
		g_free (field_data->alias);
	}

	if (field_data->field_name) {
		g_free (field_data->field_name);
	}

	if (field_data->meta_field) {
		g_free (field_data->meta_field);
	}

	if (field_data->table_name) {
		g_free (field_data->table_name);
	}

	if (field_data->id_field) {
		g_free (field_data->id_field);
	}

	g_free (field_data);
}


static FieldData *
add_metadata_field (ParserData *data, const char *field_name, gboolean is_select, gboolean is_condition)
{
	gboolean     field_exists;
	FieldData    *field_data;
	const GSList *tmp;

	field_exists = FALSE;
	field_data = NULL;

	// check if field is already in list
	for (tmp = data->fields; tmp; tmp = tmp->next) {
		FieldData *tmp_field;

		tmp_field = tmp->data;

		if (tmp_field && tmp_field->field_name) {
			if (strcmp (tmp_field->field_name, field_name) == 0) {

				field_data = tmp_field;
		
				if ((field_data->multiple_values && field_data->is_condition && is_select) || (field_data->multiple_values && field_data->is_select && is_condition) ) {
					field_exists = FALSE;
				} else {
					field_exists = TRUE;
	
					if (is_condition) {
						field_data->is_condition = TRUE;
					} 

					if (is_select) {
						field_data->is_select = TRUE;
					}

					break;
				}
			}
		}
	}

	

	if (!field_exists) {
		FieldDef *def;
		char	 *istr;
		int	 i;

		field_data = g_new0 (FieldData, 1);

		field_data->is_select = is_select;
		field_data->is_condition = is_condition;
		field_data->field_name = g_strdup (field_name);

		i = g_slist_length (data->fields);

		istr = tracker_int_to_str (i);

		field_data->alias = g_strconcat ("M", istr, NULL);
		g_free (istr);

		def = tracker_db_get_field_def (data->db_con, field_name);

		if (def) {
			char *st;
			
			if (is_select && def->multiple_values) {
				st = g_strdup ("ServiceMetaDataDisplay");
			} else {
				st = tracker_get_metadata_table (def->type);
			}

			field_data->data_type = def->type;
			field_data->meta_field = g_strconcat (field_data->alias, ".MetaDataValue", NULL);
			field_data->table_name = g_strdup (st);
			field_data->id_field = g_strdup (def->id);
			field_data->multiple_values = def->multiple_values;
			
			data->fields = g_slist_prepend (data->fields, field_data);

			if (is_select) {

			/* leave datetime fields as integers (seconds from epoch) so clients can format as they wish */

//				if (def->type == DATA_DATE) {
//					g_string_append_printf (data->sql_select, ", FormatDate(%s)", field_data->meta_field);
//				} else {
				g_string_append_printf (data->sql_select, ", %s", field_data->meta_field);
//				}

			}

			g_free (st);

			tracker_db_free_field_def (def);

		} else {
			g_free (field_data);
			return NULL;
		}

	} 
	

	return field_data;
}


static void
start_element_handler (GMarkupParseContext *context,
		       const gchar	   *element_name,
		       const gchar	   **attribute_names,
		       const gchar	   **attribute_values,
		       gpointer		   user_data,
		       GError		   **error)
{
	ParserData *data;
	ParseState state;

	data = user_data;
	state = peek_state (data);

	if (ELEMENT_IS (ELEMENT_RDF_CONDITION)) {

		if (set_error_on_fail ((state == STATE_START), context, "Condition element not expected here", error)) {
			return;
		}

		push_stack (data, STATE_CONDITION);

	} else if (ELEMENT_IS (ELEMENT_RDF_PROPERTY)) {
		const char *name;

		if (set_error_on_fail ( is_operator (state), context,  "Property element not expected here", error)) {
			return;
		}

		name = get_attribute_value_required (context, "<rdfq:Property>", "name",
						     attribute_names, attribute_values,
						     error);

		if (!name) {
			return;
		} else {

			if (data->current_operator == OP_NONE ) {
				set_error (error, context, PARSE_ERROR, "no operator found for Property \"%s\"", name);
				return;
			}

			data->current_field =  g_strdup (name);

			push_stack (data, STATE_PROPERTY);
		}

	} else if (ELEMENT_IS (ELEMENT_RDF_AND)) {

		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)),
				       context, "AND element not expected here", error)) {
			return;
		}

		if (data->statement_count > 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where, " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where, " OR ");
				}
			}
		}

		data->statement_count = 0;
		data->sql_where = g_string_append (data->sql_where, " ( ");
		data->current_logic_operator = LOP_AND;
		push_stack (data, STATE_AND);

	} else if (ELEMENT_IS (ELEMENT_RDF_OR)) {

		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)),
				       context, "OR element not expected here", error)) {
			return;
		}

		if (data->statement_count > 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where, " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where, " OR ");
				}
			}
		}

		data->statement_count = 0;
		data->sql_where = g_string_append (data->sql_where, " ( ");
		data->current_logic_operator = LOP_OR;
		push_stack (data, STATE_OR);

	} else if (ELEMENT_IS (ELEMENT_RDF_NOT)) {

		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)),
				       context, "NOT element not expected here", error)) {
			return;
		}

		if (data->statement_count > 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where, " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where, " OR ");
				}
			}
		}
		data->sql_where = g_string_append (data->sql_where, " NOT ( ");
		push_stack (data, STATE_NOT);

	} else if (ELEMENT_IS (ELEMENT_RDF_EQUALS)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "EQUALS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_EQUALS;
		push_stack (data, STATE_EQUALS);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_THAN)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context,  "GREATERTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_GREATER;
		push_stack (data, STATE_GREATER_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_OR_EQUAL)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "GREATEROREQUAL element not expected here", error)) {
			return;
		}

		data->current_operator = OP_GREATER_EQUAL;
		push_stack (data, STATE_GREATER_OR_EQUAL);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "LESSTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_LESS;
		push_stack (data, STATE_LESS_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_OR_EQUAL )) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "LESSOREQUAL element not expected here", error)) {
			return;
		}

		data->current_operator = OP_LESS_EQUAL;
		push_stack (data, STATE_LESS_OR_EQUAL);

	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "CONTAINS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_CONTAINS;
		push_stack (data, STATE_CONTAINS);

	} else if (ELEMENT_IS (ELEMENT_RDF_REGEX)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "REGEX element not expected here", error)) {
			return;
		}

		data->current_operator = OP_REGEX;
		push_stack (data, STATE_REGEX);

	} else if (ELEMENT_IS (ELEMENT_RDF_STARTS_WITH)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "STARTSWITH element not expected here", error)) {
			return;
		}

		data->current_operator = OP_STARTS;
		push_stack (data, STATE_STARTS_WITH);

	} else if (ELEMENT_IS (ELEMENT_RDF_IN_SET)) {

		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND || data->current_logic_operator == LOP_OR)
					 && is_end_operator (state)),
					context, "IN SET element not expected here", error)) {
			return;
		}

		data->current_operator = OP_SET;
		push_stack (data, STATE_IN_SET);


	} else if (ELEMENT_IS (ELEMENT_RDF_INTEGER)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context, "INTEGER element not expected here", error)) {
			return;
		}

		push_stack (data, STATE_INTEGER);


	} else if (ELEMENT_IS (ELEMENT_RDF_DATE)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context, "DATE element not expected here", error)) {
			return;
		}

		push_stack (data, STATE_DATE);


	} else if (ELEMENT_IS (ELEMENT_RDF_STRING)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context, "STRING element not expected here", error)) {
			return;
		}

		push_stack (data, STATE_STRING);

	} else if (ELEMENT_IS (ELEMENT_RDF_FLOAT)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context, "FLOAT element not expected here", error)) {
			return;
		}

		push_stack (data, STATE_FLOAT);
	}
}


static char *
get_value (const char *value, gboolean quote)
{
	if (quote) {
		return g_strconcat (" '", value, "' ", NULL);
	} else {
		return g_strdup (value);
	}
}


static gboolean
build_sql (ParserData *data)
{
	ParseState state;
	char 	   *avalue, *value, *sub;
	FieldData  *field_data;
	GString    *str;

	g_return_val_if_fail (data->current_field && data->current_operator != OP_NONE && data->current_value, FALSE);

	str = g_string_new ("");

	data->statement_count++;

	state = peek_state (data);

	avalue = get_value (data->current_value, (state != STATE_END_DATE && state != STATE_END_INTEGER && state != STATE_END_FLOAT));

	field_data = add_metadata_field (data, data->current_field, FALSE, TRUE);

	if (!field_data) {
		g_free (avalue);
		g_free (data->current_field);
		g_free (data->current_value);
		return FALSE;
	}

	if (field_data->data_type ==  DATA_DATE) {
		char *bvalue;
		int cvalue;

		bvalue = tracker_format_date (avalue);
		g_debug (bvalue);
		cvalue = tracker_str_to_date (bvalue);
		g_debug ("%d", cvalue);
		value = tracker_int_to_str (cvalue);
		g_free (bvalue);
	} else {
		value = g_strdup (avalue);
	}

	g_free (avalue);

	if (data->statement_count > 1) {
		if (data->current_logic_operator == LOP_AND) {
			data->sql_where = g_string_append (data->sql_where, "\n AND ");
		} else {
			if (data->current_logic_operator == LOP_OR) {
				data->sql_where = g_string_append (data->sql_where, "\n OR ");
			}
		}
	}

	char **s;

	switch (data->current_operator) {

		case OP_EQUALS:

			sub = strchr (data->current_value, '*');
			if (sub) {
				g_string_append_printf (str, " (%s glob '%s') ", field_data->meta_field, data->current_value);
			} else {
				if (field_data->data_type == DATA_DATE || field_data->data_type == DATA_NUMERIC) {
					g_string_append_printf (str, " (%s = %s) ", field_data->meta_field, value);
				} else {
					g_string_append_printf (str, " (%s = '%s') ", field_data->meta_field, value);
				}
			}

			break;

		case OP_GREATER:

			g_string_append_printf (str, " (%s > %s) ", field_data->meta_field, value);

			break;

		case OP_GREATER_EQUAL:

			g_string_append_printf (str, " (%s >= %s) ", field_data->meta_field, value);

			break;

		case OP_LESS:

			g_string_append_printf (str, " (%s < %s) ", field_data->meta_field, value);

			break;

		case OP_LESS_EQUAL:

			g_string_append_printf (str, " (%s <= %s) ", field_data->meta_field, value);

			break;

		case OP_CONTAINS:

			sub = strchr (data->current_value, '*');

			if (sub) {
				g_string_append_printf (str, " (%s glob '*%s') ", field_data->meta_field, data->current_value);
			} else {
				g_string_append_printf (str, " (%s glob  '*%s*') ", field_data->meta_field, data->current_value);
			}

			break;

		case OP_STARTS:
			
			sub = strchr (data->current_value, '*');

			if (sub) {
				g_string_append_printf (str, " (%s glob '%s') ", field_data->meta_field, data->current_value);
			} else {
				g_string_append_printf (str, " (%s glob '%s*') ", field_data->meta_field, data->current_value);
			}
			
			break;

		case OP_REGEX:

			g_string_append_printf (str, " (%s REGEXP '%s') ", field_data->meta_field, data->current_value);

			break;

		case OP_SET:

			s = g_strsplit (data->current_value, ",", 0);
			
			if (s && s[0]) {

				g_string_append_printf (str, " (%s in ('%s'", field_data->meta_field, s[0]);

				char **p;
				for (p = s+1; *p; p++) {
					g_string_append_printf (str, ",'%s'", *p); 					
				}
				g_string_append_printf (str, ") ) " ); 					
					
			}

			break;

		default:

			break;
	}

	data->sql_where = g_string_append (data->sql_where, str->str);

	g_string_free (str, TRUE);

	g_free (data->current_field);
	data->current_field = NULL;

	g_free (data->current_value);
	data->current_value = NULL;

	g_free (value);

	return TRUE;
}


static void
end_element_handler (GMarkupParseContext *context,
		     const gchar	 *element_name,
		     gpointer		 user_data,
		     GError		 **error)
{
	ParserData *data;

	data = user_data;

	if (ELEMENT_IS (ELEMENT_RDF_CONDITION)) {

		push_stack (data, STATE_END_CONDITION);
		data->query_okay = TRUE;

	} else if (ELEMENT_IS (ELEMENT_RDF_AND)) {

		data->sql_where = g_string_append (data->sql_where, " ) ");

		pop_stack_until (data, STATE_AND);

		if (peek_state (data) != STATE_AND) {
			if (peek_state (data) == STATE_OR) {
				data->current_logic_operator = LOP_OR;
			} else {
				data->current_logic_operator = LOP_NONE;
			}
		}

	} else if (ELEMENT_IS (ELEMENT_RDF_OR)) {

		data->sql_where = g_string_append (data->sql_where, " ) ");

		pop_stack_until (data, STATE_OR);

		if (peek_state (data) != STATE_OR) {
			if (peek_state (data) == STATE_AND) {
				data->current_logic_operator = LOP_AND;
			} else {
				data->current_logic_operator = LOP_NONE;
			}
		}

	} else if (ELEMENT_IS (ELEMENT_RDF_NOT)) {

		data->sql_where = g_string_append (data->sql_where, " ) ");
		pop_stack_until (data, STATE_NOT);


	} else if (ELEMENT_IS (ELEMENT_RDF_EQUALS)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}
		push_stack (data, STATE_END_EQUALS);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_THAN)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_GREATER_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_OR_EQUAL)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_GREATER_OR_EQUAL);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_LESS_THAN );

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_OR_EQUAL )) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_LESS_OR_EQUAL );


	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_CONTAINS);

	} else if (ELEMENT_IS (ELEMENT_RDF_REGEX)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_REGEX);

	} else if (ELEMENT_IS (ELEMENT_RDF_STARTS_WITH)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_STARTS_WITH);

	} else if (ELEMENT_IS (ELEMENT_RDF_IN_SET)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_IN_SET);


	} else if (ELEMENT_IS (ELEMENT_RDF_INTEGER)) {

		push_stack (data, STATE_END_INTEGER);


	} else if (ELEMENT_IS (ELEMENT_RDF_DATE)) {

		push_stack (data, STATE_END_DATE);


	} else if (ELEMENT_IS (ELEMENT_RDF_STRING)) {

		push_stack (data, STATE_END_STRING);

	}  else if (ELEMENT_IS (ELEMENT_RDF_FLOAT)) {

		push_stack (data, STATE_END_FLOAT);
	}
}


static void
text_handler (GMarkupParseContext *context,
	      const gchar	  *text,
	      gsize		  text_len,
	      gpointer		  user_data,
	      GError		  **error)
{
	ParserData *data;
	ParseState state;

	data = user_data;
	state = peek_state (data);

	switch (state) {

		case STATE_INTEGER:
		case STATE_STRING:
		case STATE_DATE:
		case STATE_FLOAT:

			data->current_value = g_strstrip (g_strndup (text, text_len));
			break;

		default :
			break;
	}
}


static void
error_handler (GMarkupParseContext *context,
	       GError		   *error,
	       gpointer		   user_data)
{
	tracker_log ("Error in rdf query parse: %s", error->message);
}

static GString *
get_select_header (const char *service) 
{
	GString *result;
	int type;
		
	result = g_string_new ("");
	type = tracker_get_id_for_service (service);

	switch (type) {

		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:		
		case 8:
			g_string_append_printf (result, " Select DISTINCT (S.Path || '%s' || S.Name) as uri, GetServiceName(S.ServiceTypeID) as stype ", G_DIR_SEPARATOR_S); 
			break;
		
		default :
			g_string_append_printf (result, " Select DISTINCT (S.Path || '%s' || S.Name) as uri, GetServiceName(S.ServiceTypeID) as stype ", G_DIR_SEPARATOR_S); 
			break;
	}
	
	return result;

}
	


char *
tracker_rdf_query_to_sql (DBConnection *db_con, const char *query, const char *service, char **fields, int field_count, const char *search_text, const char *keyword, gboolean sort_by_service, int offset, int limit, GError *error)
{
	static     gboolean inited = FALSE;
	ParserData data;
	char       *result;

	g_return_val_if_fail (query != NULL, NULL);

	if (!inited) {
		error_quark = g_quark_from_static_string ("RDF-parser-error-quark");
		inited = TRUE;
	}

	memset (&data, 0, sizeof (data));
	data.db_con = db_con;
	data.statement_count = 0;

	data.sql_select = get_select_header (service);

	if (field_count > 0) {
		int i;

		for (i = 0; i < field_count; i++) {
			FieldData *field_data;

			field_data = add_metadata_field (&data, fields[i], TRUE, FALSE);

			if (!field_data) {
				tracker_log ("RDF Query failed : field %s not found", fields[i]);
				g_slist_foreach (data.fields, (GFunc) free_metadata_field, NULL);
				g_slist_free (data.fields);
				g_string_free (data.sql_select, TRUE);
				return NULL;
			}
		}
	}

	char *table_name;
	gboolean do_search = FALSE;

	table_name = "Services";

	data.sql_from = g_string_new ("");

	g_debug ("search term is %s", search_text);


	if (search_text && (strlen (search_text) > 0)) {
		do_search = TRUE;
		g_string_append_printf (data.sql_from, "\n FROM %s S INNER JOIN SearchResults1 M ON S.ID = M.SID ", table_name);
	} else {
		g_string_append_printf (data.sql_from, "\n FROM %s S ", table_name);
	}

	if (keyword && strlen (keyword) > 0) {
		char *keyword_metadata = tracker_get_related_metadata_names (db_con, "DC:Keywords");
		g_string_append_printf (data.sql_from, "\n INNER JOIN ServiceKeywordMetaData K ON S.ID = K.ServiceID and K.MetaDataID in (%s) and K.MetaDataValue = '%s' ", keyword_metadata, keyword);
		g_free (keyword_metadata);
	} 

	data.sql_where = g_string_new ("");

	if (strlen (query) < 10) {
		g_string_append_printf (data.sql_where, "\n WHERE (S.ServiceTypeID between GetServiceTypeID('%s') and GetMaxServiceTypeID('%s')) ", service, service);
	} else {
		g_string_append_printf (data.sql_where, "\n WHERE (S.ServiceTypeID between GetServiceTypeID('%s') and GetMaxServiceTypeID('%s')) AND ", service, service);
	}

	if (limit < 1) {
		limit = 1024;
	}

	if (sort_by_service) {
		if (do_search) {
			data.sql_order = g_string_new ("\n ORDER BY M.Score desc, S.ServiceTypeID, uri LIMIT ");
		} else {
			data.sql_order = g_string_new ("\n ORDER BY S.ServiceTypeID, uri LIMIT ");
		}

	} else {
		if (do_search) {
			data.sql_order = g_string_new ("\n ORDER BY M.Score desc LIMIT ");
		} else {
			data.sql_order = g_string_new ("\n  LIMIT ");
		}

	}

	g_string_append_printf (data.sql_order, "%d,%d ", offset, limit);

	data.parser = g_new0 (GMarkupParser, 1);
	data.parser->start_element = start_element_handler;
	data.parser->text = text_handler;
	data.parser->end_element = end_element_handler;
	data.parser->error = error_handler;

	data.current_operator = OP_NONE;
	data.current_logic_operator = LOP_NONE;
	data.query_okay = FALSE;

	data.context = g_markup_parse_context_new (data.parser, 0, &data, NULL);

	push_stack (&data, STATE_START);

	result = NULL;

	if (!g_markup_parse_context_parse (data.context, query, -1, &error)) {

		g_string_free (data.sql_select, TRUE);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);
		g_string_free (data.sql_order, TRUE);

	} else {
		const GSList *tmp;
		FieldData    *tmp_field;

		for (tmp = data.fields; tmp; tmp = tmp->next) {
			tmp_field = tmp->data;

			if (!tmp_field->is_condition) {
				g_string_append_printf (data.sql_from, "\n LEFT OUTER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID = %s) ", tmp_field->table_name, tmp_field->alias, tmp_field->alias, tmp_field->alias, tmp_field->id_field);
			} else {
				char *related_metadata = tracker_get_related_metadata_names (db_con, tmp_field->field_name);
				g_string_append_printf (data.sql_from, "\n INNER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID in (%s)) ", tmp_field->table_name, tmp_field->alias, tmp_field->alias, tmp_field->alias, related_metadata);
				g_free (related_metadata);
			}
		}

		result = g_strconcat (data.sql_select->str, " ", data.sql_from->str, " ", data.sql_where->str, " ", data.sql_order->str, NULL);
		
		g_string_free (data.sql_select, TRUE);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);
		g_string_free (data.sql_order, TRUE);
	}

	g_slist_foreach (data.fields, (GFunc) free_metadata_field, NULL);
	g_slist_free (data.fields);

	g_slist_free (data.stack);
	g_markup_parse_context_free (data.context);

	if (data.current_field) {
		g_free (data.current_field);
	}

	if (data.current_value) {
		g_free (data.current_value);
	}

	g_free (data.parser);

	return result;
}


