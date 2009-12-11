include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-graph

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(DBUS_CFLAGS)

ontology_graph_SOURCES =						\
	ontology-graph.c

ontology_graph_LDADD =							\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)

EXTRA_DIST = generate-ontology.sh

GENERATED_FILES = ontology.png ontology.dot

CLEANFILES = $(GENERATED_FILES)