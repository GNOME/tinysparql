SUBDIRS = data src filters thumbnailers docs po

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tracker.pc

debian_files = \
	debian/changelog \
	debian/compat \
	debian/control \
	debian/copyright \
	debian/libtrackerclient0-dev.docs \
	debian/libtrackerclient0-dev.install \
	debian/libtrackerclient0-dev.links \
	debian/libtrackerclient0-dev.linda-overrides \
	debian/libtrackerclient0.docs \
	debian/libtrackerclient0.install \
	debian/libtrackerclient0.links \
	debian/libtrackerclient0.linda-overrides \
	debian/tracker-data.docs \
	debian/tracker-data.install \
	debian/tracker-data.linda-overrides \
	debian/tracker.docs \
	debian/tracker.install \
	debian/tracker.manpages \
	debian/tracker-utils.docs \
	debian/tracker-utils.examples \
	debian/tracker-utils.install \
	debian/tracker-utils.linda-overrides \
	debian/tracker-utils.manpages \
	debian/tracker-gnome-search-tool.install \
	debian/tracker-gnome-search-tool.docs \
	debian/tracker-gnome-search-tool.linda-overrides \
	debian/rules

EXTRA_DIST = \
	$(pkgconfig_DATA) \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	configure.in \
	rdf-query-examples/*.rdf \
	trackerd.desktop \
	trackerd-kde.desktop \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(debian_files)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update