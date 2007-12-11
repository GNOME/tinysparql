
noinst_LTLIBRARIES = libxdgmime.la

if OS_WIN32
xdgmime_win_sources = fnmatch.c \
		   fnmatch.h \
		   safe-ctype.c \
		   safe-ctype.h
endif

libxdgmime_la_SOURCES = \
	$(xdgmime_win_sources)  \
	xdgmime.c 	\
	xdgmime.h	\
	xdgmimealias.c	\
	xdgmimealias.h	\
	xdgmimecache.c	\
	xdgmimecache.h	\
	xdgmimeglob.c 	\
	xdgmimeglob.h 	\
	xdgmimeint.c 	\
	xdgmimeint.h 	\
	xdgmimemagic.c  \
	xdgmimemagic.h	\
	xdgmimeparent.c	\
	xdgmimeparent.h
