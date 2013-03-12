#include <glib.h>
#include "gvdb/gvdb-builder.h"
#include "gvdb/gvdb-reader.h"

static void
remove_file (const gchar *filename)
{
        GFile *f;
        f = g_file_new_for_path (filename);
        g_assert (g_file_delete (f, NULL, NULL));
}

static void
walk_value_cb (G_GNUC_UNUSED const gchar *name,
               G_GNUC_UNUSED gsize name_len,
               G_GNUC_UNUSED GVariant *value,
               gpointer user_data)
{
        gint *counter = (gint *)user_data;
        (*counter) += 1;
}

static gboolean
walk_open_cb (G_GNUC_UNUSED const gchar *name,
              G_GNUC_UNUSED gsize name_len,
              G_GNUC_UNUSED gpointer user_data)
{
        return TRUE;
}

static void
walk_close_cb (G_GNUC_UNUSED gsize name_len,
               G_GNUC_UNUSED gpointer user_data)
{
}

static void
test_gvdb_walk (void)
{
        GHashTable *root_table, *ns_table;
        GvdbItem   *root, *item;
        GvdbTable  *root_level, *ns_level;
        guint32     item_id;
        gchar      *key;
        gint        counter = 0;

        const gchar *DB_FILE = "./test_walk.gvdb";

        root_table = gvdb_hash_table_new (NULL, NULL);

        ns_table = gvdb_hash_table_new (root_table, "namespaces");
        root = gvdb_hash_table_insert (ns_table, "");
        for (item_id = 0; item_id < 3; item_id++) {
                key = g_strdup_printf ("ns%d", item_id);
                item = gvdb_hash_table_insert (ns_table, key);
                gvdb_item_set_parent (item, root);
                gvdb_item_set_value (item, g_variant_new_string ("http://some.cool.ns"));
                g_free (key);
        }

        g_assert (gvdb_table_write_contents (root_table, DB_FILE, FALSE, NULL));

        g_hash_table_unref (ns_table);
        g_hash_table_unref (root_table);


        root_level = gvdb_table_new (DB_FILE, TRUE, NULL);
        ns_level = gvdb_table_get_table (root_level, "namespaces");

        gvdb_table_walk (ns_level, "", walk_open_cb, walk_value_cb, walk_close_cb, &counter);
        g_assert_cmpint (counter, ==, 3);

        gvdb_table_unref (root_level);
        gvdb_table_unref (ns_level);

        remove_file (DB_FILE);
}

static void
test_gvdb_nested_keys (void)
{
        GHashTable *root_table, *ns_table;
        GvdbItem   *root, *item;
        GvdbTable  *root_level, *ns_level;
        gchar     **keys;
        GVariant   *value;
        guint32     item_id;
        gchar      *key;

        const gchar *DB_FILE = "./test_nested_keys.gvdb";

        root_table = gvdb_hash_table_new (NULL, NULL);

        ns_table = gvdb_hash_table_new (root_table, "namespaces");
        root = gvdb_hash_table_insert (ns_table, "");
        for (item_id = 0; item_id < 3; item_id++) {
                key = g_strdup_printf ("ns%d", item_id);
                item = gvdb_hash_table_insert (ns_table, key);
                gvdb_item_set_parent (item, root);
                gvdb_item_set_value (item, g_variant_new_string ("http://some.cool.ns"));
                g_free (key);
        }

        g_assert (gvdb_table_write_contents (root_table, DB_FILE, FALSE, NULL));

        g_hash_table_unref (ns_table);
        g_hash_table_unref (root_table);

        root_level = gvdb_table_new (DB_FILE, TRUE, NULL);
        g_assert (root_level);

        ns_level = gvdb_table_get_table (root_level, "namespaces");
        g_assert (ns_level);
        
        keys = gvdb_table_list (ns_level, "");
        g_assert (keys);
        g_assert_cmpint (g_strv_length (keys), ==, 3);
        for (item_id = 0; item_id < 3; item_id++) {
                key = g_strdup_printf ("ns%d", item_id);
                g_assert (gvdb_table_has_value (ns_level, key));
                value = gvdb_table_get_raw_value (ns_level, key);
                g_assert_cmpstr (g_variant_get_string (value, NULL), ==, "http://some.cool.ns");
                g_free (key);
        }
        g_strfreev (keys);
        
        gvdb_table_unref (root_level);
        gvdb_table_unref (ns_level);
        remove_file (DB_FILE);
}

