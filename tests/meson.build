subdir('common')

subdir('gvdb')
subdir('libtracker-common')
subdir('libtracker-data')

if enable_extract
  subdir('libtracker-extract')
endif

if enable_fts
  subdir('libtracker-fts')
endif

subdir('libtracker-miner')
subdir('libtracker-sparql')
subdir('tracker-steroids')

# The test case for writeback doesn't seem to work.
#if enable_writeback
#  subdir('tracker-writeback')
#endif

if get_option('functional_tests')
  subdir('functional-tests')
endif
