if HAVE_GNOME
tracker_gui_dir = tracker-search-tool
endif

if !USING_EXTERNAL_SQLITE
sqlite_dir = sqlite3
endif

if !USING_EXTERNAL_QDBM
qdbm_dir = qdbm
endif

if ENABLE_PREFERENCES
tracker_preferences_dir = tracker-preferences
endif

if ENABLE_LIBTRACKERGTK
libtrackergtk_dir = libtracker-gtk
endif

SUBDIRS = libstemmer xdgmime $(sqlite_dir) $(qdbm_dir) text-filters trackerd libtracker tracker-extract tracker-thumbnailer $(libtrackergtk_dir) $(tracker_gui_dir) $(tracker_preferences_dir)