static void
simple_test (const gchar *filename, gboolean use_byteswap) 
{
        GHashTable *table;
        GvdbTable  *read;
        GVariant   *value;
        GVariant   *expected;

        table = gvdb_hash_table_new (NULL, "level1");
        gvdb_hash_table_insert_string (table, "key1", "here just a flat string");
        g_assert (gvdb_table_write_contents (table, filename, use_byteswap, NULL));
        g_hash_table_unref (table);

        read = gvdb_table_new (filename, TRUE, NULL);
        g_assert (read);
        g_assert (gvdb_table_is_valid (read));

        g_assert (gvdb_table_has_value (read, "key1"));
        value = gvdb_table_get_value (read, "key1");
        expected = g_variant_new_string ("here just a flat string");
        g_assert (g_variant_equal (value, expected));

        g_variant_unref (expected);
        g_variant_unref (value);

        gvdb_table_unref (read);
}

static void
test_gvdb_byteswapped (void)
{
        const gchar *DB_FILE = "./test_byteswpped.gvdb";

        simple_test (DB_FILE, TRUE);

        remove_file (DB_FILE);
}

static void
test_gvdb_flat_strings (void)
{

        const gchar *DB_FILE = "./test_flat_strings.gvdb";
        
        simple_test (DB_FILE, FALSE);

        remove_file (DB_FILE);
}

static void
test_gvdb_ref_unref (void)
{
        GHashTable *table;
        GvdbTable  *read, *read_ref;
        GVariant   *value;
        GVariant   *expected;

        const gchar *DB_FILE = "./test_ref_unref.gvdb";

        /* Create a table */
        table = gvdb_hash_table_new (NULL, "level1");
        gvdb_hash_table_insert_string (table, "key1", "whatever");
        g_assert (gvdb_table_write_contents (table, DB_FILE, FALSE, NULL));
        g_hash_table_unref (table);

        /* Read the table */
        read = gvdb_table_new (DB_FILE, TRUE, NULL);
        g_assert (read && gvdb_table_is_valid (read));

        /* Run the checks with a reference */
        read_ref = gvdb_table_ref (read);
        g_assert (gvdb_table_has_value (read_ref, "key1"));
        value = gvdb_table_get_value (read_ref, "key1");
        expected = g_variant_new_string ("whatever");
        g_assert (g_variant_equal (value, expected));

        g_variant_unref (expected);
        g_variant_unref (value);

        gvdb_table_unref (read);
        gvdb_table_unref (read_ref);

        remove_file (DB_FILE);
}

static void
test_gvdb_corrupted_file (void)
{
        GError *error = NULL;

        g_file_set_contents ("./test_invalid.gvdb", 
                             "Just a bunch of rubbish to fill a text file and try to open it"
                             "as a gvdb and check the error is correctly reported",
                             -1, NULL);

        gvdb_table_new ("./test_invalid.gvdb", TRUE, &error);
        g_assert (error);

        remove_file ("./test_invalid.gvdb");
}


gint
main (gint argc, gchar **argv) 
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/gvdb/ref_unref", test_gvdb_ref_unref);
        g_test_add_func ("/gvdb/flat_strings", test_gvdb_flat_strings);
        g_test_add_func ("/gvdb/nested_keys", test_gvdb_nested_keys);
        g_test_add_func ("/gvdb/walk", test_gvdb_walk);
        g_test_add_func ("/gvdb/byteswapped", test_gvdb_byteswapped);
        g_test_add_func ("/gvdb/corrupted_file", test_gvdb_corrupted_file);

        return g_test_run ();
}
