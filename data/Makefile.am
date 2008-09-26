include $(top_srcdir)/Makefile.decl

SUBDIRS = dbus modules languages icons services

trackerd.desktop: trackerd.desktop.in
	@sed -e "s|@libexec[@]|${libexecdir}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = trackerd.desktop.in
autostart_DATA = trackerd.desktop

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tracker.pc

if ENABLE_LIBTRACKERGTK
pkgconfig_DATA += libtracker-gtk.pc
endif

configdir = $(datadir)/tracker

config_DATA =						\
	sqlite-tracker.sql 				\
	sqlite-cache.sql 				\
	sqlite-contents.sql				\
	sqlite-email.sql				\
	sqlite-service-triggers.sql 			\
	sqlite-tracker-triggers.sql 			\
	sqlite-user-data.sql 				\
	sqlite-stored-procs.sql 			\
	sqlite-service.sql				\
	sqlite-service-types.sql 			\
	sqlite-metadata.sql 				\
	sqlite-xesam.sql

CLEANFILES = $(autostart_DATA)

DISTCLEANFILES = $(autostart_in_files)

EXTRA_DIST = $(config_DATA)
