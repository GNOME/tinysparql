
if USING_INTERNAL_LIBEXTRACTOR
extractor_dir = libextractor
else
extractor_dir =
endif

if HAVE_GNOME
tracker_gui_dir = tracker-search-tool
else
tracker_gui_dir =
endif

SUBDIRS = text-filters trackerd libtracker $(extractor_dir) tracker-extract $(tracker_gui_dir)
DIST_SUBDIRS = text-filters trackerd libtracker $(extractor_dir) tracker-extract $(tracker_gui_dir)
