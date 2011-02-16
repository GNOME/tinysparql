gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-settings.h
gsettings_SCHEMAS = \
	org.freedesktop.Tracker.gschema.xml \
	org.freedesktop.Tracker.Store.gschema.xml \
	org.freedesktop.Tracker.Extractor.gschema.xml \
	org.freedesktop.Tracker.Writeback.gschema.xml \
	org.freedesktop.Tracker.Miner.FileSystem.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

CLEANFILES = $(gsettings_SCHEMAS)

EXTRA_DIST = \
	org.freedesktop.Tracker.enums.xml \
	$(gsettings_SCHEMAS)