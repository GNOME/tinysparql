include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE	\
	-I$(top_srcdir)/src				\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	$(GIO_CFLAGS)					\
	$(PANGO_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DEVKIT_POWER_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	$(UNAC_CFLAGS)

libtracker_commondir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_commonincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-common/
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

# For storage, we need HAL, tracker isn't yet ported to DeviceKit-disks
if HAVE_HAL
storage_headers =					\
	tracker-storage.h
storage_sources =					\
	tracker-storage-hal.c
endif

# Prefer DeviceKit-power for the power data if it's available, else fall back to HAL
if HAVE_DEVKIT_POWER
power_headers =						\
	tracker-power.h
power_sources =						\
	tracker-power-devicekit.c
else
if HAVE_HAL
power_headers =						\
	tracker-power.h
power_sources =						\
	tracker-power-hal.c
endif
endif

libtracker_common_la_VALASOURCES =			\
	tracker-sparql-builder.vala			\
	tracker-turtle-writer.vala

libtracker_common_la_SOURCES =	 			\
	$(marshal_sources)				\
	$(os_sources)					\
	$(power_sources)				\
	$(storage_sources)				\
	tracker-config-file.c				\
	tracker-class.c					\
	tracker-dbus.c	 				\
	tracker-file-utils.c				\
	tracker-ioprio.c				\
	tracker-keyfile-object.c			\
	tracker-language.c				\
	tracker-log.c	 				\
	tracker-namespace.c				\
	tracker-ontology.c				\
	tracker-property.c				\
	tracker-statement-list.c			\
	tracker-type-utils.c				\
	tracker-utils.c					\
	libtracker-common.vala.stamp			\
	$(libtracker_common_la_VALASOURCES:.vala=.c)

noinst_HEADERS =					\
	$(power_headers)				\
	$(storage_headers)				\
	tracker-dbus.h					\
	tracker-ioprio.h				\
	tracker-log.h					\
	tracker-os-dependant.h

libtracker_commoninclude_HEADERS =			\
	tracker-class.h					\
	tracker-config-file.h				\
	tracker-common.h				\
	tracker-file-utils.h				\
	tracker-keyfile-object.h			\
	tracker-language.h				\
	tracker-namespace.h				\
	tracker-ontology.h				\
	tracker-property.h				\
	tracker-sparql-builder.h			\
	tracker-statement-list.h			\
	tracker-type-utils.h				\
	tracker-utils.h

libtracker-common.vala.stamp: $(libtracker_common_la_VALASOURCES)
	$(VALAC) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-sparql-builder.h posix.vapi $^
	touch $@

libtracker_common_la_LDFLAGS = 				\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_common_la_LIBADD = 				\
	$(top_builddir)/src/libstemmer/libstemmer.la	\
	$(HAL_LIBS)					\
	$(DEVKIT_POWER_LIBS)				\
	$(DBUS_LIBS)					\
	$(UNAC_LIBS)					\
	$(PANGO_LIBS)					\
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(GDKPIXBUF_LIBS)

marshal_sources =                              		\
        tracker-marshal.h              			\
        tracker-marshal.c

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	echo "#include \"tracker-marshal.h\"" > $@ &&	\
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

BUILT_SOURCES =						\
	$(marshal_sources)				\
	libtracker-common.vala.stamp

CLEANFILES = $(marshal_sources)

MAINTAINERCLEANFILES =					\
	libtracker-common.vala.stamp			\
	$(libtracker_common_la_VALASOURCES:.vala=.c)	\
	tracker-sparql-builder.h			\
	$(NULL)

EXTRA_DIST = 						\
	tracker-marshal.list				\
	$(libtracker_common_la_VALASOURCES)		\
	libtracker-common.vala.stamp			\
	posix.vapi					\
	libtracker-common.vapi

