include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-validator data-validator $(TEST_PROGS)

#TEST_PROGS += qname-test

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(DBUS_CFLAGS)

ontology_validator_SOURCES =						\
	ontology-validator.c

ontology_validator_LDADD =						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)

data_validator_SOURCES = 						\
	data-validator.c

data_validator_LDADD =							\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GIO_LIBS)

#qname_test_SOURCES = qname-test.c qname.c
#qname_test_LDADD = 	$(DBUS_LIBS)					\
#			$(GLIB2_LIBS)					\
#			$(GIO_LIBS)

EXTRA_DIST = file-class.cache.test

