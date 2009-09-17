include $(top_srcdir)/Makefile.decl

libexec_PROGRAMS = tracker-search-bar

tracker_search_bar_VALASOURCES = 				\
	tracker-search-bar.vala

tracker_search_bar_SOURCES = 					\
	$(tracker_search_bar_VALASOURCES:.vala=.c)

tracker_search_bar.vala.stamp: $(tracker_search_bar_VALASOURCES)
	$(VALAC) -C -g --pkg gtk+-2.0 --pkg libgnomeui-2.0 --pkg libpanelapplet-2.0 $^
	touch tracker_search_bar.vala.stamp

tracker_search_bar_CFLAGS = \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(TRACKER_SEARCH_BAR_CFLAGS)

tracker_search_bar_LDADD = 					\
	$(GCOV_LIBS)						\
	$(TRACKER_SEARCH_BAR_LIBS)

uidir = $(datadir)/tracker
ui_DATA = tracker-search-bar.ui

# Panel applet bonobo stuff
serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Search_Bar_Applet.server.in
server_DATA = $(server_in_files:.server.in=.server)

# Misc data
resourcesdir = $(pkgdatadir)
resources_DATA =
#	GNOME_Search_Bar_Applet.xml

tracker_search_bar_dir = $(libexecdir)

# Build rules
@INTLTOOL_SERVER_RULE@
@INTLTOOL_SCHEMAS_RULE@

$(server_in_files): $(server_in_files:.server.in=.server.in.in)
	sed -e "s|\@LIBEXECDIR\@|$(tracker_search_bar_dir)|" $< > $@

BUILT_SOURCES = 						\
	tracker_search_bar.vala.stamp

EXTRA_DIST = 							\
	$(tracker_search_bar_SOURCES) 				\
	$(tracker_search_bar_VALASOURCES) 			\
	$(resources_DATA)					\
	tracker_search_bar.vala.stamp 				\
	tracker-search-bar.ui					\
	GNOME_Search_Bar_Applet.server.in.in

MAINTAINERCLEANFILES = 						\
	$(tracker_search_bar_SOURCES) 				\
	tracker_search_bar.vala.stamp

DISTCLEANFILES = 						\
	$(server_DATA) 						\
	$(server_in_files)
