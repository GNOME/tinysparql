include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-I$(top_srcdir)/src						\
	-DTRACKER_COMPILATION						\
	-DNDEBUG							\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(UNAC_CFLAGS)							\
	$(SQLITE3_CFLAGS)

if HAVE_LIBUNISTRING
  INCLUDES += $(LIBUNISTRING_CFLAGS)
else
if HAVE_LIBICU
  INCLUDES += $(LIBICU_CFLAGS)
else
  INCLUDES += $(PANGO_CFLAGS)
endif
endif

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

if HAVE_LIBUNISTRING
  libtracker_fts_la_SOURCES += tracker-parser-libunistring.c
else
if HAVE_LIBICU
  libtracker_fts_la_SOURCES += tracker-parser-libicu.c
else
  libtracker_fts_la_SOURCES += tracker-parser-glib.c
endif
endif

libtracker_fts_la_LIBADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GCOV_LIBS)							\
	$(UNAC_LIBS)							\
	$(GLIB2_LIBS)

if HAVE_LIBUNISTRING
  libtracker_fts_la_LIBADD += $(LIBUNISTRING_LIBS)
else
if HAVE_LIBICU
  libtracker_fts_la_LIBADD += $(LIBICU_LIBS)
else
  libtracker_fts_la_LIBADD += $(PANGO_LIBS)
endif
endif
