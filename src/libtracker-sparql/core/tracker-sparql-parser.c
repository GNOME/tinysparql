/*
 * Copyright (C) 2008-2010, Nokia
 * Copyright (C) 2018, Red Hat Inc.
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
#include "config.h"

#include "tracker-sparql-parser.h"
#include "tracker-sparql-grammar.h"

#include <libtracker-sparql/tracker-connection.h>
#include <string.h>

typedef struct _TrackerRuleState TrackerRuleState;
typedef struct _TrackerParserState TrackerParserState;
typedef struct _TrackerGrammarParser TrackerGrammarParser;

#define NODES_PER_CHUNK 128
#define RULE_STATE_DEFAULT_SIZE 128
#define SNIPPET_LENGTH 30

struct _TrackerRuleState {
	const TrackerGrammarRule *rule;
	TrackerParserNode *node;
	gssize start_pos;
	gint cur_child;
	guint visited  : 1;
	guint finished : 1;
};

struct _TrackerNodeTree {
	GPtrArray *chunks;
	gint current;
	TrackerParserNode *root;
};

struct _TrackerParserNode {
	GNode node;
	const TrackerGrammarRule *rule;
	gssize start;
	gssize end;
	guint n_children;
	gint cur_child;
};

struct _TrackerParserState {
	TrackerNodeTree *node_tree;
	gssize current;
	struct {
		TrackerRuleState *rules;
		guint array_size;
		guint len;
		gint64 last_matched;
		TrackerParserNode *last_matched_node;
	} rule_states;

	GPtrArray *error_rules;
	gssize error_len;
};

struct _TrackerGrammarParser {
	const gchar *query;
	gssize query_len;
};

static TrackerNodeTree *
tracker_node_tree_new (void)
{
	TrackerNodeTree *tree;

	tree = g_slice_new0 (TrackerNodeTree);
	tree->chunks = g_ptr_array_new_with_free_func (g_free);

	return tree;
}

void
tracker_node_tree_free (TrackerNodeTree *tree)
{
	g_ptr_array_unref (tree->chunks);
	g_slice_free (TrackerNodeTree, tree);
}

TrackerParserNode *
tracker_node_tree_get_root (TrackerNodeTree *tree)
{
	return tree->root;
}

static inline TrackerParserNode *
tracker_node_tree_allocate (TrackerNodeTree *tree)
{
	TrackerParserNode *node_array;
	guint chunk, chunk_idx;

	chunk = tree->current / NODES_PER_CHUNK;
	chunk_idx = tree->current % NODES_PER_CHUNK;
	tree->current++;

	if (chunk >= tree->chunks->len) {
		node_array = g_new0 (TrackerParserNode, NODES_PER_CHUNK);
		g_ptr_array_add (tree->chunks, node_array);
	} else {
		node_array = g_ptr_array_index (tree->chunks, chunk);
	}

	return &node_array[chunk_idx];
}

static void
tracker_node_tree_reset (TrackerNodeTree   *tree,
                         TrackerParserNode *node)
{
	gint i;

	if (!node)
		return;

	g_node_unlink ((GNode *) node);

	for (i = tree->chunks->len - 1; i >= 0; i--) {
		TrackerParserNode *range = g_ptr_array_index (tree->chunks, i);

		if (node >= range && node < &range[NODES_PER_CHUNK]) {
			guint pos = node - range;
			tree->current = (i * NODES_PER_CHUNK) + pos;
			return;
		}
	}

	g_assert_not_reached ();
}

static inline void
tracker_parser_node_reset (TrackerParserNode        *node,
                           const TrackerGrammarRule *rule,
                           const TrackerParserState *state)
{
	node->rule = rule;
	node->start = node->end = state->current;

	switch (rule->type) {
	case RULE_TYPE_RULE:
	case RULE_TYPE_SEQUENCE:
	case RULE_TYPE_GT0:
	case RULE_TYPE_GTE0:
	case RULE_TYPE_OPTIONAL:
	case RULE_TYPE_OR:
		node->cur_child = -1;
		break;
	case RULE_TYPE_LITERAL:
	case RULE_TYPE_TERMINAL:
		break;
	case RULE_TYPE_NIL:
		g_assert_not_reached ();
		break;
	}
}

static inline TrackerParserNode *
tracker_parser_node_new (const TrackerGrammarRule *rule,
                         const TrackerParserState *state)
{
	TrackerParserNode *node;

	node = tracker_node_tree_allocate (state->node_tree);
	node->node = (GNode) { node, 0, };
	tracker_parser_node_reset (node, rule, state);

	return node;
}

static void
tracker_grammar_parser_init (TrackerGrammarParser *parser,
                             const gchar          *query,
                             gsize                 len)
{
	parser->query = query;
	parser->query_len = len;
}

static void
tracker_parser_state_push (TrackerParserState       *state,
                           const TrackerGrammarRule *rule)
{
	TrackerRuleState *rule_state;

	state->rule_states.len++;

	if (state->rule_states.len > state->rule_states.array_size) {
		state->rule_states.array_size <<= 1;
		state->rule_states.rules = g_realloc_n (state->rule_states.rules,
		                                        state->rule_states.array_size,
		                                        sizeof (TrackerRuleState));
	}

	rule_state = &state->rule_states.rules[state->rule_states.len - 1];

	rule_state->rule = rule;
	rule_state->node = NULL;
	rule_state->start_pos = state->current;
	rule_state->cur_child = 0;
	rule_state->visited = rule_state->finished = FALSE;
}

static TrackerRuleState *
tracker_parser_state_peek (TrackerParserState *state)
{
	return &state->rule_states.rules[state->rule_states.len - 1];
}

static TrackerParserNode *
tracker_parser_state_pop (TrackerParserState *state)
{
	TrackerRuleState *rule_state;
	TrackerParserNode *node = NULL;

	rule_state = tracker_parser_state_peek (state);
	if (rule_state->node) {
		node = rule_state->node;
		node->end = state->current;

		if (node == state->rule_states.last_matched_node) {
			state->rule_states.last_matched_node =
				(TrackerParserNode *) node->node.parent;
		}
	}

	state->rule_states.len--;
	state->rule_states.last_matched = MIN (state->rule_states.last_matched,
	                                       state->rule_states.len);

	return node;
}

static const TrackerGrammarRule *
tracker_parser_state_peek_current_rule (TrackerParserState *state)
{
	TrackerRuleState *rule_state;

	rule_state = tracker_parser_state_peek (state);

	return rule_state->rule;
}

static const TrackerGrammarRule *
tracker_parser_state_lookup_child (TrackerParserState *state)
{
	TrackerRuleState *rule_state;
	const TrackerGrammarRule *children;

	rule_state = tracker_parser_state_peek (state);

	if (rule_state->finished)
		return NULL;

	if (rule_state->rule->type == RULE_TYPE_LITERAL ||
	    rule_state->rule->type == RULE_TYPE_TERMINAL)
		return NULL;

	children = tracker_grammar_rule_get_children (rule_state->rule);
	if (!children)
		return NULL;

	return &children[rule_state->cur_child];
}

static inline gboolean
tracker_parser_state_next_child (TrackerParserState *state,
                                 gboolean            success)
{
	const TrackerGrammarRule *children;
	TrackerRuleState *rule_state;

	rule_state = tracker_parser_state_peek (state);

	if (rule_state->finished)
		return FALSE;

	if (success) {
		if (rule_state->rule->type == RULE_TYPE_OR) {
			/* Successful OR rules are satisfied already */
			rule_state->finished = TRUE;
			return FALSE;
		} else if (rule_state->rule->type == RULE_TYPE_GT0 ||
		           rule_state->rule->type == RULE_TYPE_GTE0) {
			/* Successful + and * rules are evaluated again */
			return TRUE;
		}
	} else {
		if (rule_state->rule->type == RULE_TYPE_GT0 ||
		    rule_state->rule->type == RULE_TYPE_GTE0) {
			rule_state->finished = TRUE;
			return FALSE;
		}
	}

	children = tracker_grammar_rule_get_children (rule_state->rule);
	if (!children)
		return FALSE;

	rule_state->cur_child++;
	rule_state->finished = children[rule_state->cur_child].type == RULE_TYPE_NIL;

	return !rule_state->finished;
}

