include $(top_srcdir)/Makefile.decl

libexec_PROGRAMS = tracker-search-bar

tracker_search_bar_SOURCES = 					\
	tracker-applet.c 					\
	tracker-applet.h					\
	tracker-aligned-window.c				\
	tracker-aligned-window.h				\
	tracker-results-window.c				\
	tracker-results-window.h				\
	tracker-utils.c						\
	tracker-utils.h

tracker_search_bar_CFLAGS = \
	-DPKGDATADIR=\""$(pkgdatadir)"\"	 		\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-I$(top_srcdir)/src					\
	-I$(top_builddir)/src					\
	-I$(top_builddir)/src/libtracker-client			\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_APPLETS_CFLAGS)				\
	$(GDKPIXBUF_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)

tracker_search_bar_LDADD = 					\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(TRACKER_APPS_LIBS)					\
	$(TRACKER_APPLETS_LIBS)					\
	$(GDKPIXBUF_LIBS)					\
	$(GCOV_LIBS)

uidir = $(datadir)/tracker
ui_DATA = tracker-search-bar.ui

# Panel applet bonobo stuff
serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Search_Bar_Applet.server.in
server_DATA = $(server_in_files:.server.in=.server)

# Misc data
resourcesdir = $(pkgdatadir)
resources_DATA =						\
	GNOME_Search_Bar_Applet.xml

# Build rules
@INTLTOOL_SERVER_RULE@
@INTLTOOL_SCHEMAS_RULE@

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	$(AM_V_GEN)sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 							\
	$(resources_DATA)					\
	tracker-search-bar.ui					\
	GNOME_Search_Bar_Applet.server.in.in

DISTCLEANFILES = 						\
	$(server_DATA) 						\
	$(server_in_files)
