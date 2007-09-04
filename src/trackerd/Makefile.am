INCLUDES =				\
	-DTRACKER_DATADIR=\""$(datadir)"\"	\
	-DLIBDIR=\""$(libdir)"\"	\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" \
	$(GLIB2_CFLAGS)			\
	$(PANGO_CFLAGS)			\
	$(GMIME_CFLAGS)			\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(SQLITE3_CFLAGS)		\
	$(UNAC_CFLAGS)		


if HAVE_INOTIFY
inotify_sources =			\
	linux-inotify-syscalls.h	\
	tracker-inotify.c		\
	tracker-inotify.h
else
if HAVE_FAM
fam_sources =				\
	tracker-fam.c			\
	tracker-fam.h 
endif
endif

if OS_WIN32
win_sources =				\
	tracker-win-fs-changes.c	\
	tracker-win-fs-changes.h	\
	tracker-os-dependant-win.c
else
unix_sources = \
	tracker-os-dependant-unix.c
endif

db_sources =				\
	tracker-indexer.c		\
	tracker-indexer.h		\
	tracker-cache.c			\
	tracker-cache.h			\
	tracker-parser.c		\
	tracker-parser.h		\
	tracker-db-sqlite.c		\
	tracker-db-sqlite.h		\
	tracker-db-email.c		\
	tracker-db-email.h


bin_PROGRAMS = trackerd

trackerd_SOURCES =			\
	trackerd.c			\
	$(inotify_sources)		\
	$(fam_sources)			\
	$(win_sources)			\
	$(unix_sources)			\
	$(db_sources)			\
	tracker-apps.c			\
	tracker-apps.h			\
	tracker-db.c			\
	tracker-db.h			\
	tracker-dbus.c			\
	tracker-dbus.h			\
	tracker-dbus-files.c		\
	tracker-dbus-files.h		\
	tracker-dbus-methods.c		\
	tracker-dbus-methods.h		\
	tracker-dbus-metadata.c		\
	tracker-dbus-metadata.h		\
	tracker-dbus-keywords.c		\
	tracker-dbus-keywords.h		\
	tracker-dbus-search.c		\
	tracker-dbus-search.h		\
	tracker-email.c			\
	tracker-email.h			\
	tracker-email-utils.c		\
	tracker-email-utils.h		\
	tracker-email-evolution.c	\
	tracker-email-evolution.h	\
	tracker-email-thunderbird.c	\
	tracker-email-thunderbird.h	\
	tracker-email-kmail.c		\
	tracker-email-kmail.h		\
	tracker-metadata.c		\
	tracker-metadata.h		\
	tracker-rdf-query.c		\
	tracker-rdf-query.h		\
	tracker-stemmer.c		\
	tracker-stemmer.h		\
	tracker-utils.c			\
	tracker-utils.h			\
	tracker-ioprio.c		\
	tracker-ioprio.h		\
	tracker-os-dependant.h

if OS_WIN32
trackerd_win_libs = -lws2_32 -lkernel32
endif

trackerd_LDADD =			\
	$(GLIB2_LIBS)			\
	$(FAM_LIBS)			\
	$(DBUS_LIBS)			\
	$(PANGO_LIBS)			\
	$(GMIME_LIBS)			\
	$(SQLITE3_LIBS)			\
	$(UNAC_LIBS)			\
	$(GTHREAD_LIBS)			\
	-lz				\
	-lm				\
	$(top_builddir)/src/libstemmer/libstemmer-private.la \
	$(top_builddir)/src/xdgmime/libxdgmime.la \
	$(trackerd_win_libs)
