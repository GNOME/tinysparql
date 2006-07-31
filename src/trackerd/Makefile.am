additional_mysql_flags = -D_THREAD_SAFE -D_REENTRANT


INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GLIB2_CFLAGS)			\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(MYSQL_CFLAGS)			\
	$(additional_mysql_flags)	\
	$(CFLAGS) 			\
	-g 

if HAVE_INOTIFY
inotify_sources = \
	tracker-inotify.c	\
	tracker-inotify.h	
else
if HAVE_FAM
fam_sources = \
	tracker-fam.c \
	tracker-fam.h
endif
endif



bin_PROGRAMS = trackerd

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
	tracker-metadata.c	\
	tracker-metadata.h	\
	tracker-rdf-query.c	\
	tracker-rdf-query.h	\
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
		 -lstdc++


