include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-search-tool

tracker_search_tool_VALASOURCES = 				\
	tracker-query.gs 					\
	tracker-entry.gs 					\
	tracker-result-grid.gs		 			\
	tracker-search-tool.gs 					\
	tracker-category-view.gs 				\
	tracker-metadata-tile.gs		 		\
	tracker-utils.gs

tracker_search_tool_SOURCES = 					\
	$(tracker_search_tool_VALASOURCES:.gs=.c)

tracker_search_tool.gs.stamp: $(tracker_search_tool_VALASOURCES)
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gee-1.0 --pkg gtk+-2.0 $^
	touch tracker_search_tool.gs.stamp

tracker_search_tool_CFLAGS = \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_VALA_CFLAGS)					\
	$(PANGO_CFLAGS)						\
	$(GDKPIXBUF_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	-include$(top_builddir)/config.h  			\
	$(NULL)

tracker_search_tool_LDADD = 					\
	$(TRACKER_APPS_LIBS)					\
	$(TRACKER_VALA_LIBS)					\
	$(GDKPIXBUF_LIBS)					\
	$(GCOV_LIBS)						\
	$(NULL)

uidir = $(datadir)/tracker
ui_DATA = tst.ui

desktopdir = $(datadir)/applications
desktop_DATA = tracker-search-tool.desktop

icondir = $(datadir)/tracker/icons
icon_DATA = thumbnail_frame.png

CLEANFILES = $(desktop_DATA)

EXTRA_DIST = 							\
	$(tracker_search_tool_VALASOURCES) 			\
	$(icon_DATA) 						\
	$(ui_DATA)						\
	tracker_search_tool.gs.stamp

MAINTAINERCLEANFILES = 						\
	tracker_search_tool.gs.stamp 				\
	$(tracker_search_tool_VALASOURCES:.gs=.c)

BUILT_SOURCES = 						\
	tracker_search_tool.gs.stamp

