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

int
main (int argc, char **argv)
{
	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

        qname_init ("test://local_uri#", "local", NULL);

	g_test_add_func ("/html_generator/qname/qname_to_shortname",
			 test_qname_to_shortname);

	result = g_test_run ();

        qname_shutdown ();

	return result;
}



