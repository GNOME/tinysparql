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

#include <libtracker-common/tracker-field-data.h>
#include <libtracker-common/tracker-log.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-utils.h>

#include "tracker-rdf-query.h"
#include "tracker-db.h"

/* RDF Query Condition
 * <rdfq:Condition>
 *	<rdfq:and>
 *		<rdfq:greaterThan>
 *			<rdfq:Property name="File:Size" />
 *			<rdf:Integer>1000000</rdf:Integer>
 *		</rdfq:greaterThan>
 *		<rdfq:equals>
 *			<rdfq:Property name="File:Path" />
 *			<rdf:String>/home/jamie</rdf:String>
 *		</rdfq:equals>
 *	</rdfq:and>
 * </rdfq:Condition>
 */

/* Main elements */
#define ELEMENT_RDF_CONDITION		"rdfq:Condition"
#define ELEMENT_RDF_PROPERTY		"rdfq:Property"

/* Operators */
#define ELEMENT_RDF_AND			"rdfq:and"
#define ELEMENT_RDF_OR			"rdfq:or"
#define ELEMENT_RDF_NOT			"rdfq:not"
#define ELEMENT_RDF_EQUALS		"rdfq:equals"
#define ELEMENT_RDF_GREATER_THAN	"rdfq:greaterThan"
#define ELEMENT_RDF_GREATER_OR_EQUAL	"rdfq:greaterOrEqual"
#define ELEMENT_RDF_LESS_THAN		"rdfq:lessThan"
#define ELEMENT_RDF_LESS_OR_EQUAL	"rdfq:lessOrEqual"

/* Extension operators - "contains" does a substring or full text
 * match, "in_Set" does string in list match
 */
#define ELEMENT_RDF_CONTAINS		"rdfq:contains"
#define ELEMENT_RDF_REGEX		"rdfq:regex"
#define ELEMENT_RDF_STARTS_WITH		"rdfq:startsWith"
#define ELEMENT_RDF_IN_SET		"rdfq:inSet"

/* Types */
#define ELEMENT_RDF_INTEGER		"rdf:Integer"
#define ELEMENT_RDF_DATE		"rdf:Date"	/* Format can
							 * be iso 8601
							 * with
							 * optional
							 * timezone
							 * "yyyy-mm-ddThh:mm:ss"
							 * or
							 * "yyyy-mm-ddThh:mm:ss+hh:mm"
							 * - most
							 * other
							 * formats are
							 * supported
							 * too
							 */
#define ELEMENT_RDF_STRING		"rdf:String"
#define ELEMENT_RDF_FLOAT		"rdf:Float"

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
	GMarkupParseContext *context;
	GMarkupParser	    *parser;
	GSList		    *stack;
	GSList		    *fields;
	gboolean	     query_okay;
	gint		     statement_count;
	LogicOperators	     current_logic_operator;
	Operators	     current_operator;
	gchar		    *current_field;
	gchar		    *current_value;
	TrackerDBInterface  *iface;
	GString		    *sql_select;
	GString		    *sql_from;
	GString		    *sql_where;
	GString		    *sql_order;
	gchar		    *service;
} ParserData;

static GQuark error_quark;

static void start_element_handler (GMarkupParseContext	*context,
				   const gchar		*element_name,
				   const gchar	       **attribute_names,
				   const gchar	       **attribute_values,
				   gpointer		 user_data,
				   GError	       **error);
static void end_element_handler   (GMarkupParseContext	*context,
				   const gchar		*element_name,
				   gpointer		 user_data,
				   GError	       **error);
static void text_handler	  (GMarkupParseContext	*context,
				   const gchar		*text,
				   gsize		 text_len,
				   gpointer		 user_data,
				   GError	       **error);
static void error_handler	  (GMarkupParseContext	*context,
				   GError		*error,
				   gpointer		 user_data);

