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

/* RDF Query 
<rdfq:rdfquery>
  <rdfq:From eachResource="Files">
  <rdfq:Select>
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
 </rdfq:Select>
  </rdfq:From>
</rdfq:rdfquery>

*/

#define SQL_METADATA_START		" CREATE Temporary Table TMP" 
#define SQL_METADATA_END		" ENGINE = MEMORY SELECT FileID From FileMetaData WHERE MetaDataID = "
#define SQL_CONTAINS_INDEXABLE_START	" (MATCH (MetaDataIndexValue) AGAINST ('"
#define SQL_CONTAINS_INDEXABLE_END	"' IN BOOLEAN MODE)) "


/* main elements */
#define ELEMENT_RDF_QUERY 		"rdfq:rdfquery"
#define ELEMENT_RDF_FROM 		"rdfq:From"
#define ELEMENT_RDF_SELECT 		"rdfq:Select"
#define ELEMENT_RDF_CONDITION 		"rdfq:Condition"
#define ELEMENT_RDF_PROPERTY 		"rdfq:Property"

/* operators */
#define ELEMENT_RDF_AND 		"rdfq:and"
#define ELEMENT_RDF_NOT 		"rdfq:not"
#define ELEMENT_RDF_EQUALS 		"rdfq:equals"
#define ELEMENT_RDF_GREATER_THAN	"rdfq:greaterThan"
#define ELEMENT_RDF_LESS_THAN 		"rdfq:lessThan"

/* extension operators - "contains" does a substring match, "in_Set" does string in list match */
#define ELEMENT_RDF_CONTAINS 		"rdfq:contains"
#define ELEMENT_RDF_IN_SET		"rdfq:inSet"

/* types */
#define ELEMENT_RDF_INTEGER 		"rdf:Integer"
#define ELEMENT_RDF_DATE 		"rdf:Date"	/* format is "yyyy-mm-dd hh:mm:ss" */
#define ELEMENT_RDF_STRING 		"rdf:String"

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
	STATE_NOT,
	STATE_END_NOT,
	STATE_EQUALS,
	STATE_END_EQUALS,
	STATE_GREATER_THAN,
	STATE_END_GREATER_THAN,
	STATE_LESS_THAN,
	STATE_END_LESS_THAN,
	STATE_CONTAINS,
	STATE_END_CONTAINS,
	STATE_IN_SET,
	STATE_END_IN_SET,
	STATE_INTEGER,
	STATE_END_INTEGER,
	STATE_STRING,
	STATE_END_STRING,
	STATE_DATE,
	STATE_END_DATE
} ParseState;

typedef enum {
	OP_NONE,
	OP_EQUALS,
	OP_GREATER,
	OP_LESS,
	OP_CONTAINS,
	OP_SET
} Operators;

typedef struct {
	GMarkupParseContext 	*context;
	GMarkupParser       	*parser;
	GSList 			*stack;
	gboolean		query_okay;
	gboolean		current_and;
	gboolean		current_not;
	Operators		current_operator;
	char 			*current_field;
	char			*current_value;
	DBConnection		*db_con;
	int			temp_table_count;
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
			state == STATE_CONTAINS || state == STATE_IN_SET;

}

static gboolean
is_end_operator (ParseState state)
{
	
	return state == STATE_END_EQUALS || state == STATE_END_GREATER_THAN || state == STATE_END_LESS_THAN ||
			state == STATE_END_CONTAINS || state == STATE_END_IN_SET;

}


static gboolean
is_value (ParseState state)
{
	
	return state == STATE_INTEGER || state == STATE_STRING || state == STATE_DATE;

}

static gboolean
is_end_value (ParseState state)
{
	
	return state == STATE_END_INTEGER || state == STATE_END_STRING || state == STATE_END_DATE;

}

static gboolean
is_logic (ParseState state)
{
	
	return state == STATE_AND || state == STATE_NOT;

}

