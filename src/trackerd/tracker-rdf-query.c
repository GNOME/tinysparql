/* Tracker
 * Copyright (C) 2005, Mr Jamie McCracken (jamiemcc@gnome.org)
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "tracker-rdf-query.h"

/* RDF Query Condition
<rdfq:Condition>
	<rdfq:and>
        	<rdfq:greaterThan>
            		<rdfq:Property name="File.Size" />
            		<rdf:Integer>1000000</rdf:Integer> 
          	</rdfq:greaterThan>
          	<rdfq:equals>
             		<rdfq:Property name="File.Path" />
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
	char	 	*id_field;
	DataTypes	data_type;
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
			STATE_GREATER_OR_EQUAL || state == STATE_STARTS_WITH;

}

static gboolean
is_end_operator (ParseState state)
{
	
	return state == STATE_END_EQUALS || state == STATE_END_GREATER_THAN || state == STATE_END_LESS_THAN ||
			state == STATE_END_CONTAINS || state == STATE_END_IN_SET || STATE_END_LESS_OR_EQUAL || 
			STATE_END_GREATER_OR_EQUAL || state == STATE_END_STARTS_WITH;

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
	int line, ch;
	va_list   args;
	char     *str;
  
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
get_attribute_value (const char  *name,
		     const char **names,
		     const char **values)
{
	int i = 0;

	while (names[i]) {
		if (strcmp (name, names[i]) == 0) {
			return values[i];
		}
		i++;
	}

	return NULL;
}

static const char *
get_attribute_value_required (GMarkupParseContext  *context,
			      const char           *tag,
			      const char           *name,
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
	data->stack = g_slist_prepend (data->stack,  GINT_TO_POINTER (state));
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

	if (field_data->alias) g_free (field_data->alias);
	if (field_data->field_name) g_free (field_data->field_name);
	if (field_data->meta_field) g_free (field_data->meta_field);
	if (field_data->id_field) g_free (field_data->id_field);

	g_free (field_data);

}


static FieldData *
add_metadata_field (ParserData *data, const char* field_name, gboolean is_select, gboolean is_condition)
{


	gboolean field_exists = FALSE;
	FieldData *field_data = NULL, *tmp_field;
	const GSList *tmp;
	char *st;

	// check if field is already in list 
	tmp = data->fields;

	while (tmp) {

		tmp_field = tmp->data;

		if (tmp_field && tmp_field->field_name) {
			if (strcmp (tmp_field->field_name, field_name) == 0) {
				field_exists = TRUE;
				field_data = tmp_field;
				break;
			}
		}


		tmp = tmp->next;
	}
	


	if (!field_exists) {

		field_data = g_new0 (FieldData, 1);

		field_data->is_select = is_select;
		field_data->is_condition = is_condition;	
		field_data->field_name = g_strdup (field_name);

		int i = g_slist_length (data->fields);

		char *istr = tracker_int_to_str (i);

		field_data->alias =  g_strconcat ("M", istr , NULL);
		g_free (istr);

		FieldDef *def = tracker_db_get_field_def (data->db_con, field_name);

		if (def) {
			
			if (def->type == DATA_INDEX_STRING) {	

				st = "MetaDataIndexValue";

			} else if (def->type == DATA_STRING) {

				st = "MetaDataValue";
			} else { 
				st = "MetaDataNumericValue";
			}
		
			field_data->data_type = def->type;
			field_data->meta_field = g_strconcat (field_data->alias, ".", st,  NULL);
			field_data->id_field = g_strdup (def->id);
			tracker_db_free_field_def (def);

			data->fields = g_slist_prepend (data->fields, field_data);
			
			if (is_select) {
				char *str = g_strconcat (", ", field_data->meta_field, NULL);
				data->sql_select = g_string_append (data->sql_select, str);
				g_free (str);
			}
		
			
		} else {
			g_free (field_data);
			return NULL;
		}

	} else {
		if (is_condition) {
			field_data->is_condition = TRUE;
		}
	}

	return field_data;

}	


static void
start_element_handler (GMarkupParseContext *context,
			      const gchar *element_name,
			      const gchar **attribute_names,
			      const gchar **attribute_values,
			      gpointer user_data,
			      GError **error)
{

	ParserData   *data;
	ParseState   state;

	data = user_data;
	state = peek_state (data);	

	if (ELEMENT_IS (ELEMENT_RDF_CONDITION)) {

		if (set_error_on_fail ((state == STATE_START), context, "Condition element not expected here",error)) {
			return;
		}

		push_stack (data, STATE_CONDITION);

	} else if (ELEMENT_IS (ELEMENT_RDF_PROPERTY)) {
	
		const char *name = NULL;

		if (set_error_on_fail ( is_operator (state), context,  "Property element not expected here",error)) {
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
		
		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)), context,  "AND element not expected here",error)) {
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
		
		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)), context,  "OR element not expected here",error)) {
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

		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)), context,   "NOT element not expected here",error)) {
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
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "EQUALS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_EQUALS;
		push_stack (data, STATE_EQUALS);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_THAN)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "GREATERTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_GREATER;
		push_stack (data, STATE_GREATER_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_OR_EQUAL)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "GREATEROREQUAL element not expected here", error)) {
			return;
		}

		data->current_operator = OP_GREATER_EQUAL;
		push_stack (data, STATE_GREATER_OR_EQUAL);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "LESSTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_LESS;		
		push_stack (data, STATE_LESS_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_OR_EQUAL )) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "LESSOREQUAL element not expected here", error)) {
			return;
		}

		data->current_operator = OP_LESS_EQUAL;		
		push_stack (data, STATE_LESS_OR_EQUAL);

	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "CONTAINS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_CONTAINS;		
		push_stack (data, STATE_CONTAINS);

	} else if (ELEMENT_IS (ELEMENT_RDF_STARTS_WITH)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "STARTSWITH element not expected here", error)) {
			return;
		}

		data->current_operator = OP_STARTS;		
		push_stack (data, STATE_STARTS_WITH);

	} else if (ELEMENT_IS (ELEMENT_RDF_IN_SET)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_logic_operator == LOP_AND) && (is_end_operator (state)) ),
					 context,  "IN SET element not expected here", error)) {
			return;
		}

		data->current_operator = OP_SET;		
		push_stack (data, STATE_IN_SET);



	} else if (ELEMENT_IS (ELEMENT_RDF_INTEGER)) {
		
		if (set_error_on_fail (state == STATE_PROPERTY, context,  "INTEGER element not expected here",error)) {
			return;
		}

		push_stack (data, STATE_INTEGER);


	} else if (ELEMENT_IS (ELEMENT_RDF_DATE)) {
		
		if (set_error_on_fail (state == STATE_PROPERTY, context,  "DATE element not expected here",error)) {
			return;
		}

		push_stack (data, STATE_DATE);


	} else if (ELEMENT_IS (ELEMENT_RDF_STRING)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context,  "STRING element not expected here",error)) {
			return;
		}
	
		push_stack (data, STATE_STRING);

	} else if (ELEMENT_IS (ELEMENT_RDF_FLOAT)) {

		if (set_error_on_fail (state == STATE_PROPERTY, context,  "FLOAT element not expected here",error)) {
			return;
		}
	
		push_stack (data, STATE_FLOAT);

	}
}



static char *
get_value (const char *value, gboolean quote)
{
	char *str;

	if (quote) {
		str = g_strconcat (" '", value, "' ", NULL);
		return str;
	} else {
		return g_strdup (value);
	}

}



static gboolean
build_sql (ParserData *data)
{
	ParseState	state;
	char 		*avalue, *value, *str, *sub = NULL;;
	gboolean	is_indexable_metadata;
	

	g_return_val_if_fail (data->current_field && data->current_operator != OP_NONE && data->current_value, FALSE);

	data->statement_count++; 

	state = peek_state (data);

	avalue = get_value (data->current_value, (state != STATE_END_INTEGER && state != STATE_END_FLOAT));

	FieldData *field_data = add_metadata_field (data, data->current_field, FALSE, TRUE);
	
	if (!field_data) {
		g_free (avalue);
		g_free (data->current_field);
		g_free (data->current_value);
		return FALSE;
	}

	is_indexable_metadata = (field_data->data_type == DATA_INDEX_STRING);


	if (field_data->data_type ==  DATA_DATE) {

		char *bvalue = tracker_format_date (avalue);
		tracker_log (bvalue);
		long cvalue = tracker_str_to_date (bvalue);
		tracker_log ("%d", cvalue);
		value = tracker_long_to_str (cvalue);
		g_free (bvalue);
	} else {
		value = g_strdup (avalue);
	}


	
	g_free (avalue);

	
	if (data->statement_count > 1) {
		if (data->current_logic_operator == LOP_AND) {
			data->sql_where = g_string_append (data->sql_where, " AND ");
		} else {
			if (data->current_logic_operator == LOP_OR) {
				data->sql_where = g_string_append (data->sql_where, " OR ");
			}
		}
	
	} 


	switch (data->current_operator) {

		case OP_EQUALS :

			/* replace * wildcard with SQL's like wildcard "%" */
			sub = strchr (data->current_value, '*');
			if (sub) {
				*sub = '%';
				str = g_strconcat (" (", field_data->meta_field, " like '", data->current_value, "' ) ", NULL);
			} else {
				str = g_strconcat (" (", field_data->meta_field, " = ", value, " ) ", NULL);
			}

			break;

		case OP_GREATER :

			str = g_strconcat (" (", field_data->meta_field,  " > ", value, ") ", NULL);

			break;

		case OP_GREATER_EQUAL :

			str = g_strconcat (" (", field_data->meta_field,  " >= ", value, ") ", NULL);

			break;

		case OP_LESS :

			str = g_strconcat (" (", field_data->meta_field,  " < ", value, ")", NULL);

			break;

		case OP_LESS_EQUAL :

			str = g_strconcat (" (", field_data->meta_field,  " <= ", value, ")", NULL);

			break;

		case OP_CONTAINS :

			if (!is_indexable_metadata) {

				/* replace * wildcard with SQL's like wildcard "%" */
				sub = strchr (data->current_value, '*');
				if (sub) {
					*sub = '%';
					str = g_strconcat (" (", field_data->meta_field, " like '%", data->current_value, "' ) ", NULL);
				} else {
					str = g_strconcat (" (", field_data->meta_field,  " like '%", data->current_value, "%' ) ", NULL);
				}
			} else {
				str = g_strconcat (" ("," MATCH (", field_data->meta_field, ") AGAINST ('", data->current_value, "' IN BOOLEAN MODE)) ", NULL);
			}

			break;

		case OP_STARTS :

			if (!is_indexable_metadata) {

				/* replace * wildcard with SQL's like wildcard "%" */
				sub = strchr (data->current_value, '*');
				if (sub) {
					*sub = '%';
					str = g_strconcat (" (", field_data->meta_field, " like '", data->current_value, "' ) ", NULL);
				} else {
					str = g_strconcat (" (", field_data->meta_field,  " like '", data->current_value, "%' ) ", NULL);
				}
			} else {
				str = g_strconcat (" ("," MATCH (", field_data->meta_field, ") AGAINST ('", data->current_value, "*' IN BOOLEAN MODE)) ", NULL);
			}

			break;

		case OP_SET :

			str = g_strconcat (" (FIND_IN_SET(", field_data->meta_field, ", '", data->current_value, "')) ", NULL);
		
			break;

		default :

			

			break;
	
	}

	if (str) {
		data->sql_where = g_string_append (data->sql_where, str);
		g_free (str);
	}

	g_free (data->current_field);
	data->current_field = NULL;

	g_free (data->current_value);
	data->current_value = NULL;
	
	g_free (value);
	
	return TRUE;
}

