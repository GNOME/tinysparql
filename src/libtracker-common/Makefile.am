include $(top_srcdir)/Makefile.decl

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
	tracker-type-utils.c \
	tracker-utils.c \
	tracker-crc32.c \
	tracker-locale.c \
	tracker-albumart.c

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
	tracker-type-utils.h \
	tracker-utils.h \
	tracker-crc32.h \
	tracker-locale.h \
	tracker-albumart.h

if HAVE_TRACKER_FTS
libtracker_common_la_SOURCES += tracker-language.c
noinst_HEADERS += tracker-language.h
endif

if HAVE_MEEGOTOUCH
libtracker_common_la_SOURCES += tracker-locale-gconfdbus.c
noinst_HEADERS += tracker-locale-gconfdbus.h
endif

libtracker_common_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_common_la_LIBADD = \
	$(BUILD_LIBS) \
	$(LIBTRACKER_COMMON_LIBS)

if HAVE_TRACKER_FTS
libtracker_common_la_LIBADD += $(top_builddir)/src/libstemmer/libstemmer.la
endif

marshal_sources = \
	tracker-marshal.h \
	tracker-marshal.c

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ && \
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

tracker-enum-types.h: tracker-enum-types.h.template $(NOINST_H_FILES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.h.template $(noinst_HEADERS)) > $@

tracker-enum-types.c: tracker-enum-types.c.template $(NOINST_H_FILES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-enum-types.c.template $(noinst_HEADERS)) > $@


BUILT_SOURCES =	                                       \
	$(marshal_sources)                             \
	tracker-enum-types.c                           \
	tracker-enum-types.h

CLEANFILES =                                           \
	$(marshal_sources)                             \
	tracker-enum-types.c                           \
	tracker-enum-types.h

EXTRA_DIST =                                           \
	tracker-marshal.list                           \
	libtracker-common.vapi                         \
	tracker-enum-types.c.template                  \
	tracker-enum-types.h.template