static TrackerParserNode *
tracker_parser_state_transact_match (TrackerParserState *state)
{
	TrackerParserNode *parser_node = state->rule_states.last_matched_node;
	guint i = 0;

	for (i = state->rule_states.last_matched; i < state->rule_states.len; i++) {
		TrackerRuleState *rule_state = &state->rule_states.rules[i];

		rule_state->visited = TRUE;
		state->rule_states.last_matched = i;

		if (rule_state->rule->type != RULE_TYPE_LITERAL &&
		    rule_state->rule->type != RULE_TYPE_TERMINAL &&
		    rule_state->rule->type != RULE_TYPE_RULE)
			continue;

		if (rule_state->node == NULL) {
			rule_state->node = tracker_parser_node_new (rule_state->rule, state);
			if (parser_node) {
				g_node_append ((GNode *) parser_node,
				               (GNode *) rule_state->node);
			}
		}

		parser_node = rule_state->node;
		state->rule_states.last_matched_node = parser_node;
	}

	return parser_node;
}

static void
tracker_parser_state_take_error (TrackerParserState       *state,
                                 const TrackerGrammarRule *rule)
{
	if (state->current < state->error_len) {
		return;
	}

	/* If we advance in parsing, reset the expect token stack */
	if (state->current > state->error_len)
		g_ptr_array_set_size (state->error_rules, 0);

	if (rule->type == RULE_TYPE_LITERAL ||
	    rule->type == RULE_TYPE_TERMINAL) {
		/* We only want literals and terminals here, these are the
		 * actual string tokens.
		 */
		g_ptr_array_add (state->error_rules, (gpointer) rule);
	}

	state->error_len = state->current;
}

