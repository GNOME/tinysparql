include $(top_srcdir)/Makefile.decl

SUBDIRS = chrome defaults

extensiondir = $(datadir)/xul-ext/trackerbird
extension_DATA = \
	install.rdf \
	chrome.manifest \
	README

install-data-hook:
	mkdir -p "$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)"
	$(LN_S) "$(extensiondir)" "$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"

uninstall-hook:
	$(RM) "$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"