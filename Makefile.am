SUBDIRS = data src filters thumbnailers docs po python

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = trackerd.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tracker.pc

if ENABLE_LIBTRACKERGTK
pkgconfig_DATA += libtracker-gtk.pc
endif

EXTRA_DIST = \
	rdf-query-examples/80s-music.rdf \
	rdf-query-examples/90s-music.rdf \
	rdf-query-examples/all-documents.rdf \
	rdf-query-examples/big-documents.rdf \
	rdf-query-examples/wallpapers.rdf \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in 

CLEANFILES = $(autostart_DATA)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = \
	--with-session-bus-services-dir="\$(datadir)"/dbus-1/services \
	--disable-deskbar-applet
