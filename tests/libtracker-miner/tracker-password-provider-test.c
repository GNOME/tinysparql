/*
 * Copyright (C) 2010, Adrien Bustany (abustany@gnome.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>

#include <libtracker-miner/tracker-password-provider.h>

#define SERVICE_NAME  "TestService"
#define TEST_USERNAME "test-user"
#define TEST_PASSWORD "s3cr3t"

static TrackerPasswordProvider *provider;

static void
test_password_provider_setting (void)
{
	GError *error = NULL;
	gboolean success;

	g_print ("Storing password '%s' for user '%s'\n",
	         TEST_PASSWORD,
	         TEST_USERNAME);

	success = tracker_password_provider_store_password (provider,
	                                                    SERVICE_NAME,
	                                                    "This is the test service",
	                                                    TEST_USERNAME,
	                                                    TEST_PASSWORD,
	                                                    &error);

	g_assert_cmpint (success, ==, TRUE);
}

static void
test_password_provider_getting (void)
{
	gchar *username = NULL;
	gchar *password = NULL;
	GError *error = NULL;

	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   &username,
	                                                   &error);

	g_assert_cmpstr (username, ==, TEST_USERNAME);
	g_assert_cmpstr (password, ==, TEST_PASSWORD);

	g_print ("Found password is '%s' for username '%s'\n", 
	         password,
	         username);

	g_free (username);
	g_free (password);

	/* Also test without getting the username */
	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   NULL,
	                                                   &error);

	g_assert_cmpstr (password, ==, TEST_PASSWORD);

	g_print ("Found password is '%s' for NULL username\n", password);

	g_free (password);
}

int 
main (int argc, char **argv)
{
	g_type_init ();

	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);

	g_test_message ("Testing password provider");

	g_test_add_func ("/libtracker-miner/tracker-password-provider/setting",
	                 test_password_provider_setting);
	g_test_add_func ("/libtracker-miner/tracker-password-provider/getting",
	                 test_password_provider_getting);

	provider = tracker_password_provider_get ();
	g_assert (provider);

	/* g_object_unref (provider); */

	return g_test_run ();
}
