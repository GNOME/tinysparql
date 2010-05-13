include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# Generation of ontology.png using ontology-graph
ontology.png:
	$(top_builddir)/docs/tools/ontology-graph 			\
		-d $(top_srcdir)/data/ontologies 			\
		-o ontology.dot
	$(AM_V_GEN) $(GRAPHVIZ_FDP) -Tpng -o $@ ontology.dot
	rm ontology.dot

# Generation of the additional PNG files from DIA files
#  (list of original '.dia' files with '.png' extension)
ONTOLOGY_DIAGRAMS =							\
	$(top_srcdir)/docs/ontologies/notation.dia			\
	$(top_srcdir)/docs/ontologies/mfo/feeds-overview.dia		\
	$(top_srcdir)/docs/ontologies/nmm/radio-overview.dia		\
	$(top_srcdir)/docs/ontologies/nmm/images-overview.dia		\
	$(top_srcdir)/docs/ontologies/mlo/location-overview.dia		\
	$(top_srcdir)/docs/ontologies/nie/overview.dia			\
	$(top_srcdir)/docs/ontologies/nmo/message-class-overview.dia	\
	$(top_srcdir)/docs/ontologies/nmo/email-mimeparts-overview.dia

# The wanted PNG files
ONTOLOGY_DIAGRAMS_PNG = $(notdir $(ONTOLOGY_DIAGRAMS:.dia=.png))

# DIA to PNG conversion
$(ONTOLOGY_DIAGRAMS_PNG): $(ONTOLOGY_DIAGRAMS)
	dia -t png $^ 2>/dev/null

# The original Ontology explanation files
ONTOLOGY_EXPLANATIONS = 						\
	$(top_srcdir)/docs/ontologies/mfo/explanation.xml		\
	$(top_srcdir)/docs/ontologies/nmm/explanation.xml		\
	$(top_srcdir)/docs/ontologies/mlo/explanation.xml		\
	$(top_srcdir)/docs/ontologies/nie/explanation.xml		\
	$(top_srcdir)/docs/ontologies/nmm/explanation.xml		\
	$(top_srcdir)/docs/ontologies/nmo/explanation.xml

# The XMLs to generate from the Ontology info
ONTOLOGY_INFOS_XML =							\
	fts-properties.xml						\
	nie-ontology.xml						\
	nco-ontology.xml						\
	nmo-ontology.xml						\
	nfo-ontology.xml						\
	nao-ontology.xml						\
	mto-ontology.xml						\
	nmm-ontology.xml						\
	mfo-ontology.xml						\
	mlo-ontology.xml						\
	tracker-ontology.xml						\
	maemo-ontology.xml						\
	ncal-ontology.xml						\
	scal-ontology.xml						\
	xsd-ontology.xml

# Generation of the ontology XML files.
gen-doc.stamp: $(ONTOLOGY_EXPLANATIONS)
	$(top_srcdir)/docs/tools/gen-doc.sh 				\
		$(top_builddir)/docs/tools/ttl2sgml			\
		$(top_srcdir)/data/ontologies 				\
		$(top_srcdir)/docs/ontologies 				\
			.
	$(AM_V_GEN) touch $@

# Make the final XML files depend on the stamp
$(ONTOLOGY_INFOS_XML): gen-doc.stamp

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

# Images to copy into HTML directory
HTML_IMAGES = 								\
	$(ONTOLOGY_DIAGRAMS_PNG)					\
	ontology.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
#  Note that PNG files are also added in content_files so that
#  the documentation is not built before the PNGs.
content_files =								\
	$(ONTOLOGY_INFOS_XML) 						\
	$(HTML_IMAGES)							\
	version.xml

expand_content_files =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# Note that HTML_FILES and content_files are already included
#  by gtk-doc
EXTRA_DIST += 								\
	gen-doc.stamp							\
	version.xml.in							\
	$(ONTOLOGY_DIAGRAMS)						\
	$(ONTOLOGY_EXPLANATIONS)

# PNGs and XMLs generated are removed only in maintainer-clean
MAINTAINERCLEANFILES = 							\
	$(HTML_IMAGES)							\
	$(ONTOLOGY_INFOS_XML) 						\
	gen-doc.stamp