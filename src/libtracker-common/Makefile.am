include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =						\
	$(BUILD_CFLAGS)					\
	-DSHAREDIR=\""$(datadir)"\"			\
	-I$(top_srcdir)/src				\
	$(HAL_CFLAGS)					\
	$(UPOWER_CFLAGS)				\
	$(LIBTRACKER_COMMON_CFLAGS)

libtracker_commondir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

# Prefer UPower for the power data if it's available, else fall back to HAL
if HAVE_UPOWER
power_headers =						\
	tracker-power.h
power_sources =						\
	tracker-power-upower.c
else
if HAVE_HAL
power_headers =						\
	tracker-power.h
power_sources =						\
	tracker-power-hal.c
endif
endif

libtracker_common_la_SOURCES =	 			\
	$(marshal_sources)				\
	$(os_sources)					\
	$(power_sources)				\
	tracker-config-file.c				\
	tracker-date-time.c				\
	tracker-dbus.c	 				\
	tracker-file-utils.c				\
	tracker-ioprio.c				\
	tracker-keyfile-object.c			\
	tracker-log.c	 				\
	tracker-type-utils.c				\
	tracker-utils.c					\
	tracker-crc32.c

noinst_HEADERS =					\
	$(power_headers)				\
	tracker-dbus.h					\
	tracker-ioprio.h				\
	tracker-log.h					\
	tracker-os-dependant.h				\
	tracker-config-file.h				\
	tracker-common.h				\
	tracker-date-time.h				\
	tracker-file-utils.h				\
	tracker-keyfile-object.h			\
	tracker-ontologies.h				\
	tracker-type-utils.h				\
	tracker-utils.h					\
	tracker-crc32.h

if HAVE_TRACKER_FTS
libtracker_common_la_SOURCES +=	 			\
	tracker-language.c
noinst_HEADERS +=					\
	tracker-language.h
endif

libtracker_common_la_LDFLAGS = 				\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_common_la_LIBADD = 				\
	$(BUILD_LIBS)					\
	$(LIBTRACKER_COMMON_LIBS)			\
	$(HAL_LIBS)					\
	$(UPOWER_LIBS)

if HAVE_TRACKER_FTS
libtracker_common_la_LIBADD += 				\
	$(top_builddir)/src/libstemmer/libstemmer.la
endif

marshal_sources =                              		\
        tracker-marshal.h              			\
        tracker-marshal.c

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ &&	\
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

BUILT_SOURCES =						\
	$(marshal_sources)

CLEANFILES = $(marshal_sources)

EXTRA_DIST = 						\
	tracker-marshal.list				\
	libtracker-common.vapi

