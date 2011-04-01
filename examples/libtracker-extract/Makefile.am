include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_EXTRACT_CFLAGS)

noinst_LTLIBRARIES = libextract-mockup.la

libextract_mockup_la_SOURCES = tracker-extract-mockup.c
libextract_mockup_la_LDFLAGS = $(module_flags)
libextract_mockup_la_LIBADD =                          \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_EXTRACT_LIBS)

rules_in_files = 01-mockup.rule.in

%.rule: %.rule.in
	@sed -e "s|@modulesdir[@]|${modulesdir}|" $< > $@

rulesdir = $(datadir)/tracker/extract-rules
rules_DATA = 01-mockup.rule

CLEANFILES = $(rules_DATA)

EXTRA_DIST = $(rules_in_files)