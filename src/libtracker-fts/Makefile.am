include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-I$(top_srcdir)/src						\
	-DTRACKER_COMPILATION						\
	-DNDEBUG							\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(LIBTRACKER_FTS_CFLAGS)					\
	$(UNICODE_SUPPORT_CFLAGS)

noinst_LTLIBRARIES = libtracker-fts.la

libtracker_fts_la_SOURCES = 						\
	tracker-fts.c							\
	tracker-fts.h							\
	tracker-fts-config.c						\
	tracker-fts-config.h						\
	tracker-fts-hash.c						\
	tracker-fts-hash.h						\
	tracker-parser-utils.c						\
	tracker-parser-utils.h						\
	tracker-parser.h

if BUILD_LIBUNISTRING_PARSER
  libtracker_fts_la_SOURCES += tracker-parser-libunistring.c
else
if BUILD_LIBICU_PARSER
  libtracker_fts_la_SOURCES += tracker-parser-libicu.c
else
  libtracker_fts_la_SOURCES += tracker-parser-glib.c
endif
endif

libtracker_fts_la_LIBADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GCOV_LIBS)							\
	$(LIBTRACKER_FTS_LIBS)						\
	$(UNICODE_SUPPORT_LIBS)
