include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-graph ttl2sgml

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

ttl2sgml_SOURCES =							\
	ttl2sgml.c							\
	ttl_loader.h							\
	ttl_loader.c							\
	ttl_model.h 							\
	ttl_model.c							\
	qname.h								\
	qname.c								\
	ttl_sgml.h							\
	ttl_sgml.c

ttl2sgml_LDADD = 							\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GIO_LIBS)

gen-doc.stamp: ttl2sgml
	./gen-doc.sh
	$(AM_V_GEN) touch $@

if HAVE_GRAPHVIZ_FDP
dist-check-graphviz:
else
dist-check-graphviz:
	@echo "*** Graphviz's fdp tool must be installed in order to make dist"
	@false
endif

ontology.dot: ontology-graph
	$(AM_V_GEN) ./ontology-graph -d $(top_srcdir)/data/ontologies -o ontology.dot

ontology.png: ontology.dot
	$(AM_V_GEN)$(GRAPHVIZ_FDP) -Tpng -o $(top_srcdir)/docs/reference/ontology/ontology.png ontology.dot

BUILT_SOURCES = gen-doc.stamp

EXTRA_DIST = 			\
	gen-doc.sh		\
	$(BUILT_SOURCES)

DISTCLEANFILES = ontology.dot
CLEANFILES = $(BUILT_SOURCES)

dist-hook: dist-check-graphviz ontology.dot ontology.png