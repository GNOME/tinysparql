include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-graph ttl2sgml

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	-I$(top_builddir)/src/libtracker-data				\
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

EXTRA_DIST = gen-doc.sh
