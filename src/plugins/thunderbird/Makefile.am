SUBDIRS = chrome defaults

extensiondir = $(datadir)/xul-ext/trackerbird
extension_DATA = \
	install.rdf \
	chrome.manifest \
	README

mimefixdir = $(datadir)/applications/
mimefix_DATA = trackerbird-launcher.desktop

EXTRA_DIST = $(extension_DATA) $(mimefix_DATA)

update-desktop-database:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating .desktop database."; \
		update-desktop-database; \
	else \
		echo "*** .desktop database not updated.  After (un)install, run this:"; \
		echo "***   update-desktop-database"; \
	fi

install-data-hook: update-desktop-database
	$(MKDIR_P) "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)"
	$(LN_S) -f "$(extensiondir)" "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"

uninstall-hook: update-desktop-database
	$(RM) "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"
