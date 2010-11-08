include $(top_srcdir)/Makefile.decl

officetestdir = $(datadir)/tracker-tests/test-extraction-data/office

# initialize variable for unconditional += appending
officetest_DATA =                                      

if HAVE_LIBGSF
officetest_DATA +=					\
	office-doc.doc                                 	\
	office-doc.expected                            	\
	powerpoint.ppt                                 	\
	powerpoint.expected
endif

if HAVE_POPPLER
officetest_DATA +=					\
	pdf-doc.pdf                                    	\
	pdf-doc.expected
endif

EXTRA_DIST = $(officetest_DATA)
