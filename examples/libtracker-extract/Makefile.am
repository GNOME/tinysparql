module_flags = -module -avoid-version -no-undefined

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_EXTRACT_CFLAGS)

noinst_LTLIBRARIES = libextract-mockup.la

libextract_mockup_la_SOURCES = tracker-extract-mockup.c
libextract_mockup_la_LDFLAGS = $(module_flags)
libextract_mockup_la_LIBADD =                          \
	$(top_builddir)/src/libtracker-extract/libtracker-extract.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_EXTRACT_LIBS)

# This is noinst_ only because it's an example, normally this would be
# "rules" (for rules_DATA and rulesdir) or whatever name you want to
# give for your rules files.
#
# _RULES_DIR = $(datadir)/tracker/extract-rules
#
noinstdir = $(TRACKER_EXTRACT_RULES_DIR)
noinst_DATA = 01-mockup.rule

EXTRA_DIST = $(noinst_DATA)
