include $(top_srcdir)/Makefile.decl

if ENABLE_SQLITE_FTS
build_sqlite_fts = tracker-fts
endif

SUBDIRS = common 			\
	libtracker-common		\
	libtracker-db 			\
	trackerd 			\
	scripts 			\
	$(build_sqlite_fts)		\
	tracker-indexer

