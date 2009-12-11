#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gprintf.h>
#include <libtracker-common/tracker-common.h>
#include <libtracker-data/tracker-sparql-query.h>

#define RDF_PREFIX TRACKER_RDF_PREFIX
#define RDF_PROPERTY RDF_PREFIX "Property"
#define RDF_TYPE RDF_PREFIX "type"

#define RDFS_PREFIX TRACKER_RDFS_PREFIX
#define RDFS_CLASS RDFS_PREFIX "Class"
#define RDFS_DOMAIN RDFS_PREFIX "domain"
#define RDFS_RANGE RDFS_PREFIX "range"
#define RDFS_SUB_CLASS_OF RDFS_PREFIX "subClassOf"
#define RDFS_SUB_PROPERTY_OF RDFS_PREFIX "subPropertyOf"

#define NRL_PREFIX TRACKER_NRL_PREFIX
#define NRL_INVERSE_FUNCTIONAL_PROPERTY TRACKER_NRL_PREFIX "InverseFunctionalProperty"
#define NRL_MAX_CARDINALITY NRL_PREFIX "maxCardinality"

#define TRACKER_PREFIX TRACKER_TRACKER_PREFIX

static gchar *ontology_dir = NULL;
static gchar *output_file = NULL;

static gchar *colors[] = {
        "#dd0000",
        "#00dd00",
        "#0000dd",
        "#dd00dd",
        "#dddd00",
        "#00dddd",
        "#dddddd"
        "#bb0000",
        "#00bb00",
        "#0000bb",
        "#bb00bb",
        "#bbbb00",
        "#00bbbb",
        "#bbbbbb"
};

static GOptionEntry   entries[] = {
	{ "ontology-dir", 'd', 0, G_OPTION_ARG_FILENAME, &ontology_dir,
	  "Ontology directory",
	  NULL
	},
	{ "output", 'o', 0, G_OPTION_ARG_FILENAME, &output_file,
	  "File to write the output (default stdout)",
	  NULL
	},
	{ NULL }
};

