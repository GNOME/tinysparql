docs = \
	office-doc.doc \
	office-doc.expected \
	powerpoint.ppt \
	powerpoint.expected

pdfs = \
	pdf-doc.pdf \
	pdf-doc.expected

officetestdir = $(datadir)/tracker-tests/test-extraction-data/office

# initialize variable for unconditional += appending
officetest_DATA =

if HAVE_LIBGSF
officetest_DATA += $(docs)
endif

if HAVE_POPPLER
officetest_DATA += $(pdfs)
endif

EXTRA_DIST = \
	$(docs) \
	$(pdfs)
