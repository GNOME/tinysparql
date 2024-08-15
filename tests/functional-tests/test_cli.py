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

import random
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
            self.run_cli([COMMAND_NAME, "export", "--database", tmpdir, "rdfs:Class", "rdf:Property"])

            ex = None
            try:
                self.run_cli([COMMAND_NAME, "export", "--database", tmpdir, "--output", "banana"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Unsupported serialization format", str(ex), "Output not matched")

    def test_export_dbus(self):
        """Export contents of a D-Bus endpoint."""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.ExportOverDBus%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            self.run_cli([COMMAND_NAME, "export", "--output", "trig", "--dbus-service", bus_name])

    def test_export_http(self):
        """Export contents of a D-Bus endpoint."""

        with self.tmpdir() as tmpdir:
            port = random.randint(32000, 65000)
            address = "http://127.0.0.1:%d/sparql" % port

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--http-port", port,
                    "--loopback",
                ],
                "Listening",
            )

            self.run_cli([COMMAND_NAME, "export", "--output", "trig", "--remote-service", address])

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

    def test_import_dbus(self):
        """Import contents to a D-Bus endpoint."""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.ImportOverDBus%d" % nr

            testdata = str(self.data_path("serialized/test-movie.ttl"))

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            self.run_cli([COMMAND_NAME, "import", "--dbus-service", bus_name, testdata])

    def test_import_noargs(self):
        """Call import command with no arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli([COMMAND_NAME, "import"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Usage:", str(ex), "Output not found")

    def test_import_wrongarg(self):
        """Call import command with wrong arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli([COMMAND_NAME, "import", "--banana"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Unknown option", str(ex), "Output not found")

    def test_import_failure(self):
        """Import a Turtle file into a TinySPARQL database."""

        testdata = str(self.data_path("serialized/nonexistent.ttl"))

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

            ex = None
            try:
                self.run_cli([COMMAND_NAME, "import", "--database", tmpdir, testdata])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Could not run import", str(ex), "Output not found")

    def test_help(self):
        """Get help for a command."""

        with self.tmpdir() as tmpdir:
            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "help",
                    "import",
                ]
            )
            self.assertIn("TINYSPARQL-IMPORT(1)", output, "Manpage not found")

    def test_help_unknown(self):
        """Fail to get help for an unknown command."""
        ex = None

        with self.tmpdir() as tmpdir:
            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "help",
                        "banana",
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("CLI command failed", str(ex), "Failed to get an error")

    def test_noargs(self):
        """Call tinysparql with no args."""

        with self.tmpdir() as tmpdir:
            output = self.run_cli(
                [
                    COMMAND_NAME,
                ]
            )
            self.assertIn("usage:", output, "Mismatched output")
            self.assertIn("Available tinysparql commands are", output, "Mismatched output")

    def test_wrong_subcommand(self):
        """Call wrong subcommand."""
        ex = None

        with self.tmpdir() as tmpdir:
            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "banana",
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("is not a tinysparql command", str(ex), "Mismatched output")

    def test_list_dbus_endpoint(self):
        """List D-Bus endpoints"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.ListEndpoints%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--list"
                ]
            )
            self.assertIn(bus_name, output, "Mismatched output")

if __name__ == "__main__":
    fixtures.tracker_test_main()
