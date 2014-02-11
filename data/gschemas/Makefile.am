gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-enums.h

base_schemas = \
	org.freedesktop.Tracker.gschema.xml \
	org.freedesktop.Tracker.DB.gschema.xml \
	org.freedesktop.Tracker.FTS.gschema.xml \
	org.freedesktop.Tracker.Store.gschema.xml \
	org.freedesktop.Tracker.Extract.gschema.xml \
	org.freedesktop.Tracker.Writeback.gschema.xml

miner_fs_schema = org.freedesktop.Tracker.Miner.Files.gschema.xml

all_schemas = $(base_schemas) $(miner_fs_schema)

gsettings_SCHEMAS = $(base_schemas)

if HAVE_TRACKER_MINER_FS
gsettings_SCHEMAS += $(miner_fs_schema)
endif

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

CLEANFILES = $(gsettings_SCHEMAS)

EXTRA_DIST = \
	$(all_schemas:.xml=.xml.in)
