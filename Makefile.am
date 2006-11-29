SUBDIRS = data src filters thumbnailers docs po

pkgconfigdir = $(libdir)/pkgconfig
xdgconfigdir = $(sysconfdir)/xdg/autostart
pkgconfig_DATA = tracker.pc
xdgconfig_DATA = trackerd.desktop

EXTRA_DIST = \
	$(pkgconfig_DATA) \
	$(xdgconfig_DATA) \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	configure.in \
	rdf-query-examples/*.rdf \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in 

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
