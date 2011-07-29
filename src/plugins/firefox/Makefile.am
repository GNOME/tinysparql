include $(top_srcdir)/Makefile.decl

SUBDIRS = chrome defaults

extensiondir = $(datadir)/xul-ext/trackerfox
extension_DATA = \
	install.rdf \
	chrome.manifest \
	README \
	COPYRIGHT

install-data-hook:
	mkdir -p "$(TRACKER_MINER_FIREFOX_INSTALL_DIR)"
	$(LN_S) "$(extensiondir)" "$(TRACKER_MINER_FIREFOX_INSTALL_DIR)/trackerfox@bustany.org"

uninstall-hook:
	$(RM) "$(TRACKER_MINER_FIREFOX_INSTALL_DIR)/trackerfox@bustany.org"