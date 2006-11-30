

if HAVE_GNOME
tracker_gui_dir = tracker-search-tool
endif

if !USING_EXTERNAL_SQLITE
sqlite_dir = sqlite3
endif


SUBDIRS = libstemmer $(sqlite_dir) text-filters trackerd libtracker tracker-extract tracker-thumbnailer $(tracker_gui_dir)

