SUBDIRS = dbus languages icons ontologies miners gschemas

# Set up pkg-config .pc files for exported libraries
if HAVE_LIBMEDIAART
requires="libmediaart-1.0"
else
requires=
endif

# First we fix the VERSION and libexecdir in the desktop files
tracker-extract.desktop.in: tracker-extract.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-store.desktop.in: tracker-store.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-fs.desktop.in: tracker-miner-fs.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-apps.desktop.in: tracker-miner-apps.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-rss.desktop.in: tracker-miner-rss.desktop.in.in
	@$(SED) -e "s|@libexecdir[@]|${libexecdir}|"   \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

tracker-miner-user-guides.desktop.in: tracker-miner-user-guides.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|"      \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files =                                  \
	tracker-extract.desktop.in.in                  \
	tracker-store.desktop.in.in                    \
	tracker-miner-fs.desktop.in.in                 \
	tracker-miner-apps.desktop.in.in               \
	tracker-miner-user-guides.desktop.in.in        \
	tracker-miner-rss.desktop.in.in

# Second take those new .in files and translate them
desktop_in_files =                                     \
	tracker-extract.desktop.in                     \
	tracker-store.desktop.in                       \
	tracker-miner-fs.desktop.in                    \
	tracker-miner-apps.desktop.in                  \
	tracker-miner-user-guides.desktop.in           \
	tracker-miner-rss.desktop.in

# Third we choose what we want installed based on config
autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA =                                       \
	tracker-store.desktop

if HAVE_TRACKER_MINER_FS
autostart_DATA += tracker-miner-fs.desktop
endif

if HAVE_TRACKER_EXTRACT
autostart_DATA += tracker-extract.desktop
endif

if HAVE_TRACKER_MINER_APPS
autostart_DATA += tracker-miner-apps.desktop
endif

if HAVE_TRACKER_MINER_RSS
autostart_DATA += tracker-miner-rss.desktop
endif

if HAVE_TRACKER_MINER_USER_GUIDES
autostart_DATA += tracker-miner-user-guides.desktop
endif

@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =                                       \
	tracker-control-$(TRACKER_API_VERSION).pc      \
	tracker-sparql-$(TRACKER_API_VERSION).pc       \
	tracker-miner-$(TRACKER_API_VERSION).pc

tracker-control-$(TRACKER_API_VERSION).pc: tracker-control.pc
	cp $< $@

tracker-sparql-$(TRACKER_API_VERSION).pc: tracker-sparql.pc
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).pc: tracker-miner.pc
	cp $< $@

CLEANFILES =                                           \
	$(autostart_DATA)                              \
	$(desktop_in_files)                            \
	tracker-control-$(TRACKER_API_VERSION).pc      \
	tracker-sparql-$(TRACKER_API_VERSION).pc       \
	tracker-miner-$(TRACKER_API_VERSION).pc

EXTRA_DIST = $(desktop_in_in_files)
