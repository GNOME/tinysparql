include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE = ontology

# The top-level SGML file.
DOC_MAIN_SGML_FILE = ontology-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = .

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =						\
	fts-properties.xml				\
	nie-ontology.xml				\
	nco-ontology.xml				\
	nmo-ontology.xml				\
	nfo-ontology.xml				\
	nao-ontology.xml				\
	mto-ontology.xml				\
	nmm-ontology.xml				\
	mfo-ontology.xml				\
	mlo-ontology.xml				\
	tracker-ontology.xml				\
	maemo-ontology.xml				\
	ncal-ontology.xml				\
	scal-ontology.xml				\
	xsd-ontology.xml				\
	version.xml

expand_content_files =

ONTOLOGY_DIAGRAMS = 					\
	notation.png					\
	feeds-overview.png				\
	radio-overview.png				\
	images-overview.png				\
	location-overview.png				\
	overview.png					\
	message-class-overview.png			\
	email-mimeparts-overview.png

# Images to copy into HTML directory
HTML_IMAGES = ontology.png notation.png $(ONTOLOGY_DIAGRAMS)

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in $(HTML_IMAGES)
