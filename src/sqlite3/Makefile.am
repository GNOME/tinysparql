if OS_WIN32
SQLITE_OS = -DOS_WIN=1
libsqlite_private_la_LIBADD = -lpthreadGC2
else
SQLITE_OS = -DOS_UNIX=1
libsqlite_private_la_LIBADD = -lpthread
endif

INCLUDES =				\
	 $(SQLITE_OS)			\
	 -DHAVE_USLEEP=1 		\
	 -DHAVE_FDATASYNC=1 		\
	 -DNDEBUG			\
	 -DTHREADSAFE=1			\
	 -DSQLITE_THREAD_OVERRIDE_LOCK=-1	\
	 -DSQLITE_OMIT_CURSOR 		


noinst_LTLIBRARIES=libsqlite-private.la


libsqlite_private_la_SOURCES = \
	sqlite3.h \
	sqlite3ext.h \
	alter.c \
	analyze.c \
	attach.c \
	auth.c \
	btree.c \
	btree.h \
	build.c \
	callback.c \
	complete.c \
	date.c \
	delete.c \
	expr.c \
	func.c \
	hash.c \
	hash.h \
	insert.c \
	keywordhash.h \
	legacy.c \
	loadext.c \
	main.c \
	opcodes.h \
	opcodes.c \
	os.c \
	os.h \
	os_common.h \
	os_unix.c \
	os_win.c \
	os_os2.c \
	pager.c \
	pager.h \
	parse.h \
	parse.c \
	pragma.c \
	prepare.c \
	printf.c \
	random.c \
	select.c \
	shell.c \
	sqliteInt.h \
	table.c \
	tokenize.c \
	trigger.c \
	utf.c \
	update.c \
	util.c \
	vacuum.c \
	vdbe.c \
	vdbe.h \
	vdbeapi.c \
	vdbeaux.c \
	vdbefifo.c \
	vdbemem.c \
	vdbeInt.h \
	vtab.c \
	where.c

