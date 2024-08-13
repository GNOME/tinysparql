# Copyright (C) 2024, Sam Thursfield <sam@afuera.me.uk>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

"""
Test HTTP endpoint
"""

import unittest

import configuration
import fixtures
import json
from pathlib import Path
import random
import shutil
from tempfile import mkdtemp
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

COMMAND_NAME = "tinysparql"

class TestEndpointHttp(fixtures.TrackerCommandLineTestCase):
    def setUp(self):
        super().setUp()

        self._dirpath = Path(mkdtemp())

        port = random.randint(32000, 65000)
        self.address = "http://127.0.0.1:%d/sparql/" % port

        # Create the database
        self.run_background(
            [
                COMMAND_NAME,
                "endpoint",
                "--database",
                self._dirpath,
                "--ontology",
                "nepomuk",
                "--http-port",
                port,
            ],
            "Listening",
        )

    def tearDown(self):
        super().tearDown()
        shutil.rmtree(self._dirpath)

    def test_http_query_cli(self):
        """Query the HTTP endpoint via `tracker3 sparql`"""

        stdout = self.run_cli(
            [
                COMMAND_NAME,
                "query",
                "--remote-service",
                self.address,
                "ASK { ?u a rdfs:Resource }",
            ]
        )
        self.assertIn("true", stdout)

    @staticmethod
    def example_ask_query() -> str:
        """Simple query that should always return 'true'."""
        return "ASK { ?u a rdfs:Resource }"

    def validate_ask_query_response(self, data):
        self.assertEqual(len(data["head"]["vars"]), 1);
        self.assertEqual(len(data["results"]["bindings"]), 1);

        var_name = data["head"]["vars"][0]
        row = data["results"]["bindings"][0]
        self.assertEqual(row[var_name]["type"], "literal")
        self.assertEqual(row[var_name]["value"], "true")

    def test_http_get_without_query(self):
        """Get the endpoint descriptor, returned when there's no query."""
        with urlopen(self.address) as response:
            data = response.read().decode()

        # Don't check the entire output, just make sure it's got some of the
        # expected info.
        self.assertIn("format:JSON-LD", data)

    def test_http_get_query(self):
        query = quote(self.example_ask_query())
        request = Request(f"{self.address}?query={query}")
        request.add_header("Accept", "application/sparql-results+json");
        with urlopen(request) as response:
            text = response.read().decode()

        data = json.loads(text)
        self.validate_ask_query_response(data)

    def test_http_post_query(self):
        query = "ASK { ?u a rdfs:Resource }"

        request = Request(self.address, data=query.encode())
        request.add_header("Accept", "application/sparql-results+json");
        with urlopen(request) as response:
            text = response.read().decode()

        data = json.loads(text)
        self.validate_ask_query_response(data)

    def test_missing_accept_header(self):
        """Ensure error code when there is no valid response format specified."""
        query = "ASK { ?u a rdfs:Resource }"

        request = Request(self.address, data=query.encode())
        request.add_header("Accept", "Invalid");
        request.add_header("Accept", r"Nonsense !\0");
        with self.assertRaises(HTTPError) as error_context:
            urlopen(request)

        error = error_context.exception
        self.assertEqual(error.code, 400);
        self.assertIn(error.msg, "No recognized accepted formats");

    def test_http_invalid_utf8(self):
        """Ensure error code when the query is not valid UTF-8"""
        query = "AB\xfc\0\0\0\xef"

        request = Request(self.address, data=query.encode())
        request.add_header("Accept", "application/sparql-results+json");
        with self.assertRaises(HTTPError) as error_context:
            urlopen(request)

        error = error_context.exception
        self.assertEqual(error.code, 400);
        self.assertIn("invalid UTF-8", error.msg);

    def test_http_invalid_sparql(self):
        """Ensure error code when the query is not valid SPARQL"""
        query = "Bananas in space"

        request = Request(self.address, data=query.encode())
        request.add_header("Accept", "application/sparql-results+json");
        with self.assertRaises(HTTPError) as error_context:
            urlopen(request)

        error = error_context.exception
        self.assertEqual(error.code, 400);
        self.assertIn("Parser error", error.msg);


if __name__ == "__main__":
    fixtures.tracker_test_main()
