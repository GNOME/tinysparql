AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_srcdir)/src/sqlite                     \
	-DNDEBUG                                       \
	$(LIBTRACKER_FTS_CFLAGS)

noinst_LTLIBRARIES = libtracker-fts.la

fts4_sources =                                         \
	fts3_aux.c                                     \
	fts3.c                                         \
	fts3_expr.c                                    \
	fts3.h                                         \
	fts3_hash.c                                    \
	fts3_hash.h                                    \
	fts3_icu.c                                     \
	fts3Int.h                                      \
	fts3_porter.c                                  \
	fts3_snippet.c                                 \
	fts3_term.c                                    \
	fts3_tokenizer1.c                              \
	fts3_tokenizer.c                               \
	fts3_tokenizer.h                               \
	fts3_unicode.c                                 \
	fts3_unicode2.c                                \
	fts3_write.c

libtracker_fts_la_SOURCES =                            \
	tracker-fts.c                                  \
	tracker-fts.h                                  \
	tracker-fts-config.c                           \
	tracker-fts-config.h                           \
	tracker-fts-tokenizer.c                        \
	tracker-fts-tokenizer.h

if !HAVE_BUILTIN_FTS
  libtracker_fts_la_SOURCES += $(fts4_sources)
endif

libtracker_fts_la_LIBADD =                             \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/sqlite/libtracker-sqlite.la            \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_FTS_LIBS)

EXTRA_DIST = $(fts4_sources)

# Configuration / GSettings
gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-enums.h

gsettings_SCHEMAS = org.freedesktop.Tracker.FTS.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

if HAVE_XSLTPROC
DATE=`date +'%B %Y'`
man_MANS = tracker-fts.cfg.5
else
man_MANS =
endif

%.cfg.5: $(gsettings_SCHEMAS)
	$(AM_V_GEN)$(XSLTPROC) \
	  --stringparam version "${VERSION}" \
	  --stringparam current-date "${DATE}" \
	  --stringparam config-file "$(@:.cfg.5=.cfg)" \
	  $(top_srcdir)/docs/manpages/gsettings.xsl $^ > $@

# Dist / Clean rules for all data
CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(man_MANS)

EXTRA_DIST += \
	$(gsettings_SCHEMAS:.xml=.xml.in) \
	$(man_MANS)