static void
end_element_handler (GMarkupParseContext *context,
			    const gchar *element_name,
			    gpointer user_data,
			    GError **error)
{
	ParserData   *data;


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
		     const gchar *text,
		     gsize text_len,
		     gpointer user_data,
		     GError **error)
{
	ParserData   *data;
	ParseState    state;

	data = user_data;
	state = peek_state (data);
	
	switch (state) {

		case STATE_INTEGER :
		case STATE_STRING :
		case STATE_DATE :
		case STATE_FLOAT :

			data->current_value = g_strstrip (g_strndup (text, text_len));
			break;

		default :
			break;
	
	}

		
}

static void
error_handler (GMarkupParseContext *context,
		      GError *error,
		      gpointer user_data)
{
	tracker_log ("Error in rdf query parse: %s", error->message);
}


char *
tracker_rdf_query_to_sql (DBConnection *db_con, const char *query, const char *service, char **fields, int field_count, const char *search_text, gboolean sort_by_service, int limit, GError *error)
{
	ParserData data;
	int 	   i;
	char       *str, *stext, *result = NULL;
	static     gboolean inited = FALSE;
  
	g_return_val_if_fail (query != NULL, NULL);


	if (!inited) {
		error_quark = g_quark_from_static_string ("RDF-parser-error-quark");
		inited = TRUE;
	}
  
	memset (&data, 0, sizeof (data));
  	data.db_con = db_con;
	data.statement_count = 0;

	data.sql_select = g_string_new ("Select DISTINCT Concat(S.Path, '/', S.Name) as uri, GetServiceName(S.ServiceTypeID) as stype ");  

	if (field_count > 0) {

		for (i=0; i<field_count; i++) {
			FieldData *field_data = add_metadata_field (&data, fields[i], TRUE, FALSE);

			if (!field_data) {
				tracker_log ("RDF Query failed : field %s not found", fields[i]);
				g_slist_foreach (data.fields, (GFunc) free_metadata_field, NULL); 
				g_slist_free (data.fields);
				g_string_free (data.sql_select, TRUE);
				return NULL;
			}
		}		
	}


	
	
	if (search_text && (strlen (search_text) > 2)) {
		gboolean	use_boolean_search;
		stext = tracker_format_search_terms (search_text, &use_boolean_search);
		data.sql_from = g_string_new  (" FROM Services S INNER JOIN ServiceMetaData M ON S.ID = M.ServiceID ");  
		str = g_strconcat (" WHERE  (S.ServiceTypeID between GetServiceTypeID('", service, "') and GetMaxServiceTypeID('", service ,"')) AND (MATCH (M.MetaDataIndexValue) AGAINST ('", stext, "' IN BOOLEAN MODE)) AND " , NULL);
		data.sql_where = g_string_new (str);	
		g_free (stext);
		g_free (str);
	} else {
		data.sql_from = g_string_new  (" FROM Services S ");  
		str = g_strconcat (" WHERE (S.ServiceTypeID between GetServiceTypeID('", service, "') and GetMaxServiceTypeID('", service ,"')) AND ", NULL);
		data.sql_where = g_string_new (str);
		g_free (str);
	}
  

	if (limit < 1) {
		limit = 1024;
	}

	if (sort_by_service) {
		data.sql_order = g_string_new (" ORDER BY S.ServiceTypeID, uri LIMIT ");  
	} else {
		data.sql_order = g_string_new (" LIMIT ");  
	}
	
	char *limit_str = tracker_int_to_str (limit);
	data.sql_order =  g_string_append (data.sql_order, limit_str);
	g_free (limit_str);

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

	if (!g_markup_parse_context_parse (data.context, query, -1, &error)) {

		g_string_free (data.sql_select, TRUE);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);
		g_string_free (data.sql_order, TRUE);

	} else {

		const GSList *tmp;
		FieldData *tmp_field;
		char *st = NULL;

		tmp = data.fields;

		while (tmp) {

			tmp_field = tmp->data;

			st = g_strconcat ( " AND (", tmp_field->alias, ".MetaDataID = ", tmp_field->id_field ," ) ",  NULL);
			data.sql_where = g_string_append (data.sql_where, st);
			g_free (st);


			if (tmp_field->is_condition) {

				st = g_strconcat ( " INNER JOIN ServiceMetaData ", tmp_field->alias, " ON S.ID = ", tmp_field->alias ,".ServiceID ",  NULL);
				data.sql_from = g_string_append (data.sql_from, st);
				g_free (st);

			} else {
				st = g_strconcat ( " LEFT OUTER JOIN ServiceMetaData ", tmp_field->alias, " ON S.ID = ", tmp_field->alias ,".ServiceID ",  NULL);
				data.sql_from = g_string_append (data.sql_from, st);
				g_free (st);
			}			

			tmp = tmp->next;
		}

		result = g_strconcat (g_string_free (data.sql_select, FALSE), " ", g_string_free (data.sql_from, FALSE), " ", g_string_free (data.sql_where, FALSE), " ", g_string_free (data.sql_order, FALSE), NULL);
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




/* returns number of tmp tables created 
int 
tracker_rdf_query_parse (DBConnection *db_con, const char *query)
{
	ParserData data;
	int 	   result;

	static     gboolean inited = FALSE;
  
	g_return_val_if_fail (query != NULL, -1);

	if (!inited) {
		error_quark = g_quark_from_static_string ("RDF-parser-error-quark");
		inited = TRUE;
	}
  
	memset (&data, 0, sizeof (data));
  
	data.parser = g_new0 (GMarkupParser, 1);

	data.parser->start_element = start_element_handler;
	data.parser->text = text_handler;
	data.parser->end_element = end_element_handler;
	data.parser->error = error_handler;

	data.db_con = db_con;
	data.current_operator = OP_NONE;
	data.current_and = FALSE;
	data.current_not = FALSE;
	data.query_okay = FALSE;
	data.temp_table_count = 0;

	data.context = g_markup_parse_context_new (data.parser, 0, &data, NULL);

  	push_stack (&data, STATE_START);

	if (!g_markup_parse_context_parse (data.context, query, -1, NULL)) {
		result = -1;
	} else {
		result = data.temp_table_count; 
	}

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
*/
