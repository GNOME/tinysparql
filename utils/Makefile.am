include $(top_srcdir)/Makefile.decl

if ENABLE_SQLITE_FTS
build_sqlite_fts = tracker-fts
endif

if !ENABLE_SQLITE_FTS
build_qdbm = qdbm
endif

SUBDIRS = 			\
	services		\
	$(build_sqlite_fts)	\
	$(build_qdbm)

DIST_SUBDIRS = 			\
	services		\
	tracker-fts		\
	qdbm
