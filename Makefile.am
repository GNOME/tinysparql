SUBDIRS = data src filters thumbnailers docs

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
	debian/tracker.docs \
	debian/tracker.install \
	debian/tracker-utils.docs \
	debian/tracker-utils.examples \
	debian/tracker-utils.install \
	debian/tracker-utils.linda-overrides \
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
	$(debian_files)
