SUBDIRS = stop-words

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-DSHAREDIR=\""$(datadir)"\" \
	$(LIBTRACKER_COMMON_CFLAGS)

libtracker_commondir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_common_LTLIBRARIES = libtracker-common.la

libtracker_common_la_SOURCES = \
	$(BUILT_SOURCES) \
	tracker-date-time.c \
	tracker-dbus.c \
	tracker-file-utils.c \
	tracker-ioprio.c \
	tracker-log.c \
	tracker-sched.c \
	tracker-storage.c \
	tracker-type-utils.c \
	tracker-utils.c \
	tracker-locale.c \
	tracker-parser-utils.c \
	tracker-language.c

noinst_HEADERS = \
	tracker-dbus.h \
	tracker-enums.h \
	tracker-ioprio.h \
	tracker-log.h \
	tracker-common.h \
	tracker-date-time.h \
	tracker-file-utils.h \
	tracker-sched.h \
	tracker-storage.h \
	tracker-type-utils.h \
	tracker-utils.h \
	tracker-locale.h \
	tracker-parser.h \
	tracker-parser-utils.h \
	tracker-language.h


if BUILD_LIBUNISTRING_PARSER
  libtracker_common_la_SOURCES += tracker-parser-libunistring.c
endif

if BUILD_LIBICU_PARSER
  libtracker_common_la_SOURCES += tracker-parser-libicu.c
endif

libtracker_common_la_LIBADD = \
	$(BUILD_LIBS) \
	$(LIBTRACKER_COMMON_LIBS) \
	-lm

tracker-enum-types.h: tracker-enum-types.h.template $(NOINST_H_FILES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.h.template $(noinst_HEADERS)) > $@

tracker-enum-types.c: tracker-enum-types.c.template $(NOINST_H_FILES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.c.template $(noinst_HEADERS)) > $@


BUILT_SOURCES =	                                       \
	tracker-enum-types.c                           \
	tracker-enum-types.h

CLEANFILES =                                           \
	tracker-enum-types.c                           \
	tracker-enum-types.h

EXTRA_DIST =                                           \
	libtracker-common.vapi                         \
	tracker-enum-types.c.template                  \
	tracker-enum-types.h.template

