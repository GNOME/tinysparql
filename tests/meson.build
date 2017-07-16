subdir('common')

subdir('gvdb')
subdir('libtracker-common')
subdir('libtracker-data')

if enable_fts
  subdir('libtracker-fts')
endif

subdir('libtracker-miner')
subdir('libtracker-sparql')
subdir('tracker-steroids')

if get_option('functional_tests')
  subdir('functional-tests')
endif
