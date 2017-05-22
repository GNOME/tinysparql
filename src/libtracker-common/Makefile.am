SUBDIRS = stop-words

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-DSHAREDIR=\""$(datadir)"\" \
	$(LIBTRACKER_COMMON_CFLAGS)

TRACKER_PARSER_FILES =                \
	tracker-parser.h              \
	tracker-parser-utils.c        \
	tracker-parser-utils.h        \
	tracker-parser-libicu.c       \
	tracker-parser-libunistring.c

tracker-parser-sha1.h: $(TRACKER_PARSER_FILES)
	$(AM_V_GEN) echo $(ECHO_N) "#define TRACKER_PARSER_SHA1 \""`git log -n1 --format=format:%H -- $^`"\"$(ECHO_C)" > $@

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
	tracker-seccomp.c \
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
	tracker-seccomp.h \
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

tracker-enum-types.h: tracker-enum-types.h.template tracker-enums.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.h.template $(noinst_HEADERS)) > $@

tracker-enum-types.c: tracker-enum-types.c.template tracker-enums.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.c.template $(noinst_HEADERS)) > $@


BUILT_SOURCES =	                                       \
	tracker-parser-sha1.h                          \
	tracker-enum-types.c                           \
	tracker-enum-types.h

CLEANFILES =                                           \
	tracker-enum-types.c                           \
	tracker-enum-types.h

EXTRA_DIST =                                           \
	tracker-parser-sha1.h                          \
	libtracker-common.vapi                         \
	tracker-enum-types.c.template                  \
	tracker-enum-types.h.template                  \
	generate-tracker-parser-sha1-header.sh         \
	meson.build
