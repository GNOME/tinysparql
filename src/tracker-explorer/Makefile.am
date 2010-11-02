include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-explorer

AM_VALAFLAGS =                                         \
	-C -g                                          \
	--pkg dbus-glib-1 --pkg gee-1.0 --pkg gtk+-2.0

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"       \
	-DSRCDIR=\"$(abs_srcdir)/\"                    \
	$(TRACKER_EXPLORER_CFLAGS)

LDADD =                                                \
	$(BUILD_LIBS)                                  \
	$(TRACKER_EXPLORER_LIBS)

tracker_explorer_SOURCES =                             \
	tracker-explorer.vala

uidir = $(datadir)/tracker
ui_DATA = tracker-explorer.ui

EXTRA_DIST =                                           \
	$(tracker_explorer_SOURCES)                    \
	$(ui_DATA)

MAINTAINERCLEANFILES =                                 \
	$(tracker_explorer_SOURCES)

