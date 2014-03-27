AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-DSHAREDIR=\""$(datadir)"\" \
	$(LIBTRACKER_COMMON_CFLAGS)

libtracker_commondir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

libtracker_common_la_SOURCES = \
	$(BUILT_SOURCES) \
	$(os_sources) \
	tracker-config-file.c \
	tracker-date-time.c \
	tracker-dbus.c \
	tracker-file-utils.c \
	tracker-ioprio.c \
	tracker-keyfile-object.c \
	tracker-log.c \
	tracker-sched.c \
	tracker-storage.c \
	tracker-type-utils.c \
	tracker-utils.c \
	tracker-crc32.c \
	tracker-locale.c \
	tracker-miner-locale.c

noinst_HEADERS = \
	tracker-dbus.h \
	tracker-enums.h \
	tracker-ioprio.h \
	tracker-log.h \
	tracker-os-dependant.h \
	tracker-config-file.h \
	tracker-common.h \
	tracker-date-time.h \
	tracker-file-utils.h \
	tracker-keyfile-object.h \
	tracker-ontologies.h \
	tracker-sched.h \
	tracker-storage.h \
	tracker-type-utils.h \
	tracker-utils.h \
	tracker-crc32.h \
	tracker-locale.h \
	tracker-miner-locale.h

if HAVE_TRACKER_FTS
libtracker_common_la_SOURCES += tracker-language.c
noinst_HEADERS += tracker-language.h
endif

if HAVE_MEEGOTOUCH
libtracker_common_la_SOURCES += 
	tracker-locale-gconfdbus.c \
	tracker-locale-gconfdbus.h \
	tracker-meego.cpp \
	tracker-meego.h
noinst_HEADERS += \
	tracker-locale-gconfdbus.h \
	tracker-meego.h
endif

libtracker_common_la_LIBADD = \
	$(BUILD_LIBS) \
	$(LIBTRACKER_COMMON_LIBS) \
	-lm

if HAVE_TRACKER_FTS
libtracker_common_la_LIBADD += $(top_builddir)/src/libstemmer/libstemmer.la
endif

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