static void
tracker_parser_state_forward (TrackerParserState   *state,
			      TrackerGrammarParser *parser,
			      gssize                len)
{
	g_assert (len >= 0 && state->current + len <= parser->query_len);
	state->current += len;
}

static void
tracker_parser_state_rewind (TrackerParserState *state)
{
	TrackerRuleState *rule_state;

	rule_state = tracker_parser_state_peek (state);
	g_assert (rule_state->start_pos >= 0 && rule_state->start_pos <= state->current);
	state->current = rule_state->start_pos;
}

static void
tracker_parser_state_skip_whitespace (TrackerParserState   *state,
				      TrackerGrammarParser *parser)
{
	while (state->current < parser->query_len) {
		/* Skip comments too */
		if (parser->query[state->current] == '#') {
			while (state->current < parser->query_len &&
			       parser->query[state->current] != '\n') {
				tracker_parser_state_forward (state, parser, 1);
			}
		}

		if (parser->query[state->current] != ' ' &&
		    parser->query[state->current] != '\n' &&
		    parser->query[state->current] != '\t')
			break;

		tracker_parser_state_forward (state, parser, 1);
	}
}

static gboolean
tracker_grammar_parser_apply_rule_literal (TrackerGrammarParser     *parser,
                                           TrackerParserState       *state,
                                           const TrackerGrammarRule *rule)
{
	TrackerParserNode *node;
	gboolean next_isalnum;
	gssize len;

	if (rule->string[0] != parser->query[state->current] &&
	    rule->string[0] != g_ascii_tolower (parser->query[state->current]))
		goto error;

	len = strlen (rule->string);
	g_assert (len > 0);

	if (state->current + len > parser->query_len)
		goto error;

	if (len > 1 &&
	    rule->string[len - 1] != parser->query[state->current + len - 1] &&
	    rule->string[len - 1] != g_ascii_tolower (parser->query[state->current + len - 1]))
		goto error;

	next_isalnum = g_ascii_isalnum (parser->query[state->current + len]);

	/* Special case for '?', which may be a property path operator, and
	 * the beginning of VAR1. If the next char is alphanumeric, it's probably
	 * the latter.
	 */
	if (rule->data.literal == LITERAL_PATH_OPTIONAL && next_isalnum)
		goto error;

	/* Generic check for other literals, if the literal is alphanumeric, and
	 * the remaining text starts with alphanumeric, probably that was not it.
	 */
	if (rule->string[0] >= 'a' && rule->string[0] <= 'z' && next_isalnum)
		goto error;

	if (len > 1 &&
	    g_ascii_strncasecmp (rule->string, &parser->query[state->current], len) != 0)
		goto error;

	node = tracker_parser_state_transact_match (state);
	tracker_parser_state_forward (state, parser, len);
	node->end = state->current;
	return TRUE;

error:
	tracker_parser_state_take_error (state, rule);
	return FALSE;
}

