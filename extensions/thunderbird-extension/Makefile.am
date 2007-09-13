PROJECT = tracker
VERSION = 0.1.3

all: $(PROJECT).xpi

JAR_FILES =					\
	content/tracker.css			\
	content/trackerIndexer.js		\
	content/trackerMailWindow.xul		\
	content/trackerMessenger.xul		\
	content/trackerPrefs.xul			\
	content/trackerQueue.js			\
	content/trackerService.js		\
	content/trackerSettings.js		\
	content/trackerUnindex.js		\
	content/trackerUnindex.xul		\
	content/trackerUtils.js			\
	content/contents.rdf			\
	locale/en-US/tracker.dtd			\
	locale/en-US/strings.properties		\
	locale/en-US/contents.rdf		\
	skin/classic/overlay.css		\
	skin/classic/tracker.png			\
	skin/classic/tracker-disabled.png	\
	skin/classic/tracker-error.png		\
	skin/classic/contents.rdf

XPI_FILES =					\
	chrome.manifest				\
	install.rdf				\
	chrome/$(PROJECT).jar			\
	defaults/preferences/default.js		\
	components/TrackerIndexer.js		\
	components/TrackerQueue.js		\
	components/TrackerSettings.js		\
	components/TrackerCommandLine.js

chrome/$(PROJECT).jar: $(JAR_FILES)
	-@mkdir -p chrome
	zip -q9 $@ $^

$(PROJECT).xpi: $(XPI_FILES)
	zip -q9 $@ $^

EXTRA_DIST =					\
	$(JAR_FILES)				\
	$(XPI_FILES)

CLEANFILES =					\
	chrome/$(PROJECT).jar			\
	$(PROJECT).xpi				
