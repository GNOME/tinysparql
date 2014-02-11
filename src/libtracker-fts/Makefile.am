AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
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
	tracker-fts-tokenizer.h                        \
	tracker-parser-utils.c                         \
	tracker-parser-utils.h                         \
	tracker-parser.h

if !HAVE_BUILTIN_FTS
  libtracker_fts_la_SOURCES += $(fts4_sources)
endif

if BUILD_LIBUNISTRING_PARSER
  libtracker_fts_la_SOURCES += tracker-parser-libunistring.c
endif

if BUILD_LIBICU_PARSER
  libtracker_fts_la_SOURCES += tracker-parser-libicu.c
endif

libtracker_fts_la_LIBADD =                             \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_FTS_LIBS)

EXTRA_DIST = $(fts4_sources)
