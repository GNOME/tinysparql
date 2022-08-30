/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

static gchar *tests_data_dir = NULL;

typedef struct _TestInfo TestInfo;

struct _TestInfo {
	const gchar *test_name;
	const gchar *data;
	gboolean expect_query_error;
	gboolean expect_update_error;
};

const TestInfo tests[] = {
	{ "aggregates/aggregate-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-count-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-count-2", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-count-3", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-distinct-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-group-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-group-2", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-group-3", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-group-as-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-group-having-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-sample-1", "aggregates/data-1", FALSE },
	{ "aggregates/aggregate-sample-2", "aggregates/data-1", FALSE },
	{ "algebra/two-nested-opt", "algebra/two-nested-opt", FALSE },
	{ "algebra/two-nested-opt-alt", "algebra/two-nested-opt", FALSE },
	{ "algebra/opt-filter-3", "algebra/opt-filter-3", FALSE },
	{ "algebra/filter-placement-1", "algebra/data-2", FALSE },
	{ "algebra/filter-placement-2", "algebra/data-2", FALSE },
	{ "algebra/filter-placement-3", "algebra/data-2", FALSE },
	{ "algebra/filter-placement-3a", "algebra/data-2", FALSE },
	{ "algebra/filter-nested-1", "algebra/data-1", FALSE },
	{ "algebra/filter-nested-2", "algebra/data-1", FALSE },
	{ "algebra/filter-scope-1", "algebra/data-2", FALSE },
	{ "algebra/filter-in-1", "algebra/data-2", FALSE },
	{ "algebra/filter-in-2", "algebra/data-2", FALSE },
	{ "algebra/filter-in-3", "algebra/data-2", FALSE },
	{ "algebra/filter-in-4", "algebra/data-2", FALSE },
	{ "algebra/filter-in-5", "algebra/data-2", FALSE },
	{ "algebra/var-scope-join-1", "algebra/var-scope-join-1", FALSE },
	{ "algebra/modifier-limit-offset-1", "algebra/data-1", FALSE },
	{ "algebra/modifier-limit-offset-2", "algebra/data-1", FALSE },
	{ "algebra/modifier-limit-1", "algebra/data-1", FALSE },
	{ "algebra/modifier-offset-1", "algebra/data-1", FALSE },
	{ "anon/query", "anon/data", FALSE },
	{ "anon/query-2", "anon/data", FALSE },
	{ "anon/query-3", "anon/data", FALSE },
	{ "anon/query-4", "anon/data", FALSE },
	{ "anon/query-5", "anon/data", FALSE },
	{ "ask/ask-1", "ask/data", FALSE },
	{ "basic/base-1", "basic/data-1", FALSE },
	{ "basic/base-prefix-3", "basic/data-1", FALSE },
	{ "basic/compare-cast", "basic/data-1", FALSE },
	{ "basic/predicate-variable", "basic/data-1", FALSE },
	{ "basic/predicate-variable-2", "basic/data-1", FALSE },
	{ "basic/predicate-variable-3", "basic/data-1", FALSE },
	{ "basic/predicate-variable-4", "basic/data-1", FALSE },
	{ "basic/urn-in-as", "basic/data-1", FALSE },
	{ "basic/codepoint-escaping", "basic/data-1", FALSE },
	{ "basic/long-strings", "basic/data-1", FALSE },
	{ "bnode/query-1", "bnode/data", FALSE },
	{ "bnode/query-2", "bnode/data", FALSE },
	{ "bnode/query-3", "bnode/data", FALSE },
	{ "bnode/query-4", "bnode/data", FALSE },
	{ "bnode/query-5", "bnode/data", FALSE },
	{ "bnode-coreference/query", "bnode-coreference/data", FALSE },
	{ "bound/bound1", "bound/data", FALSE },
	{ "unbound/unbound-1", "unbound/data", FALSE },
	{ "unbound/unbound-2", "unbound/data", FALSE },
	{ "unbound/unbound-3", "unbound/data", FALSE },
	{ "unbound/unbound-4", "unbound/data", FALSE },
	{ "unbound/unbound-5", "unbound/data", FALSE },
	{ "construct/construct-where", "construct/data", FALSE },
	{ "construct/construct-pattern", "construct/data", FALSE },
	{ "construct/construct-with-modifiers", "construct/data", FALSE },
	{ "datetime/direct-1", "datetime/data-1", FALSE },
	{ "datetime/delete-1", "datetime/data-3", FALSE },
	{ "datetime/insert-1", "datetime/data-4", FALSE },
	{ "datetime/functions-localtime-1", "datetime/data-1", FALSE },
	{ "datetime/functions-timezone-1", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-2", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-3", "datetime/data-2", FALSE },
	{ "datetime/functions-tz-1", "datetime/data-2", FALSE },
	{ "datetime/filter-1", "datetime/data-1", FALSE },
	{ "datetime/filter-2", "datetime/data-1", FALSE },
	{ "datetime/filter-3", "datetime/data-1", FALSE },
	{ "datetime/filter-4", "datetime/data-1", FALSE },
	{ "datetime/filter-5", "datetime/data-1", FALSE },
	{ "describe/describe-single", "describe/data", FALSE },
	{ "describe/describe-non-existent", "describe/data", FALSE },
	{ "describe/describe-pattern", "describe/data", FALSE },
	{ "describe/describe-limit", "describe/data", FALSE },
	{ "describe/describe-multiple", "describe/data", FALSE },
	{ "expr-ops/query-ge-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-le-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-minus-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-mul-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-plus-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-unminus-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-unplus-1", "expr-ops/data", FALSE },
	{ "expr-ops/query-res-1", "expr-ops/data", FALSE },
	{ "functions/functions-cast-1", "functions/data-1", FALSE },
	{ "functions/functions-cast-2", "functions/data-1", FALSE },
	{ "functions/functions-property-1", "functions/data-1", FALSE },
	{ "functions/functions-property-2", "functions/data-5", FALSE },
	{ "functions/functions-tracker-1", "functions/data-1", FALSE },
	{ "functions/functions-tracker-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-3", "functions/data-2", FALSE },
	{ "functions/functions-tracker-4", "functions/data-2", FALSE },
	{ "functions/functions-tracker-5", "functions/data-2", FALSE },
	{ "functions/functions-tracker-6", "functions/data-2", FALSE },
	{ "functions/functions-tracker-7", "functions/data-2", FALSE },
	{ "functions/functions-tracker-8", "functions/data-2", FALSE },
	{ "functions/functions-tracker-9", "functions/data-2", FALSE },
	{ "functions/functions-tracker-loc-1", "functions/data-3", FALSE },
	{ "functions/functions-xpath-1", "functions/data-1", FALSE },
	{ "functions/functions-xpath-2", "functions/data-1", FALSE },
	{ "functions/functions-xpath-3", "functions/data-1", FALSE },
	{ "functions/functions-xpath-4", "functions/data-1", FALSE },
	{ "functions/functions-xpath-5", "functions/data-1", FALSE },
	{ "functions/functions-xpath-6", "functions/data-1", FALSE },
	{ "functions/functions-xpath-7", "functions/data-1", FALSE },
	{ "functions/functions-xpath-8", "functions/data-1", FALSE },
	{ "functions/functions-xpath-9", "functions/data-1", FALSE },
	{ "functions/functions-xpath-10", "functions/data-4", FALSE },
	{ "functions/functions-xpath-11", "functions/data-4", FALSE },
	{ "functions/functions-xpath-12", "functions/data-4", FALSE },
	{ "functions/functions-xpath-13", "functions/data-4", FALSE },
	{ "functions/functions-xpath-14", "functions/data-4", FALSE },
	{ "functions/functions-xpath-15", "functions/data-1", FALSE },
	{ "functions/functions-coalesce-1", "functions/data-1", FALSE },
	{ "functions/functions-datatypes-1", "functions/data-1", FALSE },
	{ "functions/functions-datatypes-2", "functions/data-2", FALSE },
	{ "functions/functions-datatypes-3", "functions/data-3", FALSE },
	{ "functions/functions-datatypes-4", "functions/data-4", FALSE },
	{ "functions/functions-datatypes-5", "functions/data-1", FALSE },
	{ "functions/functions-builtin-bnode-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-bnode-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-uuid-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-hash-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-ucase-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-lcase-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strlen-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strbefore-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strafter-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-substr-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-2", "functions/data-1", TRUE },
	{ "functions/functions-builtin-replace-3", "functions/data-1", TRUE },
	{ "functions/functions-builtin-replace-4", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-5", "functions/data-1", TRUE },
	{ "functions/functions-builtin-contains-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-abs-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-ceil-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-floor-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-round-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-uri-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-year-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-month-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-day-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-hours-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-minutes-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-seconds-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-now-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-encode-for-uri-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strdt-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-sameterm-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-if-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-rand-1", "functions/data-1", FALSE },
	/* Graph semantics and operations */
	{ "graph/graph-1", "graph/data-1", FALSE },
	{ "graph/graph-2", "graph/data-2", FALSE },
	{ "graph/graph-3", "graph/data-3", FALSE },
	{ "graph/graph-4", "graph/data-3", FALSE },
	{ "graph/graph-5", "graph/data-4", FALSE },
	{ "graph/graph-6", "graph/data-5", FALSE },
	{ "graph/graph-7", "graph/data-5", FALSE },
	{ "graph/non-existent-1",  "graph/data-1", FALSE },
	{ "graph/non-existent-2",  "graph/data-1", FALSE },
	{ "graph/non-existent-3",  "graph/data-1", FALSE },
	{ "graph/graph-unbound-1", "graph/data-1", FALSE },
	{ "graph/graph-unbound-2", "graph/data-1", FALSE },
	{ "graph/graph-unbound-3", "graph/data-1", FALSE },
	{ "graph/drop", "graph/data-drop", FALSE },
	{ "graph/drop-non-existent", "graph/data-drop-non-existent", FALSE, TRUE },
	{ "graph/drop-default", "graph/data-drop-default", FALSE },
	{ "graph/drop-named", "graph/data-drop-named", FALSE },
	{ "graph/drop-all", "graph/data-drop-all", FALSE },
	{ "graph/drop-silent", "graph/data-drop-silent", FALSE },
	{ "graph/clear", "graph/data-clear", FALSE },
	{ "graph/clear-non-existent", "graph/data-clear-non-existent", FALSE, TRUE },
	{ "graph/clear-default", "graph/data-clear-default", FALSE },
	{ "graph/clear-named", "graph/data-clear-named", FALSE },
	{ "graph/clear-all", "graph/data-clear-all", FALSE },
	{ "graph/copy", "graph/data-copy", FALSE },
	{ "graph/copy-to-existent", "graph/data-copy-to-existent", FALSE },
	{ "graph/copy-to-non-existent", "graph/data-copy-to-non-existent", FALSE },
	{ "graph/copy-from-non-existent", "graph/data-copy-from-non-existent", FALSE },
	{ "graph/copy-into-self", "graph/data-copy-into-self", FALSE },
	{ "graph/copy-from-default", "graph/data-copy-from-default", FALSE },
	{ "graph/copy-to-default", "graph/data-copy-to-default", FALSE },
	{ "graph/move", "graph/data-move", FALSE },
	{ "graph/move-to-existent", "graph/data-move-to-existent", FALSE },
	{ "graph/move-from-non-existent", "graph/data-move-from-non-existent", FALSE },
	{ "graph/move-into-self", "graph/data-move-into-self", FALSE },
	{ "graph/move-from-default", "graph/data-move-from-default", FALSE },
	{ "graph/move-to-default", "graph/data-move-to-default", FALSE },
	{ "graph/add", "graph/data-add", FALSE },
	{ "graph/add-to-existent", "graph/data-add-to-existent", FALSE },
	{ "graph/add-to-non-existent", "graph/data-add-to-non-existent", FALSE },
	{ "graph/add-from-non-existent", "graph/data-add-from-non-existent", FALSE },
	{ "graph/add-into-self", "graph/data-add-into-self", FALSE },
	{ "graph/add-from-default", "graph/data-add-from-default", FALSE },
	{ "graph/add-to-default", "graph/data-add-to-default", FALSE },
	{ "langstring/match-with-non-langstring", "langstring/data", FALSE },
	{ "langstring/match-with-langstring", "langstring/data", FALSE },
	{ "langstring/match-non-langstring", "langstring/data", FALSE },
	{ "langstring/langmatches", "langstring/data", FALSE },
	{ "langstring/strlang", "langstring/data", FALSE },
	{ "lists/list-in-object", "lists/data-list-in-object", FALSE },
	{ "lists/list-in-subject", "lists/data-list-in-subject", FALSE },
	{ "lists/list-in-select", "lists/data-list-in-select", FALSE },
	{ "lists/list-nested", "lists/data-list-nested", FALSE },
	{ "optional/q-opt-complex-1", "optional/complex-data-1", FALSE },
	{ "optional/simple-optional-triple", "optional/simple-optional-triple", FALSE },
	{ "regex/regex-query-001", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-002", "regex/regex-data-01", FALSE },
	{ "sort/query-sort-1", "sort/data-sort-1", FALSE },
	{ "sort/query-sort-2", "sort/data-sort-1", FALSE },
	{ "sort/query-sort-3", "sort/data-sort-3", FALSE },
	{ "sort/query-sort-4", "sort/data-sort-4", FALSE },
	{ "sort/query-sort-5", "sort/data-sort-4", FALSE },
	{ "sort/query-sort-6", "sort/data-sort-4", FALSE },
	{ "sort/query-sort-7", "sort/data-sort-1", FALSE },
	{ "sort/query-sort-8", "sort/data-sort-5", FALSE },
	{ "sort/query-sort-9", "sort/data-sort-5", FALSE },
	{ "sort/query-title-sort-1", "sort/data-title-sort-1", FALSE },
	{ "subqueries/subqueries-1", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-union-1", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-union-2", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-union-3", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-union-4", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-minus-1", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-minus-2", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-minus-3", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-minus-4", "subqueries/data-1", FALSE },
	{ "subqueries/subqueries-minus-5", "subqueries/data-1", FALSE },
	/* Bracket error after WHERE */
	{ "error/query-error-1", "error/query-error-1", TRUE, FALSE },
	/* Unknown property */
	{ "error/query-error-2", "error/query-error-2", TRUE, FALSE },
	{ "error/update-error-query-1", "error/update-error-1", FALSE, TRUE },
	/* Remapping variables in BIND */
	{ "error/bind-reused-same-pattern", "error/query-error-1", TRUE, FALSE },

	{ "turtle/turtle-query-001", "turtle/turtle-data-001", FALSE },
#if 0
	{ "turtle/turtle-query-002", "turtle/turtle-data-002", FALSE },
#endif
	{ "turtle-comments/turtle-query-001", "turtle-comments/turtle-data-001", FALSE },
	/* Mixed cardinality tests */
	{ "mixed-cardinality/insert-mixed-cardinality-query-1", "mixed-cardinality/insert-mixed-cardinality-1", FALSE, FALSE },
	{ "mixed-cardinality/update-mixed-cardinality-query-1", "mixed-cardinality/update-mixed-cardinality-1", FALSE, FALSE },
	/* Bind tests */
	{ "bind/bind1", "bind/data", FALSE },
	{ "bind/bind2", "bind/data", FALSE },
	{ "bind/bind3", "bind/data", FALSE },
	{ "bind/bind4", "bind/data", FALSE },
	{ "bind/bind5", "bind/data", FALSE },
	{ "bind/bind6", "bind/data", FALSE },
	{ "bind/bind7", "bind/data", FALSE },
	{ "bind/bind-reused-different-patterns", "bind/data", FALSE },
	/* Property paths */
	{ "property-paths/inverse-path-1", "property-paths/data", FALSE },
	{ "property-paths/inverse-path-2", "property-paths/data", FALSE },
	{ "property-paths/sequence-path-1", "property-paths/data", FALSE },
	{ "property-paths/sequence-path-2", "property-paths/data", FALSE },
	{ "property-paths/sequence-path-3", "property-paths/data", FALSE },
	{ "property-paths/optional-path-1", "property-paths/data", FALSE },
	{ "property-paths/optional-path-2", "property-paths/data", FALSE },
	{ "property-paths/recursive-path-1", "property-paths/data", FALSE },
	{ "property-paths/recursive-path-2", "property-paths/data", FALSE },
	{ "property-paths/recursive-path-3", "property-paths/data", FALSE },
	{ "property-paths/alternative-path-1", "property-paths/data", FALSE },
	{ "property-paths/alternative-path-2", "property-paths/data", FALSE },
	{ "property-paths/alternative-path-3", "property-paths/data", FALSE },
	{ "property-paths/negated-path-1", "property-paths/data", FALSE },
	{ "property-paths/negated-path-2", "property-paths/data", FALSE },
	{ "property-paths/negated-path-3", "property-paths/data", FALSE },
	{ "property-paths/negated-path-4", "property-paths/data", FALSE },
	{ "property-paths/negated-path-5", "property-paths/data", FALSE },
	{ "property-paths/negated-path-6", "property-paths/data", FALSE },
	{ "property-paths/negated-path-7", "property-paths/data", FALSE },
	{ "property-paths/negated-path-8", "property-paths/data", FALSE },
	{ "property-paths/negated-path-9", "property-paths/data", FALSE },
	{ "property-paths/negated-path-10", "property-paths/data", FALSE },
	{ "property-paths/negated-path-11", "property-paths/data", FALSE },
	{ "property-paths/mixed-inverse-and-sequence-1", "property-paths/data", FALSE },
	{ "property-paths/mixed-inverse-and-sequence-2", "property-paths/data", FALSE },
	{ "property-paths/mixed-inverse-and-sequence-3", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-sequence-1", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-sequence-2", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-recursive-and-sequence-3", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-recursive-and-sequence-4", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-recursive-and-sequence-5", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-recursive-and-alternative-1", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-alternative-2", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-inverse-1", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-inverse-2", "property-paths/data", FALSE },
	{ "property-paths/mixed-recursive-and-inverse-3", "property-paths/data", FALSE },
	{ "property-paths/mixed-optional-and-sequence-1", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-optional-and-sequence-2", "property-paths/data-2", FALSE },
	{ "property-paths/mixed-graphs", "property-paths/data-3", FALSE },
	/* Update tests */
	{ "update/insert-data-query-1", "update/insert-data-1", FALSE, FALSE },
	{ "update/insert-data-query-2", "update/insert-data-2", FALSE, TRUE },
	{ "update/delete-data-query-1", "update/delete-data-1", FALSE, FALSE },
	{ "update/delete-data-query-2", "update/delete-data-2", FALSE, TRUE },
	{ "update/delete-where-query-1", "update/delete-where-1", FALSE, FALSE },
	{ "update/delete-where-query-2", "update/delete-where-2", FALSE, FALSE },
	{ "update/delete-where-query-3", "update/delete-where-3", FALSE, FALSE },
	{ "update/invalid-insert-where-query-1", "update/invalid-insert-where-1", FALSE, TRUE },
	{ "update/delete-insert-where-query-1", "update/delete-insert-where-1", FALSE, FALSE },
	{ "update/delete-insert-where-query-2", "update/delete-insert-where-2", FALSE, FALSE },
	{ "update/delete-insert-where-query-3", "update/delete-insert-where-3", FALSE, FALSE },
	{ "update/delete-insert-where-query-4", "update/delete-insert-where-4", FALSE, FALSE },
	{ "update/delete-insert-where-query-5", "update/delete-insert-where-5", FALSE, FALSE },
	{ "update/delete-insert-where-query-6", "update/delete-insert-where-6", FALSE, FALSE },
	{ "update/select-date-with-offset-1", "update/insert-date-with-offset-1", FALSE, FALSE },
	/* Constraint declarations */
	{ "constraint/empty-graph-1", "constraint/data", FALSE, FALSE },
	{ "constraint/empty-graph-2", "constraint/data", FALSE, FALSE },
	{ "constraint/empty-graph-3", "constraint/data", FALSE, FALSE },
	{ "constraint/empty-graph-4", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-1", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-2", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-3", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-4", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-5", "constraint/data", FALSE, FALSE },
	{ "constraint/nested-6", "constraint/data", FALSE, FALSE },
	{ "constraint/coexisting-1", "constraint/data", FALSE, FALSE },
	{ "constraint/coexisting-2", "constraint/data", FALSE, FALSE },
	{ "constraint/coexisting-3", "constraint/data", FALSE, FALSE },
	{ "constraint/coexisting-4", "constraint/data", FALSE, FALSE },
	/* Inline data */
	{ "inline/inline-1", "inline/data", FALSE, FALSE },
	{ "inline/inline-2", "inline/data", FALSE, FALSE },
	{ "inline/inline-3", "inline/data", FALSE, FALSE },
	{ "inline/inline-4", "inline/data", FALSE, FALSE },
	{ "inline/inline-5", "inline/data", FALSE, FALSE },
	{ "inline/inline-6", "inline/data", FALSE, FALSE },
	{ "inline/inline-7", "inline/data", FALSE, FALSE },
	{ "inline/inline-8", "inline/data", FALSE, FALSE },
	{ "inline/inline-9", "inline/data", FALSE, FALSE },
	{ "inline/values-1", "inline/data", FALSE, FALSE },
	{ "inline/values-2", "inline/data", FALSE, FALSE },
	{ "inline/values-3", "inline/data", FALSE, FALSE },
	{ "inline/values-4", "inline/data", FALSE, FALSE },
	{ NULL }
};

static void
check_result (TrackerSparqlCursor *cursor,
              const TestInfo      *test_info,
              const gchar         *results_filename,
              GError              *error)
{
	GString *test_results;
	gchar *results;
	GError *nerror = NULL;

	if (!test_info->expect_query_error) {
		g_assert_no_error (error);
	}

	g_file_get_contents (results_filename, &results, NULL, &nerror);
	g_assert_no_error (nerror);
	g_clear_error (&nerror);

	/* compare results with reference output */

	test_results = g_string_new ("");

	if (cursor) {
		gint col;

		while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
			GString *row_str = g_string_new (NULL);

			for (col = 0; col < tracker_sparql_cursor_get_n_columns (cursor); col++) {
				const gchar *str;

				if (col > 0) {
					g_string_append (row_str, "\t");
				}

				str = tracker_sparql_cursor_get_string (cursor, col, NULL);

				/* Hack to avoid misc properties that might tamper with
				 * test reproduceability in DESCRIBE and other unrestricted
				 * queries.
				 */
				if (g_strcmp0 (str, TRACKER_PREFIX_NRL "modified") == 0 ||
				    g_strcmp0 (str, TRACKER_PREFIX_NRL "added") == 0) {
					g_string_free (row_str, TRUE);
					row_str = NULL;
					break;
				}

				if (str != NULL) {
					/* bound variable */
					g_string_append_printf (row_str, "\"%s\"", str);
				}
			}

			if (row_str) {
				g_string_append (test_results, row_str->str);
				g_string_free (row_str, TRUE);
				g_string_append (test_results, "\n");
			}
		}
	}

	if (test_info->expect_query_error) {
		g_assert_true (error != NULL);
		g_string_free (test_results, TRUE);
		g_free (results);
		return;
	}

	g_assert_no_error (error);

	if (strcmp (results, test_results->str) != 0) {
		/* print result difference */
		gchar *quoted_results;
		gchar *command_line;
		gchar *quoted_command_line;
		gchar *shell;
		gchar *diff;

		quoted_results = g_shell_quote (test_results->str);
		command_line = g_strdup_printf ("echo -n %s | diff -uZ %s -", quoted_results, results_filename);
		quoted_command_line = g_shell_quote (command_line);
		shell = g_strdup_printf ("sh -c %s", quoted_command_line);
		g_spawn_command_line_sync (shell, &diff, NULL, NULL, &error);
		g_assert_no_error (error);

		if (diff && *diff)
			g_error ("%s", diff);

		g_free (quoted_results);
		g_free (command_line);
		g_free (quoted_command_line);
		g_free (shell);
		g_free (diff);
	}

	g_string_free (test_results, TRUE);
	g_free (results);
}

static void
test_sparql_query (TestInfo      *test_info,
                   gconstpointer  context)
{
	TrackerSparqlCursor *cursor;
	GError *error = NULL;
	gchar *data_filename;
	gchar *query, *query_filename;
	gchar *results_filename;
	gchar *prefix, *data_prefix, *test_prefix;
	GFile *file, *test_schemas;
	TrackerSparqlConnection *conn;

	/* initialization */
	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "core", NULL);
	data_prefix = g_build_filename (prefix, test_info->data, NULL);
	test_prefix = g_build_filename (prefix, test_info->test_name, NULL);
	g_free (prefix);

	file = g_file_new_for_path (data_prefix);
	test_schemas = g_file_get_parent (file);
	g_object_unref (file);

	conn = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
	                                      NULL, test_schemas, NULL, &error);
	g_assert_no_error (error);

	/* data_path = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "core", NULL); */

	/* load data set */
	data_filename = g_strconcat (data_prefix, ".ttl", NULL);
	if (g_file_test (data_filename, G_FILE_TEST_IS_REGULAR)) {
		GFile *file = g_file_new_for_path (data_filename);
		gchar *uri = g_file_get_uri (file);
		gchar *query = g_strdup_printf ("LOAD <%s>", uri);

		tracker_sparql_connection_update (conn, query, NULL, &error);
		g_assert_no_error (error);
		g_object_unref (file);
		g_free (uri);
		g_free (query);
	} else {
		/* no .ttl available, assume .rq with SPARQL Update */
		gchar *data;

		g_free (data_filename);

		data_filename = g_strconcat (data_prefix, ".rq", NULL);
		g_file_get_contents (data_filename, &data, NULL, &error);
		g_assert_no_error (error);

		tracker_sparql_connection_update (conn, data, NULL, &error);
		if (test_info->expect_update_error) {
			g_assert_true (error != NULL);
			g_clear_error (&error);
		} else {
			g_assert_no_error (error);
		}

		g_free (data);
	}

	query_filename = g_strconcat (test_prefix, ".rq", NULL);
	g_file_get_contents (query_filename, &query, NULL, &error);
	g_assert_no_error (error);

	results_filename = g_strconcat (test_prefix, ".out", NULL);

	/* perform actual query */

	cursor = tracker_sparql_connection_query (conn, query, NULL, &error);

	check_result (cursor, test_info, results_filename, error);

	g_free (query_filename);
	g_free (query);

	query_filename = g_strconcat (test_prefix, ".extra.rq", NULL);
	if (g_file_get_contents (query_filename, &query, NULL, NULL)) {
		g_object_unref (cursor);
		cursor = tracker_sparql_connection_query (conn, query, NULL, &error);
		g_assert_no_error (error);
		g_free (results_filename);
		results_filename = g_strconcat (test_prefix, ".extra.out", NULL);
		check_result (cursor, test_info, results_filename, error);
	}

	g_free (data_prefix);
	g_free (test_prefix);

	if (cursor) {
		g_object_unref (cursor);
	}

	/* cleanup */

	g_free (data_filename);
	g_free (query_filename);
	g_free (query);
	g_free (results_filename);
	g_object_unref (test_schemas);

	g_object_unref (conn);
}

static void
setup (TestInfo      *info,
       gconstpointer  context)
{
	const TestInfo *test = context;

	*info = *test;
}

static void
teardown (TestInfo      *info,
          gconstpointer  context)
{
}

int
main (int argc, char **argv)
{
	gchar *current_dir;
	gint result;
	gint i;

	setlocale (LC_ALL, "en_US.utf8");

	current_dir = g_get_current_dir ();
	tests_data_dir = g_build_filename (current_dir, "sparql-test-data-XXXXXX", NULL);
	g_free (current_dir);

	g_mkdtemp (tests_data_dir);

	g_test_init (&argc, &argv, NULL);

	/* add test cases */
	for (i = 0; tests[i].test_name; i++) {
		gchar *testpath;

#ifndef HAVE_LIBICU
		/* Skip tests which fail collation tests and are known
		 * to do so. For more details see:
		 *
		 * https://bugzilla.gnome.org/show_bug.cgi?id=636074
		 */
		if (strcmp (tests[i].test_name, "functions/functions-xpath-2") == 0) {
			continue;
		}
#endif

		testpath = g_strconcat ("/core/sparql/", tests[i].test_name, NULL);
		g_test_add (testpath, TestInfo, &tests[i], setup, test_sparql_query, teardown);
		g_free (testpath);
	}

	/* run tests */
	result = g_test_run ();

	g_assert_cmpint (g_remove (tests_data_dir), ==, 0);
	g_free (tests_data_dir);

	return result;
}
