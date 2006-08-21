SUBDIRS = data src filters thumbnailers

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tracker.pc

debian_files = \
	debian/changelog \
	debian/compat \
	debian/control \
	debian/copyright \
	debian/libmetatracker0-dev.docs \
	debian/libmetatracker0-dev.install \
	debian/libmetatracker0-dev.links \
	debian/libmetatracker0.docs \
	debian/libmetatracker0.install \
	debian/libmetatracker0.links \
	debian/meta-tracker-dev.docs \
	debian/meta-tracker-dev.install \
	debian/meta-tracker.docs \
	debian/meta-tracker.install \
	debian/meta-tracker-utils.docs \
	debian/meta-tracker-utils.examples \
	debian/meta-tracker-utils.install \
	debian/rules

EXTRA_DIST = \
	$(pkgconfig_DATA) \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	configure.in \
	rdf-query-examples/*.rdf \
	$(debian_files)
