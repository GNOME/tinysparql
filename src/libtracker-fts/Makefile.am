AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-DNDEBUG                                       \
	$(LIBTRACKER_FTS_CFLAGS)

noinst_LTLIBRARIES = libtracker-fts.la

fts5_sources =                                         \
	fts5.c                                         \
	fts5.h

libtracker_fts_la_SOURCES =                            \
	tracker-fts.c                                  \
	tracker-fts.h                                  \
	tracker-fts-config.c                           \
	tracker-fts-config.h                           \
	tracker-fts-tokenizer.c                        \
	tracker-fts-tokenizer.h

if !HAVE_BUILTIN_FTS
  libtracker_fts_la_SOURCES += $(fts5_sources)
endif

libtracker_fts_la_LIBADD =                             \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_FTS_LIBS)

EXTRA_DIST = $(fts5_sources)

# Configuration / GSettings
gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-enums.h

gsettings_SCHEMAS = org.freedesktop.Tracker.FTS.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

# Dist / Clean rules for all data
CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(man_MANS)

EXTRA_DIST += \
	$(gsettings_SCHEMAS:.xml=.xml.in) \
	$(man_MANS) \
	meson.build
