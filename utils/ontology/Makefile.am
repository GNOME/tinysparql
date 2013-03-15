include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-validator data-validator

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DTOP_SRCDIR=\"$(top_srcdir)\"                 \
	$(LIBTRACKER_DATA_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_DATA_LIBS)

ontology_validator_SOURCES = ontology-validator.c

data_validator_SOURCES = data-validator.c

EXTRA_DIST = file-class.cache.test