static gboolean
is_operator (ParseState state)
{
	return
		state == STATE_EQUALS ||
		state == STATE_GREATER_THAN ||
		state == STATE_LESS_THAN ||
		state == STATE_CONTAINS ||
		state == STATE_IN_SET ||
		state == STATE_LESS_OR_EQUAL ||
		state == STATE_GREATER_OR_EQUAL ||
		state == STATE_STARTS_WITH ||
		state == STATE_REGEX;
}

static gboolean
is_end_operator (ParseState state)
{
	return
		state == STATE_END_EQUALS ||
		state == STATE_END_GREATER_THAN ||
		state == STATE_END_LESS_THAN ||
		state == STATE_END_CONTAINS ||
		state == STATE_END_IN_SET ||
		state == STATE_END_LESS_OR_EQUAL ||
		state == STATE_END_GREATER_OR_EQUAL ||
		state == STATE_END_STARTS_WITH ||
		state == STATE_REGEX;
}

static gboolean
is_logic (ParseState state)
{
	return
		state == STATE_AND ||
		state == STATE_OR ||
		state == STATE_NOT;
}

static gboolean
is_end_logic (ParseState state)
{
	return
		state == STATE_END_AND ||
		state == STATE_END_NOT ||
		state == STATE_END_OR;
}

static void
set_error (GError	       **err,
	   GMarkupParseContext	*context,
	   gint			 error_code,
	   const gchar		*format,
	   ...)
{
	gint	 line, ch;
	va_list  args;
	gchar	*str;

	g_markup_parse_context_get_position (context, &line, &ch);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_set_error (err,
		     error_quark,
		     error_code,
		     "Line %d character %d: %s",
		     line, ch, str);

	g_free (str);
}

static gboolean
set_error_on_fail (gboolean		 condition,
		   GMarkupParseContext	*context,
		   const gchar		*msg,
		   GError	       **err)
{
	if (!condition) {
		set_error (err, context, 1, msg);
		return TRUE;
	}

	return FALSE;
}

static const gchar *
get_attribute_value (const gchar *name,
		     const gchar **names,
		     const gchar **values)
{
	gint i;

	i = 0;

	while (names[i]) {
		if (strcmp (name, names[i]) == 0) {
			return values[i];
		}
		i++;
	}

	return NULL;
}

static const gchar *
get_attribute_value_required (GMarkupParseContext  *context,
			      const gchar	   *tag,
			      const gchar	   *name,
			      const gchar	  **names,
			      const gchar	  **values,
			      GError		  **error)
{
	const gchar *value;

	value = get_attribute_value (name, names, values);

	if (!value) {
		set_error (error,
			   context,
			   PARSE_ERROR,
			   "%s must have \"%s\" attribute",
			   tag, name);
	}

	return value;
}

static void
push_stack (ParserData *data, ParseState state)
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

static TrackerFieldData *
add_metadata_field (ParserData	*data,
		    const gchar *field_name,
		    gboolean	 is_select,
		    gboolean	 is_condition)
{
	TrackerFieldData *field_data;
	gboolean	  field_exists;
	GSList		 *l;

	field_exists = FALSE;
	field_data = NULL;

	/* Check if field is already in list */
	for (l = data->fields; l; l = l->next) {
		const gchar *this_field_name;

		this_field_name = tracker_field_data_get_field_name (l->data);

		if (!this_field_name) {
			continue;
		}

		if (strcasecmp (this_field_name, field_name) == 0) {
			field_data = l->data;
			field_exists = TRUE;

			if (is_condition) {
				tracker_field_data_set_is_condition (field_data, TRUE);
			}

			if (is_select) {
				if (!tracker_field_data_get_is_select (field_data)) {
					tracker_field_data_set_is_select (field_data, TRUE);
					g_string_append_printf (data->sql_select, ", %s",
								tracker_field_data_get_select_field (field_data));
				}
			}

			break;
		}
	}

	if (!field_exists) {
		field_data = tracker_db_get_metadata_field (data->iface,
							    data->service,
							    field_name,
							    g_slist_length (data->fields),
							    is_select,
							    is_condition);
		if (field_data) {
			data->fields = g_slist_prepend (data->fields, field_data);
			if (is_select) {
				g_string_append_printf (data->sql_select, ", %s",
							tracker_field_data_get_select_field (field_data));
			}
		}
	}

	return field_data;
}

