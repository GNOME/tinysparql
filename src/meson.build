# Shared common code
subdir('libtracker-common')

# Intermediate library of RDF & SPARQL helper functions.
# This gets used internally and also becomes part of the
# public libtracker-sparql library.
subdir('libtracker-sparql')

# Internal database library
subdir('gvdb')
subdir('libtracker-fts')
subdir('libtracker-data')

# Public libtracker-sparql library
subdir('libtracker-bus')
subdir('libtracker-direct')
subdir('libtracker-remote')
subdir('libtracker-sparql-backend')

# Public libtracker-miner library
subdir('libtracker-miner')

# Internal data extraction helpers
subdir('libtracker-extract')

# Public data extract tool & modules
subdir('tracker-extract')

# Public miners
subdir('miners')

# Public metadata writeback tool & modiles
subdir('tracker-writeback')

# Internal control library
subdir('libtracker-control')

# Public commandline control tool
subdir('tracker')

# Public shared database
subdir('tracker-store')

# Example search application
subdir('tracker-needle')

# Example configuration application
subdir('tracker-preferences')

meson.add_install_script('create-miner-symlinks.sh', dbus_services_dir, tracker_miner_services_dir)
