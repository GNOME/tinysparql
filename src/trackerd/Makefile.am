if USING_SQLITE
additional_mysql_flags=
else
additional_mysql_flags= -D_THREAD_SAFE -D_REENTRANT	
endif


INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GLIB2_CFLAGS)			\
	$(PANGO_CFLAGS)			\
	$(GMIME_CFLAGS)			\
	$(QDBM_CFLAGS)			\
	$(SQLITE3_CFLAGS)		\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(MYSQL_CFLAGS)			\
	$(POPPLER_GLIB_CFLAGS)			\
	$(VORBIS_CFLAGS)			\
	$(THEORA_CFLAGS)			\
	$(additional_mysql_flags)	\
	$(CFLAGS) 			\
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

if USING_SQLITE
db_sources = \
	tracker-indexer.c	\
	tracker-indexer.h	\
	tracker-parser.c	\
	tracker-parser.h	\
	tracker-db-sqlite.h	\
	tracker-db-sqlite.c	
else
db_sources = \
	tracker-db-mysql.h	\
	tracker-db-mysql.c	
endif

if BUILD_TEST
bin_PROGRAMS = trackerd tracker-convert-file test-tracker-indexer
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

test_tracker_indexer_LDADD = $(GLIB2_LIBS) $(QDBM_LIBS) -lbz2		
else
bin_PROGRAMS = trackerd tracker-convert-file 
endif

trackerd_SOURCES =		\
	trackerd.c		\
	$(inotify_sources)	\
	$(fam_sources)		\
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
	tracker-mbox.c		\
	tracker-mbox.h		\
	tracker-metadata.c	\
	tracker-metadata.h	\
	tracker-metadata-oasis.c	\
	tracker-metadata-ps.c	\
	tracker-metadata-pdf.c	\
	tracker-metadata-abw.c	\
	tracker-metadata-vorbis.c	\
	tracker-rdf-query.c	\
	tracker-rdf-query.h	\
	tracker-stemmer-english.c	\
	tracker-stemmer-english.h	\
	tracker-stemmer.h	\
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



trackerd_LDADD = $(GLIB2_LIBS) 	\
		 $(FAM_LIBS)	\
		 $(DBUS_LIBS)	\
		 $(MYSQL_LIBS)	\
		 $(PANGO_LIBS)	\
		 $(GMIME_LIBS)	\
		 $(QDBM_LIBS) 	\
		 $(SQLITE3_LIBS) \
 		 $(POPPLER_GLIB_LIBS) \
 		 $(VORBIS_LIBS) \
 		 $(THEORA_LIBS) \
		 -lbz2		\
		 -lstdc++


tracker_convert_file_SOURCES =	tracker-convert-file.c	tracker-parser.c tracker-stemmer-english.c

tracker_convert_file_LDADD = $(GLIB2_LIBS) $(PANGO_LIBS) -lbz2	
