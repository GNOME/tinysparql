Title: Builtin SPARQL functions
slug: sparql-functions

Besides the functions built in the SPARQL 1.1 syntax, type casts
and functional properties, TinySPARQL supports a number of SPARQL
functions. Some of these functions have correspondences in
[XPath](https://www.w3.org/TR/xpath-31/).

# String functions

## `fn:lower-case`

```SPARQL
fn:lower-case (?string)
```

Converts a string to lowercase, equivalent to `LCASE`.

## `fn:upper-case`

```SPARQL
fn:upper-case (?string)
```

Converts a string to uppercase, equivalent to `UCASE`.

## `fn:contains`

```SPARQL
fn:contains (?haystack, ?needle)
```

Returns a boolean indicating whether `?needle` is
found in `?haystack`. Equivalent to `CONTAINS`.

## `fn:starts-with`

```SPARQL
fn:starts-with (?string, ?prefix)
```

Returns a boolean indicating whether `?string`
starts with `?prefix`. Equivalent to `STRSTARTS`.

## `fn:ends-with`

```SPARQL
fn:ends-with (?string, ?suffix)
```

Returns a boolean indicating whether `?string`
ends with `?suffix`. Equivalent to `STRENDS`.

## `fn:substring`

```SPARQL
fn:substring (?string, ?startLoc)
fn:substring (?string, ?startLoc, ?endLoc)
```

Returns a substring delimited by the integer
`?startLoc` and `?endLoc` arguments. If `?endLoc`
is omitted, the end of the string is used.

## `fn:concat`

```SPARQL
fn:concat (?string1, ?string2, ..., ?stringN)
```

Takes a variable number of arguments and returns a string concatenation
of all its returned values. Equivalent to `CONCAT`.

## `fn:string-join`

```SPARQL
fn:string-join ((?string1, ?string2, ...), ?separator)
```

Takes a variable number of arguments and returns a string concatenation
using `?separator` to join all elements.

## `fn:replace`

```SPARQL
fn:replace (?string, ?regex, ?replacement)
fn:replace (?string, ?regex, ?replacement, ?flags)
```

Performs string replacement on `?string`. All
matches of `?regex` are replaced by `?replacement` in
the returned string. This function is similar to `REPLACE`.

The `?flags` argument is optional, a case search
is performed if not provided.

If `?flags` contains the character `“s”`, a dot metacharacter (".") in
`?regex` matches all characters, including newlines. Without it,
newlines are excluded.

If `?flags` contains the character `“m”`, the “start of line”
(`^`) and “end of line” (`$`) constructs match immediately following or
immediately before any newline in the string. If not set these constructs
will respectively match the start and end of the full string.

If `?flags` contains the character `“i”`, search is caseless.

If `?flags` contains the character `“x”`, `?regex` is
interpreted to be an extended regular expression.

## `tracker:case-fold`

```SPARQL
tracker:case-fold (?string)
```

Converts a string into a form that is independent of case.

## `tracker:title-order`

```SPARQL
tracker:title-order (?string)
```

Manipulates a string to remove leading articles for sorting
purposes, e.g. “Wall, The”. Best used in `ORDER BY` clauses.

## `tracker:ascii-lower-case`

```SPARQL
tracker:ascii-lower-case (?string)
```

Converts an ASCII string to lowercase, equivalent to `LCASE`.

## `tracker:normalize`

```SPARQL
tracker:normalize (?string, ?option)
```

Normalizes `?string`. The `?option` string must be one of
`nfc`, `nfd`, `nfkc` or `nfkd`.

## `tracker:unaccent`

```SPARQL
tracker:unaccent (?string)
```

Removes accents from a string.

## `tracker:coalesce`

```SPARQL
tracker:coalesce (?value1, ?value2, ..., ?valueN)
```

Picks the first non-null value. Equivalent to `COALESCE`.

## `tracker:strip-punctuation`

``` SPARQL
tracker:strip-punctuation (?string)
```

Removes any Unicode character which has the General
Category value of P (Punctuation) from the string.

# DateTime functions

## `fn:year-from-dateTime`

```SPARQL
fn:year-from-dateTime (?date)
```

Returns the year from a `xsd:date` type, a `xsd:dateTime`
type, or a ISO8601 date string. This function is equivalent
to `YEAR`.

## `fn:month-from-dateTime`

```SPARQL
fn:month-from-dateTime (?date)
```

Returns the month from a `xsd:date` type, a `xsd:dateTime`
type, or a ISO8601 date string. This function is equivalent to `MONTH`.

## `fn:day-from-dateTime`

```SPARQL
fn:day-from-dateTime (?date)
```

Returns the day from a `xsd:date` type, a `xsd:dateTime`
type, or a ISO8601 date string. This function is equivalent to `DAY`.

## `fn:hours-from-dateTime`

```SPARQL
fn:hours-from-dateTime (?date)
```

Returns the hours from a `xsd:dateTime` type or a ISO8601
datetime string. This function is equivalent to `HOURS`.

## `fn:minutes-from-dateTime`

```SPARQL
fn:minutes-from-dateTime (?date)
```

Returns the minutes from a `xsd:dateTime` type
or a ISO8601 datetime string. This function is equivalent to
`MINUTES`.

## `fn:seconds-from-dateTime`

```SPARQL
fn:seconds-from-dateTime (?date)
```

Returns the seconds from a `xsd:dateTime` type
or a ISO8601 datetime string. This function is equivalent to
`SECONDS`.

## `fn:timezone-from-dateTime`

```SPARQL
fn:timezone-from-dateTime (?date)
```

Returns the timezone offset in minutes. This function is similar, but
not equivalent to `TIMEZONE` or `TZ`.

## Full-text search functions

## `fts:rank`

```SPARQL
fts:rank (?match)
```

Returns the rank of a full-text search match. Must be
used in conjunction with `fts:match`.

## `fts:offsets`

```SPARQL
fts:offsets (?match)
```

Returns a string describing the offsets of a full-text search
match. Must be used in conjunction with `fts:match`. The returned
string has the format:

```
prefix:property:offset prefix:property:offset prefix:property:offset
```

## `fts:snippet`

```SPARQL
fts:snippet (?match)
fts:snippet (?match, ?delimStart, ?delimEnd)
fts:snippet (?match, ?delimStart, ?delimEnd, ?ellipsis)
fts:snippet (?match, ?delimStart, ?delimEnd, ?ellipsis, ?numTokens)
```

Returns a snippet string in accordance to the full-text search string.
Must be used in conjunction with `fts:match`.

The `?delimStart` and `?delimEnd` parameters allow specifying the
delimiters of the match strings inside the snippet.

The `?ellipsis` parameter allows specifying
the string used to separate distant matches in the snippet string.

The `?numTokens` parameter specifies the number
of tokens the returned string should containt at most.

# URI functions

## `tracker:uri-is-parent`

```SPARQL
tracker:uri-is-parent (?parent, ?uri)
```

Returns a boolean value expressing whether
`?parent` is a parent of `?uri`.

## `tracker:uri-is-descendant`

```SPARQL
tracker:uri-is-descendant (?uri1, ?uri2, ..., ?uriN, ?child)
```

Returns a boolean value expressing whether one of the
given URIs are a parent (direct or indirect) of
`?child`.

## `tracker:string-from-filename`

```SPARQL
tracker:string-from-filename (?filename)
```

Returns a UTF-8 string from a filename.

# Geolocation functions

## `tracker:cartesian-distance`

```SPARQL
tracker:cartesian-distance (?lat1, ?lat2, ?lon1, ?lon2)
```

Calculates the cartesian distance between 2 points expressed
by `?lat1 / ?lon1` and `?lat2 / ?lon2`.

## `tracker:haversine-distance`

```SPARQL
tracker:haversine-distance (?lat1, ?lat2, ?lon1, ?lon2)
```

Calculates the haversine distance between 2 points expressed
by `?lat1 / ?lon1` and `?lat2 / ?lon2`.

# Identification functions

## `tracker:id`

```SPARQL
tracker:id (?urn)
```

Returns the internal ID corresponding to a URN.
Its inverse operation is `tracker:uri`.

## `tracker:uri`

```SPARQL
tracker:uri (?id)
```

Returns the URN that corresponds to an ID.
Its inverse operation is `tracker:id`

# Full-text search

Full-text search (FTS) is a built-in feature of TinySPARQL, it allows
for efficient search of individual terms across large collections
of text.

String properties are not searchable through the full-text index
by default, only properties with the [nrl:fulltextIndexed](nrl-ontology.md#nrl:fulltextIndexed)
property enabled can be searched with FTS.

```turtle
# Enable FTS search on the example:title property
example:title a rdf:Property;
  rdfs:range xsd:string ;
  nrl:fulltextIndexed true .
```

Multiple full-text indexed properties may be defined, either
in the same or different classes.

In order to tap into the full-text search index, the SPARQL
query must use the `fts:match` pseudo-property, e.g.:

```sparql
SELECT ?res { ?res fts:match "term1 term2" }
```

This property will match the resources that match the search
terms, independently of the order and separation between
those terms in the searched text. Any full-text indexed
property is candidate for these matches.

In order to complement this pseudo-property, there are
additional functions to extract more information from the
full-text index:

- [fts:rank](#ftsrank) to get a ranking order for the matched
  element. To be used in `ORDER BY` clauses.
- [fts:offsets](#ftsoffets) to get a list of offsets of the
  lookup terms as found in the searched text.
- [fts:snippet](#ftssnippet) to get a short string representation
  of the lookup terms in the searched text.

These functions work on each of the matches provided by `fts:match`,
the following example puts all FTS features together at work:

```sparql
# Get resource IRI, snippet and match offsets of every
# document matching "GNOME"
SELECT
  ?u
  fts:snippet(?u)
  fts:offsets(?u)
{
  ?u a nfo:Document ;
    fts:match "GNOME" .
}
ORDER BY DESC (fts:rank(?u))
```

Full-text search is case insensitive, and the content of its
index may be subject to [stemming](https://en.wikipedia.org/wiki/Stemming),
[stop word lists](https://en.wikipedia.org/wiki/Stop_word) and other
methods of pre-processing, these settings may changed via the
[flags@SparqlConnectionFlags] set on a local connection.
