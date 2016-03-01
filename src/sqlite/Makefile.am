AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-DSQLITE_THREADSAFE=1                          \
	-DSQLITE_ENABLE_FTS4                           \
	-DSQLITE_ENABLE_FTS3_TOKENIZER

noinst_LTLIBRARIES = libtracker-sqlite.la

libtracker_sqlite_la_SOURCES =                         \
	sqlite3.c                                      \
	sqlite3.h                                      \
	sqlite3ext.h

libtracker_sqlite_la_LIBADD =                          \
	$(BUILD_LIBS)
