include $(top_srcdir)/Makefile.decl

if HAVE_DESKBAR_APPLET_HANDLER
handlerdir = $(DESKBAR_APPLET_DIR)
handler_DATA = 				\
	tracker-handler.py		\
	tracker-handler-static.py
endif

if HAVE_DESKBAR_APPLET_MODULE
moduledir = $(DESKBAR_APPLET_DIR)
module_DATA = tracker-module.py
endif

EXTRA_DIST = 				\
	tracker-handler.py 		\
	tracker-handler-static.py 	\
	tracker-module.py
