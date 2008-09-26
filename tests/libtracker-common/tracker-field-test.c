#include <glib.h>
#include <glib/gtestutils.h>
#include <libtracker-common/tracker-field.h>
#include <tracker-test-helpers.h>

static void
test_type_to_string ()
{
	const gchar *result;
	TrackerFieldType type;

	type = TRACKER_FIELD_TYPE_KEYWORD;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "keyword"));

	type = TRACKER_FIELD_TYPE_INDEX;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "index"));

	type = TRACKER_FIELD_TYPE_FULLTEXT;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "fulltext"));

	type = TRACKER_FIELD_TYPE_STRING;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "string"));

	type = TRACKER_FIELD_TYPE_INTEGER;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "integer"));

	type = TRACKER_FIELD_TYPE_DOUBLE;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "double"));

	type = TRACKER_FIELD_TYPE_DATE;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "date"));

	type =	TRACKER_FIELD_TYPE_BLOB;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "blob"));

	type =	TRACKER_FIELD_TYPE_STRUCT;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "struct"));

	type =	TRACKER_FIELD_TYPE_LINK;
	result = tracker_field_type_to_string (type);
	g_assert (tracker_test_helpers_cmpstr_equal (result, "link"));

}

int
main (int argc, char **argv) {

	int result;

	g_type_init ();
	g_test_init (&argc, &argv, NULL);

	/* Something is not initialized without these lines */
	TrackerField *field = tracker_field_new ();
	g_object_unref (field);

	/* Init */

	g_test_add_func ("/libtracker-common/tracker-field/type_to_string",
			 test_type_to_string);

	result = g_test_run ();

	/* End */

	return result;
}
