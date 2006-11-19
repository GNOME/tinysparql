#SUBDIRS = data help

@INTLTOOL_DESKTOP_RULE@

Desktopdir = $(datadir)/applications
icondir = $(datadir)/pixmaps/tracker

INCLUDES = 	\
	$(GLIB2_CFLAGS)						\
	$(GNOME_UTILS_CFLAGS)					\
	$(GNOMEVFS_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)					\
	$(DBUS_CFLAGS)						\
	-DG_DISABLE_DEPRECATED					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"			\
	-DDATADIR=\""$(datadir)"\"				\
	-DGREP_COMMAND=\""$(GREP_COMMAND)"\"

bin_PROGRAMS = tracker-search-tool

#schemadir = $(GCONF_SCHEMA_FILE_DIR)
#schema_in_file = tracker-search-tool.schemas.in
#schema_DATA = $(schema_in_file:.schemas.in=.schemas)


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
	$(top_builddir)/src/libtracker/libtrackerclient.la 

#man_MANS = tracker-search-tool.1

Desktop_in_files = tracker-search-tool.desktop.in
Desktop_files = $(Desktop_in_files:.desktop.in=.desktop)
 
Desktop_DATA = $(Desktop_files)
icon_DATA = thumbnail_frame.png tracker.png


#EXTRA_DIST = $(Utilities_in_files) $(Utilities_DATA) $(schema_in_file) $(schema_DATA) $(icon_DATA) $(man_MANS)
EXTRA_DIST = $(Desktop_in_files) $(icon_DATA)

#if GCONF_SCHEMAS_INSTALL
#install-data-local:
#	if test -z "$(DESTDIR)" ; then \
#		for p in $(schema_DATA) ; do \
#		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/tracker-search-tool/$$p ; \
#		done \
#	fi
#else
#install-data-local:
#endif

CLEANFILES = 				\
	$(Desktop_DATA)			\
	tracker-search-tool.schemas

disthook:
	cd $(distdir) ; rm -f $(CLEANFILES)
