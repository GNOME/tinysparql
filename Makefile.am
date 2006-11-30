SUBDIRS = data src filters thumbnailers docs po

pkgconfigdir = $(libdir)/pkgconfig
xdgconfigdir = $(sysconfdir)/xdg/autostart
pkgconfig_DATA = tracker.pc
xdgconfig_DATA = trackerd.desktop

EXTRA_DIST = \
	$(xdgconfig_DATA) \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	configure.in \
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
