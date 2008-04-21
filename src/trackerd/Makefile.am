module_flags = -module -avoid-version -no-undefined
mail_modulesdir = $(libdir)/tracker/mail-modules

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	$(GLIB2_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(GMIME_CFLAGS)							\
	$(HAL_CFLAGS)							\
	$(FAM_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(QDBM_CFLAGS)							\
	$(UNAC_CFLAGS)							\
	-DMAIL_MODULES_DIR=\"$(mail_modulesdir)\"			\
	-I$(top_srcdir)/src

if HAVE_INOTIFY
watch_sources =								\
	linux-inotify-syscalls.h					\
	tracker-inotify.c
else
if HAVE_FAM
watch_sources = tracker-fam.c
else
if OS_WIN32
watch_sources = tracker-win-fs-changes.c
else
watch_sources = tracker-poll.c
endif
endif
endif

if OS_WIN32
win_sources = tracker-os-dependant-win.c
else
unix_sources = tracker-os-dependant-unix.c
endif

if HAVE_HAL
hal_sources = 								\
	tracker-hal.c             					\
	tracker-hal.h
endif

db_sources =								\
	tracker-indexer.c						\
	tracker-indexer.h						\
	tracker-cache.c							\
	tracker-cache.h							\
	tracker-parser.c						\
	tracker-parser.h						\
	tracker-db-interface.c						\
	tracker-db-interface.h						\
	tracker-db-interface-sqlite.c					\
	tracker-db-interface-sqlite.h					\
	tracker-db-sqlite.c						\
	tracker-db-sqlite.h						\
	tracker-db-email.c						\
	tracker-db-email.h


bin_PROGRAMS = trackerd

trackerd_SOURCES =							\
	trackerd.c							\
	$(watch_sources)						\
	$(win_sources)							\
	$(unix_sources)							\
	$(hal_sources)							\
	$(db_sources)							\
	tracker-apps.c							\
	tracker-apps.h							\
	tracker-db.c							\
	tracker-db.h							\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-dbus-files.c						\
	tracker-dbus-files.h						\
	tracker-dbus-methods.c						\
	tracker-dbus-methods.h						\
	tracker-dbus-metadata.c						\
	tracker-dbus-metadata.h						\
	tracker-dbus-keywords.c						\
	tracker-dbus-keywords.h						\
	tracker-dbus-search.c						\
	tracker-dbus-search.h						\
	tracker-email.c							\
	tracker-email.h							\
	tracker-email-utils.c						\
	tracker-email-utils.h						\
	tracker-metadata.c						\
	tracker-metadata.h						\
	tracker-process-files.c						\
	tracker-process-files.h						\
	tracker-process-requests.c					\
	tracker-process-requests.h					\
	tracker-query-tree.c						\
	tracker-query-tree.h						\
	tracker-rdf-query.c						\
	tracker-rdf-query.h						\
	tracker-utils.c							\
	tracker-utils.h							\
	tracker-service.c						\
	tracker-service.h						\
	tracker-service-manager.c					\
	tracker-service-manager.h					\
	tracker-ioprio.c						\
	tracker-ioprio.h						\
	tracker-os-dependant.h						\
	tracker-watch.h

if OS_WIN32
trackerd_win_libs = -lws2_32 -lkernel32
endif

trackerd_LDADD =							\
	$(GLIB2_LIBS)							\
	$(GMODULE_LIBS)							\
	$(FAM_LIBS)							\
	$(DBUS_LIBS)							\
	$(PANGO_LIBS)							\
	$(GMIME_LIBS)							\
	$(HAL_LIBS)							\
	$(SQLITE3_LIBS)							\
	$(QDBM_LIBS)							\
	$(UNAC_LIBS)							\
	$(GTHREAD_LIBS)							\
	-lz								\
	-lm								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer-private.la 		\
	$(top_builddir)/src/xdgmime/libxdgmime.la 			\
	$(trackerd_win_libs)

mail_modules_LTLIBRARIES = \
	libemail-evolution.la \
	libemail-thunderbird.la \
	libemail-kmail.la \
	libemail-modest.la

libemail_modest_la_SOURCES = tracker-email-modest.c tracker-email-modest.h
libemail_modest_la_LDFLAGS = $(module_flags)
libemail_modest_la_LIBADD = $(GLIB2_LIBS)

libemail_evolution_la_SOURCES = tracker-email-evolution.c
libemail_evolution_la_LDFLAGS = $(module_flags)
libemail_evolution_la_LIBADD = $(GLIB2_LIBS)

libemail_thunderbird_la_SOURCES = tracker-email-thunderbird.c
libemail_thunderbird_la_LDFLAGS = $(module_flags)
libemail_thunderbird_la_LIBADD = $(GLIB2_LIBS)

libemail_kmail_la_SOURCES = tracker-email-kmail.c
libemail_kmail_la_LDFLAGS = $(module_flags)
libemail_kmail_la_LIBADD = $(GLIB2_LIBS)
