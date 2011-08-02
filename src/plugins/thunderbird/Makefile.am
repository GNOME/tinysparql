include $(top_srcdir)/Makefile.decl

SUBDIRS = chrome defaults

extensiondir = $(datadir)/xul-ext/trackerbird
extension_DATA = \
	install.rdf \
	chrome.manifest \
	README

EXTRA_DIST = $(extension_DATA)

install-data-hook:
	$(MKDIR_P) "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)"
	$(LN_S) -f "$(extensiondir)" "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"

uninstall-hook:
	$(RM) "$(DESTDIR)$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/trackerbird@bustany.org"
