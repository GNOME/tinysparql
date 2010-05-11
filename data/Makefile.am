include $(top_srcdir)/Makefile.decl

SUBDIRS = db dbus languages icons ontologies miners

# First we fix the VERSION and libexecdir in the desktop files
tracker-status-icon.desktop.in: tracker-status-icon.desktop.in.in
	@sed -e "s|@bindir[@]|${bindir}|" 		\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-store.desktop.in: tracker-store.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-fs.desktop.in: tracker-miner-fs.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-flickr.desktop.in: tracker-miner-flickr.desktop.in.in
	@$(SED) -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-rss.desktop.in: tracker-miner-rss.desktop.in.in
	@$(SED) -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files = 					\
	tracker-status-icon.desktop.in.in		\
	tracker-store.desktop.in.in			\
	tracker-miner-fs.desktop.in.in			\
	tracker-miner-flickr.desktop.in.in		\
	tracker-miner-rss.desktop.in.in

# Second take those new .in files and translate them
desktop_in_files = 					\
	tracker-status-icon.desktop.in			\
	tracker-store.desktop.in			\
	tracker-miner-fs.desktop.in			\
	tracker-miner-flickr.desktop.in			\
	tracker-miner-rss.desktop.in

# Third we choose what we want installed based on config
autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = 					\
	tracker-store.desktop				\
	tracker-miner-fs.desktop

if HAVE_TRACKER_STATUS_ICON
autostart_DATA += tracker-status-icon.desktop
endif

if HAVE_MINER_RSS
autostart_DATA += tracker-miner-rss.desktop
endif

if HAVE_MINER_FLICKR
autostart_DATA += tracker-miner-flickr.desktop
endif

@INTLTOOL_DESKTOP_RULE@

# Set up pkg-config .pc files for exported libraries
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 					\
	tracker-client-$(TRACKER_API_VERSION).pc	\
	tracker-extract-$(TRACKER_API_VERSION).pc	\
	tracker-miner-$(TRACKER_API_VERSION).pc

tracker-client-$(TRACKER_API_VERSION).pc: tracker-client.pc
	cp $< $@

tracker-extract-$(TRACKER_API_VERSION).pc: tracker-extract.pc
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).pc: tracker-miner.pc
	cp $< $@

CLEANFILES = 						\
	$(autostart_DATA) 				\
	$(desktop_in_files)				\
	tracker-client-$(TRACKER_API_VERSION).pc	\
	tracker-extract-$(TRACKER_API_VERSION).pc	\
	tracker-miner-$(TRACKER_API_VERSION).pc

EXTRA_DIST = $(desktop_in_in_files)
