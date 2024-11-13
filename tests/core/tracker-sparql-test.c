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

#include <tinysparql.h>

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
	{ "construct/construct-where-2", "construct/data", FALSE },
	{ "construct/construct-where-3", "construct/data", FALSE },
	{ "construct/construct-pattern", "construct/data", FALSE },
	{ "construct/construct-pattern-2", "construct/data", FALSE },
	{ "construct/construct-pattern-3", "construct/data", FALSE },
	{ "construct/construct-with-modifiers", "construct/data", FALSE },
	{ "date/direct-1", "date/data-1", FALSE },
	{ "date/insert-1", "date/data-1", FALSE },
	{ "datetime/direct-1", "datetime/data-1", FALSE },
	{ "datetime/delete-1", "datetime/data-3", FALSE },
	{ "datetime/insert-1", "datetime/data-4", FALSE },
	{ "datetime/functions-localtime-1", "datetime/data-1", FALSE },
	{ "datetime/functions-timezone-1", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-2", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-3", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-4", "datetime/data-2", FALSE },
	{ "datetime/functions-timezone-5", "datetime/data-5", FALSE },
	{ "datetime/functions-timezone-6", "datetime/data-2", FALSE },
	{ "datetime/functions-tz-1", "datetime/data-2", FALSE },
	{ "datetime/functions-tz-2", "datetime/data-2", FALSE },
	{ "datetime/functions-tz-3", "datetime/data-2", FALSE },
	{ "datetime/functions-tz-4", "datetime/data-2", FALSE },
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
	{ "describe/describe-from", "describe/data-graph", FALSE },
	{ "describe/describe-from-named", "describe/data-graph", FALSE },
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
	{ "functions/functions-tracker-case-fold-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-case-fold-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-4", "functions/data-2", FALSE },
	{ "functions/functions-tracker-unaccent-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-unaccent-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-parent-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-parent-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-parent-3", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-parent-4", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-descendant-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-descendant-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-uri-is-descendant-3", "functions/data-2", FALSE },
	{ "functions/functions-tracker-normalize-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-normalize-2", "functions/data-2", FALSE },
	{ "functions/functions-tracker-normalize-3", "functions/data-2", TRUE },
	{ "functions/functions-tracker-strip-punctuation-1", "functions/data-2", FALSE },
	{ "functions/functions-tracker-loc-1", "functions/data-3", FALSE },
	{ "functions/functions-xpath-1", "functions/data-1", FALSE },
	{ "functions/functions-xpath-2", "functions/data-1", FALSE },
	{ "functions/functions-xpath-fn-string-join-1", "functions/data-1", FALSE },
	{ "functions/functions-xpath-fn-string-join-2", "functions/data-1", TRUE },
	{ "functions/functions-xpath-fn-string-join-3", "functions/data-1", TRUE },
	{ "functions/functions-xpath-fn-string-join-4", "functions/data-1", TRUE },
	{ "functions/functions-xpath-fn-string-join-5", "functions/data-1", FALSE },
	{ "functions/functions-xpath-fn-string-join-6", "functions/data-1", FALSE },
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
	{ "functions/functions-datatypes-6", "functions/data-1", FALSE },
	{ "functions/functions-datatypes-7", "functions/data-1", FALSE },
	{ "functions/functions-builtin-bnode-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-bnode-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-uuid-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-hash-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-ucase-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-ucase-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-lcase-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-lcase-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strlen-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strbefore-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strbefore-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strbefore-3", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strafter-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strafter-2", "functions/data-1", FALSE },
	{ "functions/functions-builtin-strafter-3", "functions/data-1", FALSE },
	{ "functions/functions-builtin-struuid-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-substr-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-2", "functions/data-1", TRUE },
	{ "functions/functions-builtin-replace-3", "functions/data-1", TRUE },
	{ "functions/functions-builtin-replace-4", "functions/data-1", FALSE },
	{ "functions/functions-builtin-replace-5", "functions/data-1", TRUE },
	{ "functions/functions-builtin-replace-6", "functions/data-1", TRUE },
	{ "functions/functions-builtin-contains-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-abs-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-ceil-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-floor-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-round-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-uri-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-year-1", "functions/data-1", FALSE },
	{ "functions/functions-builtin-year-2", "functions/data-1", FALSE },
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
	{ "graph/from-1", "graph/data-1", FALSE },
	{ "graph/from-2", "graph/data-1", FALSE },
	{ "graph/from-3", "graph/data-1", FALSE },
	{ "graph/from-4", "graph/data-1", FALSE },
	{ "graph/from-5", "graph/data-1", FALSE },
	{ "graph/from-6", "graph/data-1", FALSE },
	{ "graph/from-7", "graph/data-1", FALSE },
	{ "graph/from-8", "graph/data-1", FALSE },
	{ "graph/from-9", "graph/data-5", FALSE },
	{ "graph/from-10", "graph/data-6", FALSE },
	{ "graph/from-11", "graph/data-6", FALSE },
	{ "graph/from-12", "graph/data-6", FALSE },
	{ "graph/from-13", "graph/data-6", FALSE },
	{ "graph/from-14", "graph/data-6", FALSE },
	{ "graph/from-15", "graph/data-6", FALSE },
	{ "graph/from-16", "graph/data-6", FALSE },
	{ "graph/from-17", "graph/data-6", FALSE },
	{ "graph/from-18", "graph/data-6", FALSE },
	{ "graph/from-19", "graph/data-6", FALSE },
	{ "graph/from-20", "graph/data-6", FALSE },
	{ "graph/from-21", "graph/data-6", FALSE },
	{ "graph/from-22", "graph/data-6", FALSE },
	{ "graph/from-23", "graph/data-6", FALSE },
	{ "graph/from-24", "graph/data-6", FALSE },
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
	{ "langstring/langmatches-1", "langstring/data", FALSE },
	{ "langstring/langmatches-2", "langstring/data", FALSE },
	{ "langstring/strlang", "langstring/data", FALSE },
	{ "lists/list-in-object", "lists/data-list-in-object", FALSE },
	{ "lists/list-in-subject", "lists/data-list-in-subject", FALSE },
	{ "lists/list-in-select", "lists/data-list-in-select", FALSE },
	{ "lists/list-nested", "lists/data-list-nested", FALSE },
	{ "lists/insert-error-1", "lists/data-insert-error-1", FALSE, TRUE },
	{ "optional/q-opt-complex-1", "optional/complex-data-1", FALSE },
	{ "optional/simple-optional-triple", "optional/simple-optional-triple", FALSE },
	{ "regex/regex-query-001", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-002", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-003", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-004", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-005", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-006", "regex/regex-data-01", FALSE },
	{ "regex/regex-query-007", "regex/regex-data-01", TRUE },
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
	{ "error/glob-empty", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-1", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-2", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-3", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-4", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-5", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-6", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-7", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-8", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-snippet-9", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-rank-1", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-rank-2", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-rank-3", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-offsets-1", "error/query-error-1", TRUE, FALSE },
	{ "error/fts-offsets-2", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-additive-1", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-additive-2", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-additive-3", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-additive-4", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-multiplicative-1", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-multiplicative-2", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-multiplicative-3", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-multiplicative-4", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-logical-1", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-logical-2", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-logical-3", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-logical-4", "error/query-error-1", TRUE, FALSE },
	{ "error/expression-unary-1", "error/query-error-1", TRUE, FALSE },
	{ "error/values-1", "error/query-error-1", TRUE, FALSE },
	{ "error/values-2", "error/query-error-1", TRUE, FALSE },
	{ "error/function-1", "error/query-error-1", TRUE, FALSE },
	{ "error/function-2", "error/query-error-1", TRUE, FALSE },
	{ "error/function-3", "error/query-error-1", TRUE, FALSE },
	{ "error/function-4", "error/query-error-1", TRUE, FALSE },
	{ "error/function-5", "error/query-error-1", TRUE, FALSE },
	{ "error/function-6", "error/query-error-1", TRUE, FALSE },
	{ "error/triples-1", "error/query-error-1", TRUE, FALSE },
	{ "error/triples-2", "error/query-error-1", TRUE, FALSE },
	{ "error/triples-3", "error/query-error-1", TRUE, FALSE },
	{ "error/triples-4", "error/query-error-1", TRUE, FALSE },
	{ "error/parameter-1", "error/query-error-1", TRUE, FALSE },
	{ "error/cast-1", "error/query-error-1", TRUE, FALSE },
	{ "error/cast-2", "error/query-error-1", TRUE, FALSE },
	{ "error/cast-3", "error/query-error-1", TRUE, FALSE },
	{ "error/cast-4", "error/query-error-1", TRUE, FALSE },
	{ "error/describe-1", "error/query-error-1", TRUE, FALSE },
	{ "error/describe-2", "error/query-error-1", TRUE, FALSE },
	{ "error/garbage-1", "error/query-error-1", TRUE, FALSE },
	/* Turtle data */
	{ "turtle/turtle-query-001", "turtle/turtle-data-001", FALSE },
	{ "turtle/turtle-query-002", "turtle/turtle-data-002", FALSE },
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
	{ "update/delete-where-query-4", "update/delete-where-4", FALSE, FALSE },
	{ "update/invalid-insert-where-query-1", "update/invalid-insert-where-1", FALSE, TRUE },
	{ "update/delete-insert-where-query-1", "update/delete-insert-where-1", FALSE, FALSE },
	{ "update/delete-insert-where-query-2", "update/delete-insert-where-2", FALSE, FALSE },
	{ "update/delete-insert-where-query-3", "update/delete-insert-where-3", FALSE, FALSE },
	{ "update/delete-insert-where-query-4", "update/delete-insert-where-4", FALSE, FALSE },
	{ "update/delete-insert-where-query-5", "update/delete-insert-where-5", FALSE, FALSE },
	{ "update/delete-insert-where-query-6", "update/delete-insert-where-6", FALSE, FALSE },
	{ "update/select-date-with-offset-1", "update/insert-date-with-offset-1", FALSE, FALSE },
	{ "update/using-query-1", "update/using-1", FALSE, FALSE },
	{ "update/using-query-2", "update/using-2", FALSE, FALSE },
	{ "update/using-named-query-1", "update/using-named-1", FALSE, FALSE },
	{ "update/using-named-query-2", "update/using-named-2", FALSE, FALSE },
	{ "update/with-query-1", "update/with-1", FALSE, FALSE },
	{ "update/with-query-2", "update/with-2", FALSE, FALSE },
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
	{ "constraint/property-path-sequence-1", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-sequence-2", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-sequence-3", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-optional-1", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-optional-2", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-optional-3", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-inverse-1", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/property-path-inverse-2", "constraint/property-paths-1", FALSE, FALSE },
	{ "constraint/describe-1", "constraint/data", FALSE, FALSE },
	{ "constraint/describe-2", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-1", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-2", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-3", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-4", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-5", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-allowed-1", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-allowed-2", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-allowed-3", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-allowed-4", "constraint/data", FALSE, FALSE },
	{ "constraint/select-from-allowed-5", "constraint/data", FALSE, FALSE },
	{ "constraint/insert-1", "constraint/insert-data-1", FALSE, TRUE },
	{ "constraint/insert-2", "constraint/insert-data-2", FALSE, TRUE },
	{ "constraint/insert-3", "constraint/insert-data-3", FALSE, TRUE },
	{ "constraint/insert-4", "constraint/insert-data-4", FALSE, TRUE },
	{ "constraint/insert-allowed-1", "constraint/insert-allowed-data-1", FALSE, FALSE },
	{ "constraint/insert-allowed-2", "constraint/insert-allowed-data-2", FALSE, FALSE },
	{ "constraint/delete-1", "constraint/delete-data-1", FALSE, TRUE },
	{ "constraint/create-graph-1", "constraint/create-graph-update-1", FALSE, TRUE },
	{ "constraint/create-graph-allowed-1", "constraint/create-graph-allowed-update-1", FALSE, FALSE },
	{ "constraint/drop-graph-1", "constraint/drop-graph-update-1", FALSE, TRUE },
	{ "constraint/drop-graph-allowed-1", "constraint/drop-graph-allowed-update-1", FALSE, FALSE },
	{ "constraint/copy-graph-1", "constraint/copy-graph-update-1", FALSE, TRUE },
	{ "constraint/copy-graph-allowed-1", "constraint/copy-graph-allowed-update-1", FALSE, FALSE },
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
	gchar *results = NULL;
	GError *nerror = NULL;
	gboolean retval;

	if (!test_info->expect_query_error) {
		g_assert_no_error (error);
		retval = g_file_get_contents (results_filename, &results, NULL, &nerror);
		g_assert_true (retval);
		g_assert_no_error (nerror);
		g_clear_error (&nerror);
	}

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
		g_clear_error (&error);
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
	GFile *file, *test_schemas, *parent;
	TrackerSparqlConnection *conn;
	gboolean retval;

	/* initialization */
	prefix = g_build_path (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "core", NULL);
	data_prefix = g_build_filename (prefix, test_info->data, NULL);
	test_prefix = g_build_filename (prefix, test_info->test_name, NULL);
	g_free (prefix);

	file = g_file_new_for_path (data_prefix);
	parent = g_file_get_parent (file);
	test_schemas = g_file_get_child (parent, "ontology");
	g_object_unref (file);
	g_object_unref (parent);

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
		retval = g_file_get_contents (data_filename, &data, NULL, &error);
		g_assert_true (retval);
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
	retval = g_file_get_contents (query_filename, &query, NULL, &error);
	g_assert_true (retval);
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
		g_free (query);
	}

	g_free (query_filename);
	g_free (data_prefix);
	g_free (test_prefix);

	if (cursor) {
		g_object_unref (cursor);
	}

	/* cleanup */

	g_free (data_filename);
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
	gint result;
	gint i;

	setlocale (LC_ALL, "en_US.utf8");

	g_test_init (&argc, &argv, NULL);

	/* add test cases */
	for (i = 0; tests[i].test_name; i++) {
		gchar *testpath;

		testpath = g_strconcat ("/core/sparql/", tests[i].test_name, NULL);
		g_test_add (testpath, TestInfo, &tests[i], setup, test_sparql_query, teardown);
		g_free (testpath);
	}

	/* run tests */
	result = g_test_run ();

	return result;
}
