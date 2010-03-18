include $(top_srcdir)/Makefile.decl

SUBDIRS = db dbus languages icons ontologies miners

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

if USING_MINER_RSS
tracker-miner-rss.desktop.in: tracker-miner-rss.desktop.in.in
	@$(SED) -e "s|@libexecdir[@]|${libexecdir}|" 	\
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@
endif

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_in_files = 				\
	tracker-status-icon.desktop.in.in		\
	tracker-store.desktop.in.in			\
	tracker-miner-fs.desktop.in.in

if USING_MINER_RSS
autostart_in_in_files += tracker-miner-rss.desktop.in.in
endif

autostart_in_files = 					\
	tracker-status-icon.desktop.in			\
	tracker-store.desktop.in			\
	tracker-miner-fs.desktop.in

if USING_MINER_RSS
autostart_in_files += tracker-miner-rss.desktop.in
endif

autostart_DATA = 					\
	tracker-store.desktop				\
	tracker-miner-fs.desktop

if HAVE_TRACKER_STATUS_ICON
autostart_DATA += tracker-status-icon.desktop
endif

if USING_MINER_RSS
autostart_DATA += tracker-miner-rss.desktop
endif

@INTLTOOL_DESKTOP_RULE@

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
	$(autostart_in_files)				\
	tracker-client-$(TRACKER_API_VERSION).pc	\
	tracker-extract-$(TRACKER_API_VERSION).pc	\
	tracker-miner-$(TRACKER_API_VERSION).pc

EXTRA_DIST = $(autostart_in_in_files)