static void
start_element_handler (GMarkupParseContext  *context,
		       const gchar	    *element_name,
		       const gchar	   **attribute_names,
		       const gchar	   **attribute_values,
		       gpointer		     user_data,
		       GError		   **error)
{
	ParserData *data;
	ParseState state;

	data = user_data;
	state = peek_state (data);

	if (ELEMENT_IS (ELEMENT_RDF_CONDITION)) {
		if (set_error_on_fail (state == STATE_START,
				       context,
				       "Condition element not expected here",
				       error)) {
			return;
		}

		push_stack (data, STATE_CONDITION);

	} else if (ELEMENT_IS (ELEMENT_RDF_PROPERTY)) {
		const char *name;

		if (set_error_on_fail (is_operator (state),
				       context,
				       "Property element not expected here",
				       error)) {
			return;
		}

		name = get_attribute_value_required (context,
						     "<rdfq:Property>", "name",
						     attribute_names, attribute_values,
						     error);

		if (!name) {
			return;
		} else {
			if (data->current_operator == OP_NONE) {
				set_error (error,
					   context,
					   PARSE_ERROR,
					   "no operator found for Property \"%s\"",
					   name);
				return;
			}

			data->current_field = g_strdup (name);

			push_stack (data, STATE_PROPERTY);
		}
	} else if (ELEMENT_IS (ELEMENT_RDF_AND)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       is_end_logic (state) ||
				       is_end_operator (state),
				       context,
				       "AND element not expected here",
				       error)) {
			return;
		}

		if (data->statement_count >= 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where,
								   " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where,
									   " OR ");
				}
			}
		}

		data->statement_count = 0;
		data->sql_where = g_string_append (data->sql_where, " ( ");
		data->current_logic_operator = LOP_AND;
		push_stack (data, STATE_AND);
	} else if (ELEMENT_IS (ELEMENT_RDF_OR)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       is_end_logic (state) ||
				       is_end_operator (state),
				       context,
				       "OR element not expected here",
				       error)) {
			return;
		}

		if (data->statement_count >= 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where,
								   " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where,
									   " OR ");
				}
			}
		}

		data->statement_count = 0;
		data->sql_where = g_string_append (data->sql_where, " ( ");
		data->current_logic_operator = LOP_OR;
		push_stack (data, STATE_OR);

	} else if (ELEMENT_IS (ELEMENT_RDF_NOT)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       is_end_logic (state) ||
				       is_end_operator (state),
				       context,
				       "NOT element not expected here",
				       error)) {
			return;
		}

		if (data->statement_count >= 1) {
			if (data->current_logic_operator == LOP_AND) {
				data->sql_where = g_string_append (data->sql_where,
								   " AND ");
			} else {
				if (data->current_logic_operator == LOP_OR) {
					data->sql_where = g_string_append (data->sql_where,
									   " OR ");
				}
			}
		}

		data->statement_count = 0;
		data->sql_where = g_string_append (data->sql_where, " NOT ( ");
		push_stack (data, STATE_NOT);

	} else if (ELEMENT_IS (ELEMENT_RDF_EQUALS)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "EQUALS element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_EQUALS;
		push_stack (data, STATE_EQUALS);

	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_THAN)) {

		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "GREATERTHAN element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_GREATER;
		push_stack (data, STATE_GREATER_THAN);
	} else if (ELEMENT_IS (ELEMENT_RDF_GREATER_OR_EQUAL)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "GREATEROREQUAL element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_GREATER_EQUAL;
		push_stack (data, STATE_GREATER_OR_EQUAL);
	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_THAN )) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "LESSTHAN element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_LESS;
		push_stack (data, STATE_LESS_THAN);
	} else if (ELEMENT_IS (ELEMENT_RDF_LESS_OR_EQUAL)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "LESSOREQUAL element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_LESS_EQUAL;
		push_stack (data, STATE_LESS_OR_EQUAL);
	} else if (ELEMENT_IS (ELEMENT_RDF_CONTAINS)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "CONTAINS element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_CONTAINS;
		push_stack (data, STATE_CONTAINS);
	} else if (ELEMENT_IS (ELEMENT_RDF_REGEX)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "REGEX element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_REGEX;
		push_stack (data, STATE_REGEX);
	} else if (ELEMENT_IS (ELEMENT_RDF_STARTS_WITH)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "STARTSWITH element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_STARTS;
		push_stack (data, STATE_STARTS_WITH);
	} else if (ELEMENT_IS (ELEMENT_RDF_IN_SET)) {
		if (set_error_on_fail (state == STATE_CONDITION ||
				       is_logic (state) ||
				       ((data->current_logic_operator == LOP_AND ||
					 data->current_logic_operator == LOP_OR) &&
					is_end_operator (state)),
				       context,
				       "IN SET element not expected here",
				       error)) {
			return;
		}

		data->current_operator = OP_SET;
		push_stack (data, STATE_IN_SET);
	} else if (ELEMENT_IS (ELEMENT_RDF_INTEGER)) {
		if (set_error_on_fail (state == STATE_PROPERTY,
				       context,
				       "INTEGER element not expected here",
				       error)) {
			return;
		}

		push_stack (data, STATE_INTEGER);
	} else if (ELEMENT_IS (ELEMENT_RDF_DATE)) {
		if (set_error_on_fail (state == STATE_PROPERTY,
				       context,
				       "DATE element not expected here",
				       error)) {
			return;
		}

		push_stack (data, STATE_DATE);
	} else if (ELEMENT_IS (ELEMENT_RDF_STRING)) {
		if (set_error_on_fail (state == STATE_PROPERTY,
				       context,
				       "STRING element not expected here",
				       error)) {
			return;
		}

		push_stack (data, STATE_STRING);
	} else if (ELEMENT_IS (ELEMENT_RDF_FLOAT)) {
		if (set_error_on_fail (state == STATE_PROPERTY,
				       context,
				       "FLOAT element not expected here",
				       error)) {
			return;
		}

		push_stack (data, STATE_FLOAT);
	}
}

