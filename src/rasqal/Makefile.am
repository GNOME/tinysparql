# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal RDF query language library
#
# Copyright (C) 2003-2008, David Beckett http://www.dajobe.org/
# Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
# 
# This package is Free Software and part of Redland http://librdf.org/
# 
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
# 
# You may not use this file except in compliance with at least one of
# the above three licenses.
# 
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
# 

noinst_LTLIBRARIES = librasqal.la

noinst_HEADERS = rasqal.h rasqal_internal.h rasqal_config.h win32_rasqal_config.h

MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_parser.output \
sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_parser.output

INCLUDES =			\
	-DRASQAL_INTERNAL=1	\
	-DVERSION=\""0.9.17"\"	\
	$(RAPTOR_CFLAGS)

librasqal_la_SOURCES = \
rasqal_algebra.c \
rasqal_expr.c rasqal_general.c rasqal_query.c rasqal_query_results.c \
rasqal_engine.c rasqal_raptor.c rasqal_literal.c \
rasqal_graph_pattern.c rasqal_map.c rasqal_feature.c \
rasqal_result_formats.c rasqal_xsd_datatypes.c rasqal_decimal.c \
rasqal_datetime.c rasqal_rowsource.c rasqal_sparql_xml.c \
rasqal_variable.c rasqal_rowsource_empty.c rasqal_rowsource_union.c \
rasqal_rowsource_rowsequence.c rasqal_query_transform.c rasqal_row.c \
rasqal_engine_algebra.c rasqal_triples_source.c \
rasqal_rowsource_triples.c rasqal_rowsource_filter.c \
rasqal_rowsource_sort.c rasqal_engine_sort.c \
rasqal_rowsource_project.c rasqal_rowsource_join.c

if RASQAL_QUERY_RDQL
librasqal_la_SOURCES += rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_common.h
endif
if RASQAL_QUERY_SPARQL
librasqal_la_SOURCES += sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_common.h
endif

librasqal_la_LIBADD = $(RAPTOR_LIBS)


EXTRA_DIST= \
rdql_lexer.l rdql_parser.y \
sparql_lexer.l sparql_parser.y \
fix-flex fix-bison \
rasqal.vapi

LEX=@LEX@
YACC=@YACC@

# Actually it needs rdql_parser.h but nevermind
rdql_lexer.c: $(srcdir)/rdql_lexer.l rdql_parser.c $(srcdir)/fix-flex
	$(LEX) -o$@ $(srcdir)/rdql_lexer.l
	$(PERL) $(srcdir)/fix-flex $@ > rdql_lexer.t
	mv rdql_lexer.t $@

rdql_parser.c: $(srcdir)/rdql_parser.y $(srcdir)/fix-bison
	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y
	$(PERL) $(srcdir)/fix-bison rdql_parser.tab.c > $@
	mv rdql_parser.tab.h rdql_parser.h
	rm -f rdql_parser.tab.c

# Actually it needs sparql_parser.h but nevermind
sparql_lexer.c: $(srcdir)/sparql_lexer.l sparql_parser.c $(srcdir)/fix-flex
	$(LEX) -o$@ $(srcdir)/sparql_lexer.l
	$(PERL) $(srcdir)/fix-flex $@ > sparql_lexer.t
	mv sparql_lexer.t $@

sparql_parser.c: $(srcdir)/sparql_parser.y $(srcdir)/fix-bison
	$(YACC) -b sparql_parser -p sparql_parser_ -d -v $(srcdir)/sparql_parser.y
	$(PERL) $(srcdir)/fix-bison sparql_parser.tab.c > $@
	mv sparql_parser.tab.h sparql_parser.h
	rm -f sparql_parser.tab.c

