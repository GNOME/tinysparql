
PROJECT = firefox-extension-xesam
VERSION = 1.0.1

all: $(PROJECT).xpi

XPI_FILES = 			\
	chrome.manifest		\
	install.rdf		\
	chrome/content/contents.rdf		\
	chrome/content/beagleOverlay.js	\
	chrome/content/beagleOverlay.xul	\
	chrome/content/beaglePrefs.js		\
	chrome/content/beaglePrefs.xul		\
	chrome/content/beagleAddFilter.xul	\
	chrome/content/beagleAddFilter.js	\
	chrome/content/indexLink.xul	\
	chrome/content/indexLink.js	\
	chrome/content/indexBookmark.js	\
	chrome/content/md5.js			\
	chrome/content/utils.js	\
	chrome/content/i18n.js	\
	chrome/content/json.js	\
	\
	chrome/content/jslib/jslib.js		\
	chrome/content/jslib/modules.js	\
	chrome/content/jslib/debug/debug.js	\
	chrome/content/jslib/io/dir.js		\
	chrome/content/jslib/io/dirUtils.js	\
	chrome/content/jslib/io/file.js	\
	chrome/content/jslib/io/fileUtils.js	\
	chrome/content/jslib/io/filesystem.js	\
	\
	chrome/locale/en-US/contents.rdf	\
	chrome/locale/en-US/beagle.dtd		\
	chrome/locale/en-US/beagle.properties	\
	\
	chrome/skin/classic/contents.rdf	\
	chrome/skin/classic/beagle-big.png	\
	chrome/skin/classic/beagle-disabled.png\
	chrome/skin/classic/beagle-error.png	\
	chrome/skin/classic/beagle.png\
	chrome/skin/classic/overlay.css	

$(PROJECT).xpi: $(XPI_FILES)
	zip -q9 $@ $^


EXTRA_DIST =			\
	$(XPI_FILES)

CLEANFILES =			\
	$(PROJECT).xpi
