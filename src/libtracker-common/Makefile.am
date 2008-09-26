include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-I$(top_srcdir)/src				\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(UNAC_CFLAGS)					\
	$(PANGO_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(GLIB2_CFLAGS)

libtracker_commondir = $(libdir)/tracker
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

if HAVE_HAL
hal_sources = 						\
	tracker-hal.c
hal_headers = 						\
	tracker-hal.h
endif

libtracker_common_la_SOURCES =	 			\
	$(os_sources)					\
	$(hal_sources)					\
	tracker-config.c 				\
	tracker-configuration.c 			\
	tracker-dbus.c	 				\
	tracker-field.c					\
	tracker-field-data.c				\
	tracker-file-utils.c				\
	tracker-ioprio.c				\
	tracker-language.c				\
	tracker-log.c	 				\
	tracker-module-config.c				\
	tracker-nfs-lock.c				\
	tracker-ontology.c				\
	tracker-parser.c				\
	tracker-service.c				\
	tracker-type-utils.c				\
	tracker-utils.c

noinst_HEADERS =					\
	$(hal_headers)					\
	tracker-config.h				\
	tracker-configuration.h				\
	tracker-dbus.h					\
	tracker-field-data.h				\
	tracker-field.h					\
	tracker-file-utils.h				\
	tracker-ioprio.h				\
	tracker-language.h				\
	tracker-log.h					\
	tracker-module-config.h				\
	tracker-nfs-lock.h				\
	tracker-ontology.h				\
	tracker-os-dependant.h				\
	tracker-parser.h				\
	tracker-service.h				\
	tracker-type-utils.h				\
	tracker-utils.h

libtracker_common_la_LDFLAGS = -version-info 0:0:0
libtracker_common_la_LIBADD = 				\
	$(top_builddir)/src/libstemmer/libstemmer.la	\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)					\
	$(UNAC_LIBS)					\
	$(PANGO_LIBS)					\
	$(GIO_LIBS)					\
	$(GLIB2_LIBS)

