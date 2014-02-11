SUBDIRS = chrome defaults

extensiondir = $(datadir)/xul-ext/trackerfox
extension_DATA = \
	install.rdf \
	chrome.manifest \
	README \
	COPYRIGHT

EXTRA_DIST = $(extension_DATA)

install-data-hook:
	$(MKDIR_P) "$(DESTDIR)$(TRACKER_MINER_FIREFOX_INSTALL_DIR)"
	$(LN_S) -f "$(extensiondir)" "$(DESTDIR)$(TRACKER_MINER_FIREFOX_INSTALL_DIR)/trackerfox@bustany.org"

uninstall-hook:
	$(RM) "$(DESTDIR)$(TRACKER_MINER_FIREFOX_INSTALL_DIR)/trackerfox@bustany.org"
