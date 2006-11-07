

if HAVE_GNOME
tracker_gui_dir = tracker-search-tool
else
tracker_gui_dir =
endif

if USING_EXTERNAL_SQLITE
sqlite_dir = 
else
sqlite_dir = sqlite3
endif


SUBDIRS = libstemmer $(sqlite_dir) text-filters trackerd libtracker tracker-extract $(tracker_gui_dir)
DIST_SUBDIRS = libstemmer $(sqlite_dir) text-filters trackerd libtracker tracker-extract $(tracker_gui_dir)