/* Stripped from tracker-data-manager.h */
static void
load_ontology_file_from_path (const gchar	 *ontology_file)
{
	TrackerTurtleReader *reader;
	GError              *error = NULL;

	reader = tracker_turtle_reader_new (ontology_file, &error);
	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
		return;
	}

	while (error == NULL && tracker_turtle_reader_next (reader, &error)) {
		const gchar *subject, *predicate, *object;

		subject = tracker_turtle_reader_get_subject (reader);
		predicate = tracker_turtle_reader_get_predicate (reader);
		object = tracker_turtle_reader_get_object (reader);

		if (g_strcmp0 (predicate, RDF_TYPE) == 0) {
			if (g_strcmp0 (object, RDFS_CLASS) == 0) {
				TrackerClass *class;

				if (tracker_ontology_get_class_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of class %s", ontology_file, subject);
					continue;
				}

				class = tracker_class_new ();
				tracker_class_set_uri (class, subject);
				tracker_ontology_add_class (class);
				g_object_unref (class);
			} else if (g_strcmp0 (object, RDF_PROPERTY) == 0) {
				TrackerProperty *property;

				if (tracker_ontology_get_property_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of property %s", ontology_file, subject);
					continue;
				}

				property = tracker_property_new ();
				tracker_property_set_uri (property, subject);
				tracker_ontology_add_property (property);
				g_object_unref (property);
			} else if (g_strcmp0 (object, NRL_INVERSE_FUNCTIONAL_PROPERTY) == 0) {
				TrackerProperty *property;

				property = tracker_ontology_get_property_by_uri (subject);
				if (property == NULL) {
					g_critical ("%s: Unknown property %s", ontology_file, subject);
					continue;
				}

				tracker_property_set_is_inverse_functional_property (property, TRUE);
			} else if (g_strcmp0 (object, TRACKER_PREFIX "Namespace") == 0) {
				TrackerNamespace *namespace;

				if (tracker_ontology_get_namespace_by_uri (subject) != NULL) {
					g_critical ("%s: Duplicate definition of namespace %s", ontology_file, subject);
					continue;
				}

				namespace = tracker_namespace_new ();
				tracker_namespace_set_uri (namespace, subject);
				tracker_ontology_add_namespace (namespace);
				g_object_unref (namespace);
			}
		} else if (g_strcmp0 (predicate, RDFS_SUB_CLASS_OF) == 0) {
			TrackerClass *class, *super_class;

			class = tracker_ontology_get_class_by_uri (subject);
			if (class == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, subject);
				continue;
			}

			super_class = tracker_ontology_get_class_by_uri (object);
			if (super_class == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_class_add_super_class (class, super_class);
		} else if (g_strcmp0 (predicate, RDFS_SUB_PROPERTY_OF) == 0) {
			TrackerProperty *property, *super_property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			super_property = tracker_ontology_get_property_by_uri (object);
			if (super_property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, object);
				continue;
			}

			tracker_property_add_super_property (property, super_property);
		} else if (g_strcmp0 (predicate, RDFS_DOMAIN) == 0) {
			TrackerProperty *property;
			TrackerClass *domain;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			domain = tracker_ontology_get_class_by_uri (object);
			if (domain == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_property_set_domain (property, domain);
		} else if (g_strcmp0 (predicate, RDFS_RANGE) == 0) {
			TrackerProperty *property;
			TrackerClass *range;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			range = tracker_ontology_get_class_by_uri (object);
			if (range == NULL) {
				g_critical ("%s: Unknown class %s", ontology_file, object);
				continue;
			}

			tracker_property_set_range (property, range);
		} else if (g_strcmp0 (predicate, NRL_MAX_CARDINALITY) == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (atoi (object) == 1) {
				tracker_property_set_multiple_values (property, FALSE);
			}
		} else if (g_strcmp0 (predicate, TRACKER_PREFIX "indexed") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (strcmp (object, "true") == 0) {
				tracker_property_set_indexed (property, TRUE);
			}
		} else if (g_strcmp0 (predicate, TRACKER_PREFIX "transient") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (g_strcmp0 (object, "true") == 0) {
				tracker_property_set_transient (property, TRUE);
			}
		} else if (g_strcmp0 (predicate, TRACKER_PREFIX "isAnnotation") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (g_strcmp0 (object, "true") == 0) {
				tracker_property_set_embedded (property, FALSE);
			}
		} else if (g_strcmp0 (predicate, TRACKER_PREFIX "fulltextIndexed") == 0) {
			TrackerProperty *property;

			property = tracker_ontology_get_property_by_uri (subject);
			if (property == NULL) {
				g_critical ("%s: Unknown property %s", ontology_file, subject);
				continue;
			}

			if (strcmp (object, "true") == 0) {
				tracker_property_set_fulltext_indexed (property, TRUE);
			}
		} else if (g_strcmp0 (predicate, TRACKER_PREFIX "prefix") == 0) {
			TrackerNamespace *namespace;

			namespace = tracker_ontology_get_namespace_by_uri (subject);
			if (namespace == NULL) {
				g_critical ("%s: Unknown namespace %s", ontology_file, subject);
				continue;
			}

			tracker_namespace_set_prefix (namespace, object);
		}
	}

	g_object_unref (reader);

	if (error) {
		g_critical ("Turtle parse error: %s", error->message);
		g_error_free (error);
	}
}

static void
load_ontology_file (GFile *file)
{
        gchar *path;

        path = g_file_get_path (file);
        load_ontology_file_from_path (path);
        g_free (path);
}

static gboolean
load_ontology_dir (GFile *dir)
{
        GFileEnumerator *enumerator;
        GFileInfo *info;
        GList *files, *f;
        const gchar *name;

        enumerator = g_file_enumerate_children (dir,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, NULL);

        if (!enumerator) {
                return FALSE;
        }

        while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
                name = g_file_info_get_name (info);

                if (g_str_has_suffix (name, ".ontology")) {
                        files = g_list_insert_sorted (files, g_strdup (name),
                                                      (GCompareFunc) g_strcmp0);
                }

                g_object_unref (info);
        }

        g_object_unref (enumerator);

        for (f = files; f; f = f->next) {
                GFile *child;

                child = g_file_get_child (dir, f->data);
                load_ontology_file (child);
                g_object_unref (child);
        }

        g_list_foreach (files, (GFunc) g_free, NULL);
        g_list_free (files);

        return TRUE;
}

static const gchar *
snip_name (const gchar *name)
{
        const gchar *subname;

        subname = strchr (name, ':');
        subname++;

        return subname;
}

