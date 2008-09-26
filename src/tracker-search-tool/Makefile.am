include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
icondir = $(datadir)/tracker/icons

INCLUDES = 	\
	-I$(top_srcdir)/src/libtracker				\
	$(GLIB2_CFLAGS)						\
	$(GNOME_UTILS_CFLAGS)					\
	$(GNOMEVFS_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)					\
	$(DBUS_CFLAGS)						\
	-DG_DISABLE_DEPRECATED					\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" 			\
	-DTRACKER_DATADIR=\""$(datadir)"\"			\
	-DGREP_COMMAND=\""$(GREP_COMMAND)"\"			\
	$(LIBTRACKERGTK_CFLAGS)					
	
bin_PROGRAMS = tracker-search-tool

tracker_search_tool_SOURCES =     \
	tracker-search-tool-support.c   \
	tracker-search-tool-support.h   \
	tracker-search-tool-callbacks.c \
	tracker-search-tool-callbacks.h \
	tracker-search-tool.c	        \
	tracker-search-tool.h		\
	sexy-icon-entry.c		\
	sexy-icon-entry.h

tracker_search_tool_LDADD =   \
	$(GLIB2_LIBS) \
	$(GNOME_UTILS_LIBS) \
	$(GNOMEVFS_LIBS)    \
	$(GNOMEDESKTOP_LIBS) \
	$(DBUS_LIBS)	\
        $(top_builddir)/src/libtracker-gtk/libtracker-gtk.la \
	$(top_builddir)/src/libtracker/libtrackerclient.la 


desktop_in_files = tracker-search-tool.desktop.in
desktop_files = $(desktop_in_files:.desktop.in=.desktop)
 
desktop_DATA = $(desktop_files)
icon_DATA = thumbnail_frame.png

EXTRA_DIST = $(icon_DATA)

CLEANFILES = $(desktop_DATA)

