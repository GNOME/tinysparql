mysql_flags = -D_THREAD_SAFE -D_REENTRANT
mysql_libs = -L$(libdir) -lmysqld -lz -lm -lcrypt -lstdc++ -lpthread

INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GLIB2_CFLAGS)			\
	$(FAM_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(mysql_flags)			\
	-g -W -Wall 



bin_PROGRAMS = trackerd

trackerd_SOURCES =		\
	trackerd.c		\
	tracker-db.c		\
	tracker-db.h		\
	tracker-dbus.c		\
	tracker-dbus.h		\
	tracker-dbus-methods.c	\
	tracker-dbus-methods.h	\
	tracker-fam.c		\
	tracker-fam.h		\
	tracker-metadata.c	\
	tracker-metadata.h	\
	tracker-rdf-query.c	\
	tracker-rdf-query.h	\
	tracker-utils.c		\
	tracker-utils.h		\
	xdgmime.c		\
	xdgmime.h		\
	xdgmimealias.c 		\
	xdgmimecache.c 		\
	xdgmimeglob.c 		\
	xdgmimeint.c 		\
	xdgmimemagic.c 		\
	xdgmimeparent.c
		

trackerd_LDADD = $(GLIB2_LIBS) 	\
		 $(FAM_LIBS)	\
		 $(DBUS_LIBS)	\
		 $(mysql_libs)	

EXTRA_DIST =
