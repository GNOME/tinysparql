INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	-DLIBDIR=\""$(libdir)"\"	\
	-D_XOPEN_SOURCE_EXTENDED=1	\
	-D_GNU_SOURCE=1			\
	-D__EXTENSIONS__=1		\
	-D_HPUX_SOURCE=1		\
	-D_POSIX_MAPPED_FILES=1		\
	-D_POSIX_SYNCHRONIZED_IO=1	\
	-DPIC=1				\
	-D_REENTRANT=1			\
	-DNDEBUG 			\
	-DMYNOMMAP			\
	$(GLIB2_CFLAGS)			\
	$(PANGO_CFLAGS)			\
	$(GMIME_CFLAGS)			\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(SQLITE3_CFLAGS)


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


db_sources =				\
	tracker-indexer.c		\
	tracker-indexer.h		\
	tracker-parser.c		\
	tracker-parser.h		\
	tracker-db-sqlite.c		\
	tracker-db-sqlite.h		\
	tracker-db-email.c		\
	tracker-db-email.h

qdbm_sources =				\
	depot.c				\
	depot.h				\
	curia.c				\
	curia.h				\
	myconf.h			\
	myconf.c

bin_PROGRAMS = trackerd

TESTS = test-tracker-indexer

check_PROGRAMS = $(TESTS)

test_tracker_indexer_SOURCES =		\
	tracker-indexer.c		\
	test-tracker-indexer.c		\
	tracker-utils.c			\
	tracker-utils.h			\
	$(qdbm_sources)

test_tracker_indexer_LDADD = 		\
	$(GLIB2_LIBS)			\
	$(GTHREAD_LIBS)			\
	-lz				\
	-lmagic				\
	$(top_builddir)/src/libstemmer/libstemmer-private.la \
	$(top_builddir)/src/xdgmime/libxdgmime.la

trackerd_SOURCES =			\
	trackerd.c			\
	$(inotify_sources)		\
	$(fam_sources)			\
	$(qdbm_sources)			\
	$(db_sources)			\
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
	tracker-ioprio.h


trackerd_LDADD =			\
	$(GLIB2_LIBS)			\
	$(FAM_LIBS)			\
	$(DBUS_LIBS)			\
	$(PANGO_LIBS)			\
	$(GMIME_LIBS)			\
	$(SQLITE3_LIBS)			\
	$(GTHREAD_LIBS)			\
	-lz				\
	-lmagic				\
	$(top_builddir)/src/libstemmer/libstemmer-private.la \
	$(top_builddir)/src/xdgmime/libxdgmime.la
