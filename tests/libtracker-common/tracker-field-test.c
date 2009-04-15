#include <glib.h>
#include <libtracker-common/tracker-field.h>
#include <tracker-test-helpers.h>

static void
test_type_to_string ()
{
	const gchar *result;
	TrackerPropertyType type;

	type = TRACKER_PROPERTY_TYPE_KEYWORD;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "keyword"));

	type = TRACKER_PROPERTY_TYPE_INDEX;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "index"));

	type = TRACKER_PROPERTY_TYPE_FULLTEXT;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "fulltext"));

	type = TRACKER_PROPERTY_TYPE_STRING;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "string"));

	type = TRACKER_PROPERTY_TYPE_INTEGER;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "integer"));

	type = TRACKER_PROPERTY_TYPE_DOUBLE;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "double"));

	type = TRACKER_PROPERTY_TYPE_DATE;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "date"));

	type =	TRACKER_PROPERTY_TYPE_BLOB;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "blob"));

	type =	TRACKER_PROPERTY_TYPE_STRUCT;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "struct"));

	type =	TRACKER_PROPERTY_TYPE_RESOURCE;
	result = tracker_property_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "link"));

}

int
main (int argc, char **argv) {

	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* Something is not initialized without these lines */
	TrackerProperty *field = tracker_property_new ();
	g_object_unref (field);

	/* Init */

	g_test_add_func ("/libtracker-common/tracker-field/type_to_string",
			 test_type_to_string);

	result = g_test_run ();

	/* End */

	return result;
}
