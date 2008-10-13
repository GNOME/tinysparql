include $(top_srcdir)/Makefile.decl

SUBDIRS = db dbus modules languages icons services

trackerd.desktop: trackerd.desktop.in
	@sed -e "s|@libexec[@]|${libexecdir}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = trackerd.desktop.in
autostart_DATA = trackerd.desktop

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 			\
	tracker.pc			\
	tracker-indexer-module-1.0.pc

if ENABLE_LIBTRACKERGTK
pkgconfig_DATA += libtracker-gtk.pc
endif

CLEANFILES = $(autostart_DATA)

DISTCLEANFILES = $(autostart_in_files)