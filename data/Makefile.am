include $(top_srcdir)/Makefile.decl

SUBDIRS = db dbus modules languages icons ontologies

tracker-store.desktop.in: tracker-store.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_in_files = tracker-store.desktop.in.in
autostart_in_files = tracker-store.desktop.in
autostart_DATA = tracker-store.desktop

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 			\
	tracker-$(TRACKER_API_VERSION).pc	\
	tracker-module-$(TRACKER_API_VERSION).pc

tracker-$(TRACKER_API_VERSION).pc: tracker.pc
	cp $< $@

tracker-module-$(TRACKER_API_VERSION).pc: tracker-module.pc
	cp $< $@

CLEANFILES = $(autostart_DATA) $(autostart_in_files)	\
	tracker-$(TRACKER_API_VERSION).pc		\
	tracker-module-$(TRACKER_API_VERSION).pc

if HAVE_LIBTRACKERGTK
pkgconfig_DATA += libtracker-gtk-$(TRACKER_API_VERSION).pc

libtracker-gtk-$(TRACKER_API_VERSION).pc: libtracker-gtk.pc
	cp $< $@

CLEANFILES += libtracker-gtk-$(TRACKER_API_VERSION).pc
endif

EXTRA_DIST = $(autostart_in_in_files)