static gboolean
is_end_logic (ParseState state)
{
	
	return state == STATE_END_AND || state == STATE_END_NOT;

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

		data->current_and = TRUE;
		push_stack (data, STATE_AND);
		
		
	} else if (ELEMENT_IS (ELEMENT_RDF_NOT)) {

		if (set_error_on_fail ((state == STATE_CONDITION || is_logic (state) || is_end_logic (state) || is_end_operator (state)), context,   "NOT element not expected here",error)) {
			return;
		}

		data->current_not = TRUE;
		push_stack (data, STATE_NOT);

	} else if (ELEMENT_IS (ELEMENT_RDF_EQUALS)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_and) && (is_end_operator (state)) ),
					 context,  "EQUALS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_EQUALS;
		push_stack (data, STATE_EQUALS);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_THAN)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_and) && (is_end_operator (state)) ),
					 context,  "GREATERTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_GREATER;
		push_stack (data, STATE_GREATER_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_and) && (is_end_operator (state)) ),
					 context,  "LESSTHAN element not expected here", error)) {
			return;
		}

		data->current_operator = OP_LESS;		
		push_stack (data, STATE_LESS_THAN);

	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_and) && (is_end_operator (state)) ),
					 context,  "CONTAINS element not expected here", error)) {
			return;
		}

		data->current_operator = OP_CONTAINS;		
		push_stack (data, STATE_CONTAINS);

	} else if (ELEMENT_IS (ELEMENT_RDF_IN_SET)) {
		
		if (set_error_on_fail ( state == STATE_CONDITION || is_logic (state) ||
					((data->current_and) && (is_end_operator (state)) ),
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
	char 		*query, *tmp_no, *value, *str, *str2, *field_name, *sub = NULL;;
	FieldDef 	*def;
	gboolean	is_indexable_metadata;
	

	g_return_val_if_fail (data->current_field && data->current_operator != OP_NONE && data->current_value, FALSE);

	state = peek_state (data);

	value = get_value (data->current_value, state != STATE_END_INTEGER);

	data->temp_table_count ++;

	tmp_no = g_strdup_printf ("%d", data->temp_table_count);

	def = tracker_db_get_field_def	(data->db_con, data->current_field);

	if (def->indexable) {

		is_indexable_metadata = TRUE;

		field_name = g_strdup ("MetaDataIndexValue");

	} else if (def->type != DATA_INTEGER) {

		is_indexable_metadata = FALSE;

		field_name = g_strdup ("MetaDataValue");
	} else {
		is_indexable_metadata = FALSE;

		field_name = g_strdup ("MetaDataIntegerValue");

	}

	str = g_strconcat (SQL_METADATA_START, def->id, SQL_METADATA_END , " AND ", NULL);
	

	switch (data->current_operator) {

		case OP_EQUALS :

			/* replace * wildcard with SQL's like wildcard "%" */
			sub = strchr (data->current_value, '*');
			if (sub) {
				*sub = '%';
				str = g_strconcat (" (", field_name, " like '", data->current_value, "' ) ", NULL);
			} else {
				str = g_strconcat (" (", field_name, " = ", value, " ) ", NULL);
			}
			
			break;

		case OP_GREATER :

			str = g_strconcat (" (", field_name,  " > ", value, ") ", NULL);

			break;

		case OP_LESS :

			str = g_strconcat (" (", field_name,  " < ", value, ")", NULL);

			break;

		case OP_CONTAINS :

			if (!is_indexable_metadata) {

				/* replace * wildcard with SQL's like wildcard "%" */
				sub = strchr (data->current_value, '*');
				if (sub) {
					*sub = '%';
					str = g_strconcat (" (", field_name, " like '%", data->current_value, "' ) ", NULL);
				} else {
					str = g_strconcat (" (", field_name,  " like '%", data->current_value, "%' ) ", NULL);
				}
			} else {
				str = g_strconcat (" ("," MATCH (MetaDataIndexValue) AGAINST ('", data->current_value, "' IN BOOLEAN MODE)) ", NULL);
			}

			break;

		case OP_SET :

			str = g_strconcat (" (FIND_IN_SET(", field_name, ", '", data->current_value, "')) ", NULL);
		
			break;

		default :

			data->temp_table_count --;

			break;
	
	}

	if (data->current_not) {
		query = g_strconcat (SQL_METADATA_START, tmp_no  , SQL_METADATA_END ,def->id, " AND NOT ", str, NULL);	
	} else {
		query = g_strconcat (SQL_METADATA_START, tmp_no  , SQL_METADATA_END ,def->id, " AND ", str, NULL);	
	}

	/* make sure no existing temp table is around */
	str2 = g_strconcat ("DROP TEMPORARY TABLE IF EXISTS TMP", tmp_no, NULL);
	tracker_exec_sql (data->db_con->db, str2);
	g_free (str2);

	tracker_exec_sql (data->db_con->db, query);

	g_free (data->current_field);
	data->current_field = NULL;

	g_free (data->current_value);
	data->current_value = NULL;

	g_free (str);
	g_free (value);
	g_free (field_name);
	g_free (tmp_no);
	g_free (query);

	tracker_db_free_field_def (def);

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
		
		pop_stack_until (data, STATE_AND);
		
		if (peek_state (data) != STATE_AND) {
			data->current_and = FALSE;
		}

	
	} else if (ELEMENT_IS (ELEMENT_RDF_NOT)) {
		
		pop_stack_until (data, STATE_NOT);

		if (peek_state (data) != STATE_NOT) {
			data->current_not = FALSE;
		}
		

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

	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_LESS_THAN );

	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {

		if (!build_sql (data)) {
			set_error (error, context, 1, "parse error");
			return;
		}

		push_stack (data, STATE_END_CONTAINS);


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


/* returns number of tmp tables created */
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
