# Copyright (C) 2020, Sam Thursfield <sam@afuera.me.uk>
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
#

"""
Test `tracker` commandline tool
"""

import unittest

import configuration
import fixtures

COMMAND_NAME = "tinysparql"

class TestCli(fixtures.TrackerCommandLineTestCase):
    def test_version(self):
        """Check we're testing the correct version of the CLI"""
        output = self.run_cli([COMMAND_NAME, "--version"])

        version_line = output.splitlines()[0]
        expected_version_line = "TinySPARQL %s" % configuration.tracker_version()
        self.assertEqual(version_line, expected_version_line)

    def test_create_local_database(self):
        """Create a database using `tinysparql endpoint` for local testing"""

        with self.tmpdir() as tmpdir:
            # Create the database
            self.run_cli(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--database",
                    tmpdir,
                    "--ontology",
                    "nepomuk",
                ]
            )

            # Sanity check that it works.
            self.run_cli(
                [
                    COMMAND_NAME,
                    "query",
                    "--database",
                    tmpdir,
                    "ASK { ?u a rdfs:Resource }",
                ]
            )

    def test_export(self):
        """Export contents of a TinySPARQL database."""

        with self.tmpdir() as tmpdir:
            # Create a database and export it as Turtle.
            # We don't validate the output in this test, but we should.
            self.run_cli(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--database",
                    tmpdir,
                    "--ontology",
                    "nepomuk",
                ]
            )
            self.run_cli([COMMAND_NAME, "export", "--database", tmpdir])
            self.run_cli([COMMAND_NAME, "export", "--database", tmpdir, "--show-graphs"])

    def test_import(self):
        """Import a Turtle file into a TinySPARQL database."""

        testdata = str(self.data_path("serialized/test-movie.ttl"))

        with self.tmpdir() as tmpdir:
            self.run_cli(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--database",
                    tmpdir,
                    "--ontology",
                    "nepomuk",
                ]
            )
            self.run_cli([COMMAND_NAME, "import", "--database", tmpdir, testdata])


if __name__ == "__main__":
    fixtures.tracker_test_main()
