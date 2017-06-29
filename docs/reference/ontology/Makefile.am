AUTOMAKE_OPTIONS = 1.6

# The original Ontology explanation files
ONTOLOGY_INTRODUCTIONS =     \
	mfo-introduction.xml \
	mlo-introduction.xml \
	nco-introduction.xml \
	nie-introduction.xml \
	nmm-introduction.xml \
	nmo-introduction.xml \
	xsd-introduction.xml

# Generation of the ontology XML files.
gen-doc.stamp: $(ONTOLOGY_INTRODUCTIONS)
	$(MKDIR_P) xml
	$(top_builddir)/docs/tools/ttl2sgml -d $(top_srcdir)/src/ontologies/nepomuk -o xml/
	$(AM_V_GEN) touch $@

version.xml: gen-doc.stamp

# The name of the module.
DOC_MODULE = ontology

# The top-level SGML file.
DOC_MAIN_SGML_FILE = ontology-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = .

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =                                        \
	version.xml

expand_content_files =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# Note that HTML_FILES and content_files are already included
#  by gtk-doc
EXTRA_DIST +=                                          \
	version.xml.in                                 \
	$(ONTOLOGY_INTRODUCTIONS)                      \
	meson.build

CLEANFILES +=                                          \
	gen-doc.stamp