static gchar *
get_prefix (const gchar *name)
{
        const gchar *str;
        gchar *prefix;

        str = strchr (name, ':');
        prefix = g_strndup (name, str - name);

        return prefix;
}

static void
generate_class_info (FILE *f)
{
        TrackerClass **classes;
        GHashTable *info;
        guint length, i, j;
        GHashTableIter iter;
        gpointer key, value;
        gint cur_color = 0;

        info = g_hash_table_new_full (g_str_hash,
                                      g_str_equal,
                                      NULL, NULL);

        classes = tracker_ontology_get_classes (&length);

        g_fprintf (f, "graph G {\n");

        g_fprintf (f, "size=\"22,22\";\n"
                   "orientation=\"portrait\";\n"
                   "shape=\"record\";\n"
                   "ratio=\"1.0\";\n"
                   "concentrate=\"true\";\n"
                   "compound=\"true\";\n"
                   "dim=\"10\";\n"
                   "rankdir=\"LR\";\n");

        for (i = 0; i < length; i++) {
                const gchar *name;
                TrackerClass **superclasses;
                gchar *prefix;
                GList *subgraph_elements;

                name = snip_name (tracker_class_get_name (classes[i]));
                prefix = get_prefix (tracker_class_get_name (classes[i]));
                superclasses = tracker_class_get_super_classes (classes[i]);

                subgraph_elements = g_hash_table_lookup (info, prefix);
                subgraph_elements = g_list_prepend (subgraph_elements, (gpointer) name);
                g_hash_table_replace (info, prefix, subgraph_elements);

                g_fprintf (f, "%s_%s [ label=\"%s:%s\", style=\"filled\" ]\n", prefix, name, prefix, name);

                for (j = 0; superclasses[j]; j++) {
                        const gchar *super_name;
                        gchar *super_prefix;

                        super_name = snip_name (tracker_class_get_name (superclasses[j]));
                        super_prefix = get_prefix (tracker_class_get_name (superclasses[j]));

                        g_fprintf (f, "%s_%s -- %s_%s [ dir=\"forward\" ", prefix, name, super_prefix, super_name);

                        if (g_strcmp0 (prefix, super_prefix) != 0) {
                                g_fprintf (f, ", ltail=cluster_%s, lhead=cluster_%s ", prefix, super_prefix);
                        }

                        g_fprintf (f, "] ;\n");

                        g_free (super_prefix);
                }
        }

        g_hash_table_iter_init (&iter, info);

        while (g_hash_table_iter_next (&iter, &key, &value)) {
                gchar *prefix = key;
                GList *subgraph_elements = value;

                g_fprintf (f, "subgraph \"cluster_%s\" { label=\"%s ontology\"; fontsize=30 ; bgcolor=\"%s\"; ", prefix, prefix, colors[cur_color]);

                while (subgraph_elements) {
                        g_fprintf (f, "%s_%s; ", prefix, (gchar *) subgraph_elements->data);
                        subgraph_elements = subgraph_elements->next;
                }

                g_fprintf (f, "}\n");

                cur_color++;

                if (cur_color >= G_N_ELEMENTS (colors)) {
                        cur_color = 0;
                }
        }

        g_hash_table_destroy (info);

        g_fprintf (f, "}\n");
}

int
main (int argc, char *argv[])
{
        GOptionContext *context;
        FILE *f = NULL;
        GFile *dir;

        g_type_init ();

	/* Translators: this messagge will apper immediately after the	*/
	/* usage string - Usage: COMMAND [OPTION]... <THIS_MESSAGE>	*/
	context = g_option_context_new ("- Generates graphviz for a TTL file");

	/* Translators: this message will appear after the usage string */
	/* and before the list of options.				*/
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

        if (!ontology_dir) {
		gchar *help;

		g_printerr ("%s\n\n",
			    "Ontology dir is mandatory");

		help = g_option_context_get_help (context, TRUE, NULL);
		g_option_context_free (context);
		g_printerr ("%s", help);
		g_free (help);

		return -1;
        }

        if (output_file) {
                f = fopen (output_file, "w");
        } else {
                f = stdout;
        }
        g_assert (f != NULL);

        tracker_ontology_init ();

        dir = g_file_new_for_commandline_arg (ontology_dir);
        load_ontology_dir (dir);
        g_object_unref (dir);

        generate_class_info (f);

        tracker_ontology_shutdown ();

        return 0;
}