static gboolean
tracker_grammar_parser_apply_rule_terminal (TrackerGrammarParser     *parser,
                                            TrackerParserState       *state,
                                            const TrackerGrammarRule *rule)
{
	TrackerParserNode *node;
	TrackerTerminalFunc func;
	const gchar *str, *end;

	str = &parser->query[state->current];

	if (state->current == parser->query_len || str[0] == '\0') {
		tracker_parser_state_take_error (state, rule);
		return FALSE;
	}

	func = tracker_grammar_rule_get_terminal_func (rule);

	if (!func (str, &parser->query[parser->query_len], &end)) {
		tracker_parser_state_take_error (state, rule);
		return FALSE;
	}

	node = tracker_parser_state_transact_match (state);
	tracker_parser_state_forward (state, parser, end - str);
	node->end = state->current;
	return TRUE;
}

static gboolean
tracker_grammar_parser_apply_rule (TrackerGrammarParser     *parser,
                                   TrackerParserState       *state,
                                   const TrackerGrammarRule *rule)
{
	switch (rule->type) {
	case RULE_TYPE_LITERAL:
		return tracker_grammar_parser_apply_rule_literal (parser,
		                                                  state, rule);
	case RULE_TYPE_TERMINAL:
		return tracker_grammar_parser_apply_rule_terminal (parser,
		                                                   state, rule);
	case RULE_TYPE_RULE:
	case RULE_TYPE_SEQUENCE:
	case RULE_TYPE_GT0:
	case RULE_TYPE_GTE0:
	case RULE_TYPE_OPTIONAL:
	case RULE_TYPE_OR:
		return TRUE;
	case RULE_TYPE_NIL:
		g_assert_not_reached ();
		return FALSE;
	}

	g_assert_not_reached ();
}

static gboolean
tracker_parser_state_iterate (TrackerParserState   *state,
                              TrackerGrammarParser *parser,
                              gboolean              try_children)
{
	const TrackerGrammarRule *child;

	if (try_children) {
		/* Try iterating into children first */
		child = tracker_parser_state_lookup_child (state);

		if (child) {
			tracker_parser_state_skip_whitespace (state, parser);
			tracker_parser_state_push (state, child);
			return TRUE;
		}
	}

	tracker_parser_state_pop (state);

	/* Find the first parent that has a next child to handle */
	while (state->rule_states.len > 0) {
		if (tracker_parser_state_next_child (state, TRUE)) {
			child = tracker_parser_state_lookup_child (state);
			tracker_parser_state_skip_whitespace (state, parser);
			tracker_parser_state_push (state, child);
			return TRUE;
		}

		tracker_parser_state_pop (state);
	}

	return FALSE;
}

static gboolean
tracker_parser_state_rollback (TrackerParserState   *state,
                               TrackerGrammarParser *parser)
{
	const TrackerGrammarRule *rule, *child;
	TrackerParserNode *node, *discard;

	/* Reset state to retry again the failed portions */
	tracker_parser_state_rewind (state);
	discard = tracker_parser_state_pop (state);

	while (state->rule_states.len > 0) {
		rule = tracker_parser_state_peek_current_rule (state);

		switch (rule->type) {
		case RULE_TYPE_OR:
			if (tracker_parser_state_next_child (state, FALSE)) {
				tracker_node_tree_reset (state->node_tree, discard);
				child = tracker_parser_state_lookup_child (state);
				tracker_parser_state_skip_whitespace (state, parser);
				tracker_parser_state_push (state, child);
				return TRUE;
			}
			break;
		case RULE_TYPE_GT0:
			/* If we errored out the first time we
			 * parse ()+, raise an error.
			 */
			if (!tracker_parser_state_peek (state)->visited)
				break;

			/* Fall through */
		case RULE_TYPE_GTE0:
		case RULE_TYPE_OPTIONAL:
			tracker_parser_state_iterate (state, parser, FALSE);
			tracker_node_tree_reset (state->node_tree, discard);
			return TRUE;
		case RULE_TYPE_RULE:
			tracker_parser_state_take_error (state, rule);
			break;
		default:
			break;
		}

		/* Reset state to retry again the failed portions */
		tracker_parser_state_rewind (state);
		node = tracker_parser_state_pop (state);
		if (node)
			discard = node;
	}

	return FALSE;
}

