if USING_EXTERNAL_SQLITE
additional_flags= $(SQLITE3_CFLAGS)
additional_libs= $(SQLITE3_LIBS)
else
additional_flags= -I$(top_builddir)/src/sqlite3
additional_libs= $(top_builddir)/src/sqlite3/libsqlite-private.la
endif


INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	-D_XOPEN_SOURCE_EXTENDED=1	\
	-D_GNU_SOURCE=1			\
	-D__EXTENSIONS__=1		\
	-D_HPUX_SOURCE=1		\
	-D_POSIX_MAPPED_FILES=1		\
	-D_POSIX_SYNCHRONIZED_IO=1	\
	-DPIC=1				\
	-D_REENTRANT=1			\
	-DNDEBUG 			\
	-DHAVE_MMAP			\
	$(GLIB2_CFLAGS)			\
	$(PANGO_CFLAGS)			\
	$(GMIME_CFLAGS)			\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(additional_flags)		\
	$(CFLAGS) 			\
	-I$(top_builddir)/src/libstemmer \
	-g 

if HAVE_INOTIFY
inotify_sources = \
	linux-inotify-syscalls.h \
	tracker-inotify.c	\
	tracker-inotify.h	
else
if HAVE_FAM
fam_sources = \
	tracker-fam.c \
	tracker-fam.h
endif
endif


db_sources = \
	tracker-indexer.c	\
	tracker-indexer.h	\
	tracker-parser.c	\
	tracker-parser.h	\
	tracker-db-sqlite.h	\
	tracker-db-sqlite.c	


if BUILD_TEST
bin_PROGRAMS = trackerd test-tracker-indexer
test_tracker_indexer_SOURCES =	tracker-indexer.c test-tracker-indexer.c \
	tracker-utils.c		\
	tracker-utils.h		\
	xdgmime.c		\
	xdgmime.h		\
	xdgmimealias.c 		\
	xdgmimealias.h		\
	xdgmimecache.c 		\
	xdgmimecache.h		\
	xdgmimeglob.c 		\
	xdgmimeglob.h		\
	xdgmimeint.c 		\
	xdgmimeint.h		\
	xdgmimemagic.c 		\
	xdgmimemagic.h		\
	xdgmimeparent.c		\
	xdgmimeparent.h			

test_tracker_indexer_LDADD = $(GLIB2_LIBS)
else
bin_PROGRAMS = trackerd 
endif

trackerd_SOURCES =		\
	trackerd.c		\
	$(inotify_sources)	\
	$(fam_sources)		\
	depot.c			\
	depot.h			\
	curia.c			\
	curia.h			\
	myconf.h		\
	myconf.c		\
	tracker-db.c		\
	tracker-db.h		\
	tracker-dbus.c		\
	tracker-dbus.h		\
	tracker-dbus-files.c	\
	tracker-dbus-files.h	\
	tracker-dbus-methods.c	\
	tracker-dbus-methods.h	\
	tracker-dbus-metadata.c	\
	tracker-dbus-metadata.h	\
	tracker-dbus-keywords.h \
	tracker-dbus-keywords.c \
	tracker-dbus-search.h   \
	tracker-dbus-search.c   \
	$(db_sources)		\
	tracker-mbox-evolution.c \
	tracker-mbox-evolution.h \
	tracker-mbox-thunderbird.c \
	tracker-mbox-thunderbird.h \
	tracker-mbox-kmail.c	\
	tracker-mbox-kmail.h	\
	tracker-mbox.c		\
	tracker-mbox.h		\
	tracker-metadata.c	\
	tracker-metadata.h	\
	tracker-rdf-query.c	\
	tracker-rdf-query.h	\
	tracker-stemmer.c	\
	tracker-stemmer.h	\
	tracker-utils.c		\
	tracker-utils.h		\
	tracker-ioprio.c	\
	tracker-ioprio.h	\
	xdgmime.c		\
	xdgmime.h		\
	xdgmimealias.c 		\
	xdgmimealias.h		\
	xdgmimecache.c 		\
	xdgmimecache.h		\
	xdgmimeglob.c 		\
	xdgmimeglob.h		\
	xdgmimeint.c 		\
	xdgmimeint.h		\
	xdgmimemagic.c 		\
	xdgmimemagic.h		\
	xdgmimeparent.c		\
	xdgmimeparent.h		



trackerd_LDADD = $(GLIB2_LIBS) 	\
		 $(FAM_LIBS)	\
		 $(DBUS_LIBS)	\
		 $(MYSQL_LIBS)	\
		 $(PANGO_LIBS)	\
		 $(GMIME_LIBS)	\
		 $(additional_libs) \
		 $(GTHREAD_LIBS) \
		 -lz	\
		 $(top_builddir)/src/libstemmer/libstemmer-private.la
		 