static gchar *
get_value (const gchar *value, gboolean quote)
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
	TrackerFieldData  *field_data;
	ParseState	   state;
	gchar		  *avalue, *value, *sub;
	const gchar	  *where_field;
	GString		  *str;
	gchar		 **s;

	g_return_val_if_fail (data->current_field &&
			      data->current_operator != OP_NONE &&
			      data->current_value,
			      FALSE);

	str = g_string_new ("");

	data->statement_count++;

	state = peek_state (data);

	avalue = get_value (data->current_value, (state != STATE_END_DATE &&
						  state != STATE_END_INTEGER &&
						  state != STATE_END_FLOAT));

	field_data = add_metadata_field (data, data->current_field, FALSE, TRUE);

	if (!field_data) {
		g_free (avalue);
		g_free (data->current_field);
		g_free (data->current_value);
		data->current_field = NULL;
		data->current_value = NULL;
		return FALSE;
	}

	if (tracker_field_data_get_data_type (field_data) == TRACKER_FIELD_TYPE_DATE) {
		gchar *bvalue;
		gint   cvalue;

		bvalue = tracker_date_format (avalue);
		cvalue = tracker_string_to_date (bvalue);
		value = tracker_gint_to_string (cvalue);
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

	where_field = tracker_field_data_get_where_field (field_data);

	switch (data->current_operator) {
	case OP_EQUALS:
		sub = strchr (data->current_value, '*');
		if (sub) {
			g_string_append_printf (str, " (%s glob '%s') ",
						where_field,
						data->current_value);
		} else {
			TrackerFieldType data_type;

			data_type = tracker_field_data_get_data_type (field_data);

			g_string_append_printf (str, " (%s = %s) ",
						where_field,
						value);
		}
		break;

	case OP_GREATER:
		g_string_append_printf (str, " (%s > %s) ",
					where_field,
					value);
		break;

	case OP_GREATER_EQUAL:
		g_string_append_printf (str, " (%s >= %s) ",
					where_field,
					value);
		break;

	case OP_LESS:
		g_string_append_printf (str, " (%s < %s) ",
					where_field,
					value);
		break;

	case OP_LESS_EQUAL:
		g_string_append_printf (str, " (%s <= %s) ",
					where_field,
					value);
		break;

	case OP_CONTAINS:
		sub = strchr (data->current_value, '*');

		if (sub) {
			g_string_append_printf (str, " (%s like '%%%%%s%%%%') ",
						where_field,
						data->current_value);
		} else {
			g_string_append_printf (str, " (%s like '%%%%%s%%%%') ",
						where_field,
						data->current_value);
		}
		break;

	case OP_STARTS:
		sub = strchr (data->current_value, '*');

		if (sub) {
			g_string_append_printf (str, " (%s like '%s') ",
						where_field,
						data->current_value);
		} else {
			g_string_append_printf (str, " (%s like '%s%%%%') ",
						where_field,
						data->current_value);
		}
		break;

	case OP_REGEX:
		g_string_append_printf (str, " (%s REGEXP '%s') ",
					where_field,
					data->current_value);
		break;

	case OP_SET:
		s = g_strsplit (data->current_value, ",", 0);

		if (s && s[0]) {
			gchar **p;

			g_string_append_printf (str, " (%s in ('%s'",
						where_field,
						s[0]);

			for (p = s + 1; *p; p++) {
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
	g_message ("in rdf query parse: %s", error->message);
}


static GString *
get_select_header (const char *service)
{
	GString *result;
	int type;

	result = g_string_new ("");
	type = tracker_ontology_get_service_id_by_name (service);

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


gchar *
tracker_rdf_query_to_sql (TrackerDBInterface  *iface,
			  const gchar	      *query,
			  const gchar	      *service,
			  gchar		     **fields,
			  gint		       field_count,
			  const gchar	      *search_text,
			  const gchar	      *keyword,
			  gboolean	       sort_by_service,
			  gchar		     **sort_fields,
			  gint		       sort_field_count,
			  gboolean	       sort_desc,
			  gint		       offset,
			  gint		       limit,
			  GError	     **error)
{
	static gboolean  inited = FALSE;
	ParserData	 data;
	gchar		*result;
	gchar		*table_name;
	gboolean	 do_search = FALSE;

	g_return_val_if_fail (TRACKER_IS_DB_INTERFACE (iface), NULL);
	g_return_val_if_fail (query != NULL, NULL);
	g_return_val_if_fail (service != NULL, NULL);
	g_return_val_if_fail (fields != NULL, NULL);
	g_return_val_if_fail (search_text != NULL, NULL);
	g_return_val_if_fail (keyword != NULL, NULL);

	if (!inited) {
		error_quark = g_quark_from_static_string ("RDF-parser-error-quark");
		inited = TRUE;
	}

	memset (&data, 0, sizeof (data));
	data.iface = iface;
	data.statement_count = 0;
	data.service = (gchar*) service;
	data.sql_select = get_select_header (service);

	if (field_count > 0) {
		gint i;

		for (i = 0; i < field_count; i++) {
			TrackerFieldData *field_data;

			field_data = add_metadata_field (&data, fields[i], TRUE, FALSE);

			if (!field_data) {
				g_set_error (error,
					     error_quark,
					     PARSE_ERROR,
					     "RDF Query failed, field:'%s' not found",
					     sort_fields[i]);

				g_slist_foreach (data.fields,
						 (GFunc) g_object_unref,
						 NULL);
				g_slist_free (data.fields);
				g_string_free (data.sql_select, TRUE);

				return NULL;
			}
		}
	}

	table_name = "Services";

	data.sql_from = g_string_new ("");

	if (!tracker_is_empty_string (search_text)) {
		do_search = TRUE;
		g_string_append_printf (data.sql_from,
					"\n FROM %s S INNER JOIN SearchResults1 M ON S.ID = M.SID ",
					table_name);
	} else {
		g_string_append_printf (data.sql_from,
					"\n FROM %s S ",
					table_name);
	}

	if (!tracker_is_empty_string (keyword)) {
		gchar *keyword_metadata;

		keyword_metadata = tracker_db_metadata_get_related_names (iface,
									  "DC:Keywords");
		g_string_append_printf (data.sql_from,
					"\n INNER JOIN ServiceKeywordMetaData K ON S.ID = K.ServiceID and K.MetaDataID in (%s) and K.MetaDataValue = '%s' ",
					keyword_metadata,
					keyword);
		g_free (keyword_metadata);
	}

	data.sql_where = g_string_new ("");

	if (strlen (query) < 10) {
		g_string_append_printf (data.sql_where,
					"\n WHERE (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) ",
					service,
					service);
	} else {
		g_string_append_printf (data.sql_where,
					"\n WHERE (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) AND ",
					service,
					service);
	}

	if (limit < 1) {
		limit = 1024;
	}

	data.sql_order = g_string_new ("");

	if (sort_by_service) {
		if (do_search) {
			g_string_append_printf (data.sql_order,"M.Score desc, S.ServiceTypeID, uri ");
		} else {
			g_string_append_printf (data.sql_order,"S.ServiceTypeID, uri ");
		}
	} else {
		if (do_search) {
			g_string_append_printf (data.sql_order,"M.Score desc ");
		} else {
			/* Nothing */
		}

	}

	if (sort_field_count>0) {
		gint i;

		for (i = 0; i < sort_field_count; i++) {
			TrackerFieldData *field_data;

			field_data = add_metadata_field (&data, sort_fields[i], FALSE, FALSE);

			if (!field_data) {
				g_set_error (error,
					     error_quark,
					     PARSE_ERROR,
					     "RDF Query failed, sort field:'%s' not found",
					     sort_fields[i]);
				g_slist_foreach (data.fields,
						 (GFunc) g_object_unref,
						 NULL);
				g_slist_free (data.fields);
				g_string_free (data.sql_select, TRUE);
				g_string_free (data.sql_where, TRUE);
				g_string_free (data.sql_order, TRUE);

				return NULL;
			}

			if (i) {
				g_string_append_printf (data.sql_order, ", ");
			}

			g_string_append_printf (data.sql_order, "%s %s",
						tracker_field_data_get_select_field (field_data),
						sort_desc ? "DESC" : "ASC");
		}
	}

	if (!tracker_is_empty_string (data.sql_order->str)) {
		g_string_prepend (data.sql_order, "\n ORDER BY ");
	}

	g_string_append_printf (data.sql_order, " LIMIT ");

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

	if (!g_markup_parse_context_parse (data.context, query, -1, error)) {
		g_string_free (data.sql_select, TRUE);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);
		g_string_free (data.sql_order, TRUE);
	} else {
		GSList *l;

		for (l = data.fields; l; l = l->next) {
			if (!tracker_field_data_get_is_condition (l->data)) {
				if (tracker_field_data_get_needs_join (l->data)) {
					g_string_append_printf (data.sql_from,
								"\n LEFT OUTER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID = %s) ",
								tracker_field_data_get_table_name (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_id_field (l->data));
				}
			} else {
				gchar *related_metadata;

				related_metadata = tracker_db_metadata_get_related_names (iface,
											  tracker_field_data_get_field_name (l->data));
				g_string_append_printf (data.sql_from,
							"\n INNER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID in (%s)) ",
							tracker_field_data_get_table_name (l->data),
							tracker_field_data_get_alias (l->data),
							tracker_field_data_get_alias (l->data),
							tracker_field_data_get_alias (l->data),
							related_metadata);
				g_free (related_metadata);
			}
		}

		result = g_strconcat (data.sql_select->str,
				      " ",
				      data.sql_from->str,
				      " ",
				      data.sql_where->str,
				      " ",
				      data.sql_order->str,
				      NULL);

		g_string_free (data.sql_select, TRUE);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);
		g_string_free (data.sql_order, TRUE);
	}

	g_slist_foreach (data.fields, (GFunc) g_object_unref, NULL);
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


/*
 * The following function turns an rdf query into a filter that can be used for example
 * for getting unique values of a field with a certain query.
 * FIXME It is not pretty. The calling function needs to define Services S that is joined in this method.
 */

void
tracker_rdf_filter_to_sql (TrackerDBInterface *iface,
			   const char	      *query,
			   const char	      *service,
			   GSList	     **fields,
			   char		     **from,
			   char		     **where,
			   GError	     **error)
{
	static gboolean inited = FALSE;
	ParserData	data;

	g_return_if_fail (TRACKER_IS_DB_INTERFACE (iface));
	g_return_if_fail (service != NULL);
	g_return_if_fail (from != NULL);
	g_return_if_fail (where != NULL);

	if (!inited) {
		error_quark = g_quark_from_static_string ("RDF-parser-error-quark");
		inited = TRUE;
	}

	memset (&data, 0, sizeof (data));
	data.iface = iface;
	data.statement_count = 0;
	data.service = (char *) service;

	data.sql_from = g_string_new ("");
	data.sql_where = g_string_new ("");

	data.fields = *fields;

	if (strlen (query) < 10) {
		g_string_append_printf (data.sql_where, " (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) ", service, service);
	} else {
		g_string_append_printf (data.sql_where, " (S.ServiceTypeID in (select TypeId from ServiceTypes where TypeName = '%s' or Parent = '%s')) AND ", service, service);
	}

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

	if ( (query != NULL) && (!g_markup_parse_context_parse (data.context, query, -1, error))) {

		*from = NULL;
		*where = NULL;

		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);

	} else {
		GSList *l;

		for (l = data.fields; l; l = l->next) {
			if (!tracker_field_data_get_is_condition (l->data)) {
				if (tracker_field_data_get_needs_join (l->data)) {
					g_string_append_printf (data.sql_from,
								"\n LEFT OUTER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID = %s) ",
								tracker_field_data_get_table_name (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_alias (l->data),
								tracker_field_data_get_id_field (l->data));
				}
			} else {
				gchar *related_metadata;

				related_metadata = tracker_db_metadata_get_related_names (iface,
											  tracker_field_data_get_field_name (l->data));
				g_string_append_printf (data.sql_from,
							"\n INNER JOIN %s %s ON (S.ID = %s.ServiceID and %s.MetaDataID in (%s)) ",
							tracker_field_data_get_table_name (l->data),
							tracker_field_data_get_alias (l->data),
							tracker_field_data_get_alias (l->data),
							tracker_field_data_get_alias (l->data),
							related_metadata);
				g_free (related_metadata);
			}
		}


		*from  = g_strdup (data.sql_from->str);
		*where = g_strdup (data.sql_where->str);
		g_string_free (data.sql_from, TRUE);
		g_string_free (data.sql_where, TRUE);


	}

	*fields = data.fields;

	g_slist_free (data.stack);
	g_markup_parse_context_free (data.context);

	if (data.current_field) {
		g_free (data.current_field);
	}

	if (data.current_value) {
		g_free (data.current_value);
	}

	g_free (data.parser);
}