static gboolean
tracker_grammar_parser_read (TrackerGrammarParser *parser,
                             TrackerParserState   *state)
{
	while (state->rule_states.len > 0) {
		const TrackerGrammarRule *rule;

		rule = tracker_parser_state_peek_current_rule (state);

		if (tracker_grammar_parser_apply_rule (parser, state, rule)) {
			if (!tracker_parser_state_iterate (state, parser, TRUE))
				break;
		} else {
			if (!tracker_parser_state_rollback (state, parser))
				break;
		}
	}

	tracker_parser_state_skip_whitespace (state, parser);

	return (g_node_first_child ((GNode *) state->node_tree->root) &&
		parser->query[state->current] == '\0');
}

static void
append_rule (GString                  *str,
             const TrackerGrammarRule *rule)
{
	if (rule->type == RULE_TYPE_LITERAL)
		g_string_append_printf (str, "\'%s\'", rule->string);
	else if (rule->type == RULE_TYPE_TERMINAL)
		g_string_append_printf (str, "%s", rule->string);
}

static guint
rule_hash (gconstpointer a)
{
	const TrackerGrammarRule *rule_a = a;

	return rule_a->type << 16 & rule_a->data.literal;
}

static gboolean
rule_equals (gconstpointer a,
             gconstpointer b)
{
	const TrackerGrammarRule *rule_a = a;
	const TrackerGrammarRule *rule_b = b;

	if (rule_a->type != rule_b->type)
		return FALSE;

	switch (rule_a->type) {
	case RULE_TYPE_LITERAL:
		return rule_a->data.literal == rule_b->data.literal;
	case RULE_TYPE_TERMINAL:
		return rule_a->data.terminal == rule_b->data.terminal;
	default:
		return FALSE;
	}
}

static gchar *
get_error_snippet (TrackerGrammarParser *parser,
                   TrackerParserState   *state)
{
	gssize start, end, error_pos;
	gchar *sample, *snippet;

	start = state->error_len - SNIPPET_LENGTH / 2;
	end = state->error_len + SNIPPET_LENGTH / 2;
	error_pos = SNIPPET_LENGTH / 2;

	if (start < 0) {
		end += ABS (start);
		error_pos += start;
		start = 0;
	}

	if (end > parser->query_len) {
		gint move = end - parser->query_len;

		if (start >= move) {
			start -= move;
			error_pos += move;
		}

		end -= move;
	}

	sample = g_strndup (&parser->query[start], end - start);
	snippet = g_strdup_printf ("%s%s%s\n%*c‸",
	                           start == 0 ? " " : "…",
	                           sample,
	                           end == parser->query_len ? " " : "…",
	                           (gint) error_pos + 1, ' ');
	g_free (sample);

	return snippet;
}

static void
tracker_parser_state_propagate_error (TrackerGrammarParser  *parser,
                                      TrackerParserState    *state,
                                      GError               **error)
{
	const TrackerGrammarRule *rule;
	GString *str = g_string_new (NULL);
	GHashTable *repeated;
	gchar *snippet;

	if (state->current > state->error_len) {
		/* The errors gathered are outdated, but we are propagating
		 * an error. This means we have reached the end of the string.
		 */
		g_ptr_array_set_size (state->error_rules, 0);
		state->error_len = state->current;
	}

	g_string_append_printf (str, "Parser error at byte %ld, expected ",
	                        state->error_len);

	if (state->error_rules->len == 0) {
		g_string_append (str, "'\\0'");
	} else if (state->error_rules->len == 1) {
		rule = g_ptr_array_index (state->error_rules, 0);
		append_rule (str, rule);
	} else {
		guint i;

		g_string_append (str, "one of ");
		repeated = g_hash_table_new (rule_hash, rule_equals);

		for (i = 0; i < state->error_rules->len; i++) {
			rule = g_ptr_array_index (state->error_rules, i);

			if (g_hash_table_contains (repeated, rule))
				continue;
			if (i != 0)
				g_string_append (str, ", ");

			append_rule (str, rule);
			g_hash_table_add (repeated, (gpointer) rule);
		}

		g_hash_table_unref (repeated);
	}

	snippet = get_error_snippet (parser, state);
	g_string_append_printf (str, ":\n%s", snippet);
	g_free (snippet);

	g_set_error (error,
	             TRACKER_SPARQL_ERROR,
	             TRACKER_SPARQL_ERROR_PARSE,
	             "%s", str->str);

	g_string_free (str, TRUE);
}

