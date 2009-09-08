include $(top_srcdir)/Makefile.decl

SUBDIRS = db dbus languages icons ontologies

# Should do this for *.desktop.in.in 
tracker-status-icon.desktop.in: tracker-status-icon.desktop.in.in
	@sed -e "s|@bindir[@]|${bindir}|" 		\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-store.desktop.in: tracker-store.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-fs.desktop.in: tracker-miner-fs.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_in_files = 				\
	tracker-status-icon.desktop.in.in		\
	tracker-store.desktop.in.in			\
	tracker-miner-fs.desktop.in.in

autostart_in_files = 					\
	tracker-status-icon.desktop.in			\
	tracker-store.desktop.in			\
	tracker-miner-fs.desktop.in

autostart_DATA = 					\
	tracker-store.desktop				\
	tracker-miner-fs.desktop

if HAVE_TRACKER_STATUS_ICON
autostart_DATA += tracker-status-icon.desktop
endif

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 					\
	tracker-$(TRACKER_API_VERSION).pc		\
	tracker-miner-$(TRACKER_API_VERSION).pc

tracker-$(TRACKER_API_VERSION).pc: tracker.pc
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).pc: tracker-miner.pc
	cp $< $@

CLEANFILES = 						\
	$(autostart_DATA) 				\
	$(autostart_in_files)				\
	tracker-$(TRACKER_API_VERSION).pc		\
	tracker-miner-$(TRACKER_API_VERSION).pc

if HAVE_LIBTRACKERGTK
pkgconfig_DATA += libtracker-gtk-$(TRACKER_API_VERSION).pc

libtracker-gtk-$(TRACKER_API_VERSION).pc: libtracker-gtk.pc
	cp $< $@

CLEANFILES += libtracker-gtk-$(TRACKER_API_VERSION).pc
endif

EXTRA_DIST = $(autostart_in_in_files)
