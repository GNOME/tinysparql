include $(top_srcdir)/Makefile.decl

INCLUDES = 				\
	-I$(top_srcdir)/src/libtracker 	\
	$(LIBTRACKERGTK_CFLAGS)
	
libtracker_gtkincdir = $(includedir)/libtracker-gtk

lib_LTLIBRARIES = libtracker-gtk.la

libtracker_gtkinc_HEADERS =		\
	tracker-gtk.h			\
	tracker-utils.h 		\
	tracker-ui.h			\
	tracker-metadata-tile.h		\
	tracker-tag-bar.h		\
	tracker-keyword-store.h

libtracker_gtk_la_SOURCES =		\
	tracker-utils.c			\
	tracker-ui.c 			\
	tracker-metadata-tile.c		\
	tracker-tag-bar.c		\
	tracker-keyword-store.c

libtracker_gtk_la_LIBADD = 		\
	$(top_builddir)/src/libtracker/libtrackerclient.la \
	$(LIBTRACKERGTK_LIBS) 

libtracker_gtk_la_LDFLAGS = -version-info 0:0:0