TrackerNodeTree *
tracker_grammar_parser_apply (TrackerGrammarParser      *parser,
                              const TrackerGrammarRule  *rule,
                              gsize                     *len_out,
                              GError                   **error)
{
	TrackerParserState state = { 0, };

	state.node_tree = tracker_node_tree_new ();
	state.rule_states.array_size = RULE_STATE_DEFAULT_SIZE;
	state.rule_states.rules = g_new0 (TrackerRuleState,
	                                  state.rule_states.array_size);
	state.error_rules = g_ptr_array_new ();

	tracker_parser_state_push (&state, rule);
	state.node_tree->root = tracker_parser_state_transact_match (&state);

	if (!tracker_grammar_parser_read (parser, &state)) {
		tracker_parser_state_propagate_error (parser, &state, error);
		tracker_node_tree_free (state.node_tree);
		g_free (state.rule_states.rules);
		return NULL;
	}

	if (len_out)
		*len_out = state.current;

	g_ptr_array_unref (state.error_rules);
	g_free (state.rule_states.rules);

	return state.node_tree;
}

TrackerNodeTree *
tracker_sparql_parse_query (const gchar  *query,
                            gssize        len,
                            gsize        *len_out,
                            GError      **error)
{
	TrackerGrammarParser parser;
	TrackerNodeTree *tree;

	g_return_val_if_fail (query != NULL, NULL);

	if (len < 0)
		len = strlen (query);

	tracker_grammar_parser_init (&parser, query, len);
	tree = tracker_grammar_parser_apply (&parser, NAMED_RULE (QueryUnit), len_out, error);

	return tree;
}

TrackerNodeTree *
tracker_sparql_parse_update (const gchar  *query,
                             gssize        len,
                             gsize        *len_out,
                             GError      **error)
{
	TrackerGrammarParser parser;
	TrackerNodeTree *tree;

	g_return_val_if_fail (query != NULL, NULL);

	if (len < 0)
		len = strlen (query);

	tracker_grammar_parser_init (&parser, query, len);
	tree = tracker_grammar_parser_apply (&parser, NAMED_RULE (UpdateUnit), len_out, error);

	return tree;
}

const TrackerGrammarRule *
tracker_parser_node_get_rule (TrackerParserNode *node)
{
	return node->rule;
}

gboolean
tracker_parser_node_get_extents (TrackerParserNode *node,
				 gssize            *start,
				 gssize            *end)
{
	if (start)
		*start = node->start;
	if (end)
		*end = node->end;

	return node->end != node->start;
}

TrackerParserNode *
tracker_sparql_parser_tree_find_first (TrackerParserNode *node,
                                       gboolean           leaves_only)
{
	g_return_val_if_fail (node != NULL, NULL);

	while (node) {
		if ((!leaves_only && node->rule->type == RULE_TYPE_RULE) ||
		    node->rule->type == RULE_TYPE_LITERAL ||
		    node->rule->type == RULE_TYPE_TERMINAL) {
			return node;
		} else if (!node->node.children) {
			return tracker_sparql_parser_tree_find_next (node, leaves_only);
		}

		node = (TrackerParserNode *) node->node.children;
	}

	return NULL;
}

TrackerParserNode *
tracker_sparql_parser_tree_find_next (TrackerParserNode *node,
                                      gboolean           leaves_only)
{
	g_return_val_if_fail (node != NULL, NULL);

	while (TRUE) {
		if (node->node.children)
			node = (TrackerParserNode *) node->node.children;
		else if (node->node.next)
			node = (TrackerParserNode *) node->node.next;
		else if (node->node.parent) {
			node = (TrackerParserNode *) node->node.parent;

			/* Traverse up all parents till we find one
			 * with a next node.
			 */
			while (node) {
				if (node->node.next) {
					node = (TrackerParserNode *) node->node.next;
					break;
				}

				node = (TrackerParserNode *) node->node.parent;
			}
		}

		if (!node)
			break;

		if ((!leaves_only && node->rule->type == RULE_TYPE_RULE) ||
		    node->rule->type == RULE_TYPE_LITERAL ||
		    node->rule->type == RULE_TYPE_TERMINAL) {
			return node;
		}
	}

	return NULL;
}
