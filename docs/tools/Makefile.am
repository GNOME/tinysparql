include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-graph ttl2sgml

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DTOP_SRCDIR=\"$(top_srcdir)\"                 \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_DATA_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(LIBTRACKER_DATA_LIBS)

ontology_graph_SOURCES =                               \
	ontology-graph.c

ttl2sgml_SOURCES =                                     \
	ttl2sgml.c                                     \
	ttl_loader.h                                   \
	ttl_loader.c                                   \
	ttl_model.h                                    \
	ttl_model.c                                    \
	qname.h                                        \
	qname.c                                        \
	ttl_sgml.h                                     \
	ttl_sgml.c

EXTRA_DIST = gen-doc.sh
