include $(top_srcdir)/Makefile.decl

libexec_PROGRAMS = tracker-search-bar

tracker_search_bar_SOURCES =                           \
	tracker-applet.c                               \
	tracker-applet.h                               \
	tracker-aligned-window.c                       \
	tracker-aligned-window.h                       \
	tracker-results-window.c                       \
	tracker-results-window.h                       \
	tracker-utils.c                                \
	tracker-utils.h

# -DMENU_UI_DIR=\""$(xmluidir)"\"

tracker_search_bar_CFLAGS =                            \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DPKGDATADIR=\""$(pkgdatadir)"\"               \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(TRACKER_SEARCH_BAR_CFLAGS)

tracker_search_bar_LDADD =                             \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_SEARCH_BAR_LIBS)

# Panel applet
appletdir = $(datadir)/gnome-panel/applets
applet_in_files = org.gnome.panel.SearchBar.panel-applet.in
applet_DATA = $(applet_in_files:.panel-applet.in=.panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN)sed                                 \
	  -e "s|\@LIBEXECDIR\@|$(libexecdir)|"         \
	  -e "s|\@VERSION\@|$(PACKAGE_VERSION)|"       \
	  $< > $@

%.panel-applet: %.panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

# Applet UIs and menu
uidir = $(pkgdatadir)
ui_DATA =                                             \
	tracker-search-bar.ui                         \
	tracker-search-bar-menu.xml

# DBus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.panel.applet.SearchBarFactory.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.gnome.panel.applet.SearchBarFactory.service: $(service_in_files)
	$(AM_V_GEN)sed                                \
	  -e "s|\@LIBEXECDIR\@|$(libexecdir)|"        \
	  $< > $@

# Distribution details
EXTRA_DIST = \
	org.gnome.panel.SearchBar.panel-applet.in.in \
	$(applet_DATA) \
	$(ui_DATA) \
	$(service_in_files)

CLEANFILES = \
	$(applet_DATA) \
	$(applet_DATA).in \
	$(service_DATA)

@INTLTOOL_SCHEMAS_RULE@
