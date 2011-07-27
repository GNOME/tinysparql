include $(top_srcdir)/Makefile.decl

# $(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)
extensiondir = $(datadir)/xul-ext/trackerbird
extension_DATA = \
	chrome.manifest \
	chrome \
	chrome/skin \
	chrome/skin/overlay.css \
	chrome/locale \
	chrome/locale/en-US \
	chrome/locale/en-US/options.dtd \
	chrome/locale/en-US/about.dtd \
	chrome/locale/en-US/overlay.dtd \
	chrome/locale/en-US/overlay.properties \
	chrome/content \
	chrome/content/mailstore.js \
	chrome/content/options.xul \
	chrome/content/queue.js \
	chrome/content/namespace.js \
	chrome/content/about.xul \
	chrome/content/tb-overlay.xul \
	chrome/content/plugin.js \
	chrome/content/bindings.js \
	chrome/content/email-address-parser.js \
	chrome/content/persistent-store.js \
	chrome/content/ui.js \
	chrome/content/trackerstore.js \
	README \
	defaults \
	defaults/preferences \
	defaults/preferences/prefs.js

install-data-hook:
	$(LN_S) $(extensiondir) "$(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)/{1234}"