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
	$(top_srcdir)/docs/tools/gen-doc.sh
	$(AM_V_GEN) touch $@

ontology.dot: ontology-graph
	$(AM_V_GEN) ./ontology-graph -d $(top_srcdir)/data/ontologies -o ontology.dot

ontology.png: ontology.dot
	$(AM_V_GEN)$(GRAPHVIZ_FDP) -Tpng -o ontology.png ontology.dot
	cp ontology.png $(top_builddir)/docs/reference/ontology/

#
# The origianal diagram but with .png extension
# Horrible, i know.
#
ONTOLOGY_DIAGRAMS=							\
	../ontologies/notation.png					\
	../ontologies/mfo/feeds-overview.png				\
	../ontologies/nmm/radio-overview.png				\
	../ontologies/nmm/images-overview.png				\
	../ontologies/mlo/location-overview.png				\
	../ontologies/nie/overview.png					\
	../ontologies/nmo/message-class-overview.png			\
	../ontologies/nmo/email-mimeparts-overview.png

.dia.png:
	dia -t png $<
	cp $(notdir $(<:.dia=.png)) $@
	cp $@ $(top_builddir)/docs/reference/ontology

BUILT_SOURCES = 							\
	$(ONTOLOGY_DIAGRAMS)						\
	ontology.png							\
	gen-doc.stamp

EXTRA_DIST = 								\
	$(ONTOLOGY_DIAGRAMS)						\
	gen-doc.sh

DISTCLEANFILES = 							\
	ontology.dot							\
	$(top_builddir)/docs/reference/ontology/notation.png		\
	$(top_builddir)/docs/reference/ontology/ontology.png

CLEANFILES = $(BUILT_SOURCES) $(notdir $(ONTOLOGY_DIAGRAMS:.dia=.png))

make-hook: ontology-graph ontology.dot ontology.png
