SUBDIRS = icons

bin_PROGRAMS = tracker-needle

AM_VALAFLAGS = \
	--pkg gtk+-3.0 --pkg gio-2.0 \
	$(BUILD_VALAFLAGS) \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	config.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_VALACFLAGS)                            \
	-D_XOPEN_SOURCE                                \
	-D_XOPEN_SOURCE_EXTENDED                       \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"       \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	$(TRACKER_NEEDLE_CFLAGS)                       \
	-include config.h

LDADD =                                            \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_NEEDLE_LIBS)

tracker_needle_SOURCES = \
	tracker-history.vala \
	tracker-query.vala \
	tracker-result-store.vala \
	tracker-stats.vala \
	tracker-tags-view.vala \
	tracker-utils.vala \
	tracker-needle.vala \
	tracker-view.vala

@INTLTOOL_DESKTOP_RULE@

uidir = $(datadir)/tracker
ui_DATA = tracker-needle.ui

desktopdir = $(datadir)/applications
desktop_DATA = tracker-needle.desktop

appdatadir = $(datadir)/appdata
appdata_DATA = tracker-needle.appdata.xml

BUILT_SOURCES = \
	tracker_needle_vala.stamp

#Desktop files
%.desktop.in: %.desktop.in.in
	@sed -e "s|@VERSION[@]|${VERSION}|" $< > $@

#GSettings
gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-enums.h

gsettings_SCHEMAS = $(schemalist)

schemalist = org.freedesktop.Tracker.Needle.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

# Dist / Clean rules for all data
CLEANFILES = \
	$(desktop_DATA:.desktop=.desktop.in) \
	$(desktop_DATA) \
	$(gsettings_SCHEMAS)

EXTRA_DIST = \
	$(desktop_DATA:.desktop=.desktop.in.in) \
	$(ui_DATA) \
	$(desktop_DATA) \
	$(appdata_DATA) \
	config.vapi \
	$(gsettings_SCHEMAS:.xml=.xml.in) \
	meson.build
