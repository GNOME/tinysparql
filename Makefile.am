SUBDIRS = data src filters thumbnailers docs po

pkgconfigdir = $(libdir)/pkgconfig
xdgconfigdir = $(sysconfdir)/xdg/autostart
pkgconfig_DATA = tracker.pc
xdgconfig_DATA = trackerd.desktop

handlerdir = $(DESKBAR_HANDLER_DIR)
handler_DATA =  python/deskbar-handler/tracker-handler.py 


EXTRA_DIST = \
	$(xdgconfig_DATA) \
	$(handler_DATA) \
	rdf-query-examples/80s-music.rdf \
	rdf-query-examples/90s-music.rdf \
	rdf-query-examples/all-documents.rdf \
	rdf-query-examples/big-documents.rdf \
	rdf-query-examples/wallpapers.rdf \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in 
	


DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = \
	--with-session-bus-services-dir="\$(datadir)"/dbus-1/services \
	--with-deskbar-applet-handler-dir="\$(libdir)"/deskbar-applet/handlers 
