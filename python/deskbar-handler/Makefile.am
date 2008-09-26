include $(top_srcdir)/Makefile.decl

if USING_DESKBAR_HANDLER
handlerdir = $(DESKBAR_HANDLER_DIR)
handler_DATA = 				\
	tracker-handler.py		\
	 tracker-handler-static.py
endif

if USING_DESKBAR_MODULE
moduledir = $(DESKBAR_MODULE_DIR)
module_DATA = tracker-module.py
endif

EXTRA_DIST = 				\
	tracker-handler.py 		\
	tracker-handler-static.py 	\
	tracker-module.py 		\
	README
