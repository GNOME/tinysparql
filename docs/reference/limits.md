Title: Implementation limits
Slug: implementation-limits

TinySPARQL is implemented on top of [SQLite](https://sqlite.org), and all of its
benefits and [limits](https://sqlite.org/limits.html) apply. This
document will break down how those limits apply to TinySPARQL. Depending on
your distributor, the limits might be changed via SQLite build-time
options.

## Maximum string length

TinySPARQL can store by default up to 1 GiB in a text field by default. This
limit is controlled by `SQLITE_MAX_LENGTH`

## Maximum number of properties and select columns

The maximum amount of properties in the domain of a single class, and
the maximum number of global fulltext-search properties in the ontology
are limited by `SQLITE_MAX_COLUMN` (defaults to 2000).

Each column in the result set of a SELECT query contains additional
information included as an additional column, so the limit of columns
that can be retrieved in a single SELECT query is effectively half
of `SQLITE_MAX_COLUMN` (1000 with the default settings).

## Underlying parser limits

SQLite defines some limits to its parser. Maximum query length is 1 MiB,
its maximum expression tree depth is 1000, and it allows up to 500
elements in `UNION / INTERSECT / EXCEPT` statements.

There is no straightforward relation between the SPARQL query and the
SQL one, so it is undefined how this limit applies to SPARQL queries.
Query and ontology simplifications may help reduce its likelyhood.

These SQLite limits are controlled by
`SQLITE_MAX_SQL_LENGTH`, `SQLITE_MAX_EXPR_DEPTH` and
`SQLITE_MAX_COMPOUND_SELECT`. The SPARQL parser
is not affected in itself by these limits.

## Maximum number of tables in a `JOIN`

SQLite does not allow Join operations containing more than 64 tables.
This limit may impact complex TinySPARQL queries in various ways, most
likely culprits are operations across the whole union graph, and
access to multi-valued properties.

Such queries can be tweaked (e.g. specifying a graph, or reducing
the amount of accessed properties) or split into multiple ones to
bypass this limit.

## Maximum number or arguments in a function

SQLite defines a limit of 100, controlled by `SQLITE_MAX_FUNCTION_ARG`.
This affects all builtin and extra functions with variable arguments,
e.g. `coalesce`.

## Limits on the number of graphs

TinySPARQL implements graphs as a set of tables with a common name
prefix, mirroring the ontology structure for each graph. SQLite requires
that at a table has at least one page in the database, so the theoretical
maximum amount of graphs for an ontology the size of Nepomuk is
approximately 14 million graphs (4294967294 pages is the maximum for
SQLite, divided by ~300 tables in Nepomuk).

It should be accounted that the practical limit will be smaller than that,
since tables with a moderate amount of data will use more than a single
page each.

## Limits on glob search

SQLite defines a maximum length of 50 KiB for GLOB patterns. This
reflects in `CONTAINS / STRSTARTS / STRENDS`.
SPARQL syntax.

## Limits on the number of parameters in a statement

SQLite defines a maximum of 999 parameters to be passed as arguments
to a statement, controlled by `SQLITE_MAX_VARIABLE_NUMBER`.
[class@SparqlStatement] has the same limit.

## Maximum number of pages in a database

SQLite sets a default restriction of 4294967294 pages to
database file size (about 32 TB with TinySPARQL settings). This limit
applies collectively to all graphs.

## Type formats and precision

Integers are 64 bit wide. Floating point numbers have IEEE764
double precision. Dates/times have microsecond precision, and may
range between `0001-01-01 00:00:00` and `9999-12-31 23:59:59`.
