#include <glib.h>
#include <qname.h>


static void
test_qname_to_shortname (void) 
{
        gchar *result = NULL;

        result = qname_to_shortname ("http://purl.org/dc/elements/1.1/source");
        g_assert_cmpstr (result, ==, "dc:source");
        g_free (result);

        result = qname_to_shortname ("http://www.semanticdesktop.org/ontologies/2007/01/19/nie#InformationElement");
        g_assert_cmpstr (result, ==, "nie:InformationElement");
        g_free (result);

        result = qname_to_shortname ("test://local_uri#Class");
        g_assert_cmpstr (result, ==, "local:Class");
        g_free (result);

        result = qname_to_shortname ("test://doesnt_exists#Class");
        g_assert_cmpstr (result, ==, "test://doesnt_exists#Class");
        g_free (result);
}

static void
test_qname_to_classname (void) 
{
        gchar *result = NULL;

        result = qname_to_classname ("http://purl.org/dc/elements/1.1/source");
        g_assert_cmpstr (result, ==, "source");
        g_free (result);

        result = qname_to_classname ("http://www.semanticdesktop.org/ontologies/2007/01/19/nie#InformationElement");
        g_assert_cmpstr (result, ==, "InformationElement");
        g_free (result);

        result = qname_to_classname ("test://local_uri#Class");
        g_assert_cmpstr (result, ==, "Class");
        g_free (result);

        result = qname_to_classname ("test://doesnt_exists#Class");
        g_assert_cmpstr (result, ==, "test://doesnt_exists#Class");
        g_free (result);
}

static void
test_qname_to_link (void)
{
        gchar *result = NULL;

        result = qname_to_link ("test://local_uri#Class");
        g_assert_cmpstr (result, ==, "#Class");
        g_free (result);

        result = qname_to_link ("http://www.semanticdesktop.org/ontologies/2007/01/19/nie#InformationElement");
        g_assert_cmpstr (result, ==, "../nie/index.html#InformationElement");
        g_free (result);

        /* This is one of the special cases, where the prefix of the class
         * doesn't match with the prefix of the file where it is defined 
         */
        result = qname_to_link ("http://www.tracker-project.org/ontologies/tracker#Namespace");
        g_assert_cmpstr (result, ==, "../rdf/index.html#Namespace");
        g_free (result);
                                
}

int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        qname_init ("test://local_uri#", "local", "./file-class.cache.test");

	g_test_add_func ("/html_generator/qname/qname_to_shortname",
			 test_qname_to_shortname);

	g_test_add_func ("/html_generator/qname/qname_to_classname",
			 test_qname_to_classname);

	g_test_add_func ("/html_generator/qname/qname_to_link",
			 test_qname_to_link);

	result = g_test_run ();

        qname_shutdown ();

	return result;
}



