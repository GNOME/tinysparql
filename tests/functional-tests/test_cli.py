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

    def test_query_dbus(self):
        """Query from a D-Bus endpoint"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.Query%d" % nr

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
                    "query",
                    "--dbus-service", bus_name,
                    "--arg", "a:i:42",
                    "--arg", "b:d:1.2345",
                    "--arg", "c:s:banana",
                    "--arg", "d:b:true",
                    "SELECT (~a AS ?a) (~b AS ?b) (~c AS ?c) (~d AS ?d) {}"
                ]
            )
            self.assertIn("42", output, "Mismatched output")
            self.assertIn("banana", output, "Mismatched output")
            self.assertIn("true", output, "Mismatched output")

    def test_query_http(self):
        """Query from a HTTP endpoint"""

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

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "query",
                    "--remote-service", address,
                    "--arg", "a:i:42",
                    "--arg", "b:d:1.2345",
                    "--arg", "c:s:banana",
                    "--arg", "d:b:true",
                    "SELECT (~a AS ?a) (~b AS ?b) (~c AS ?c) (~d AS ?d) {}"
                ]
            )
            self.assertIn("42", output, "Mismatched output")
            self.assertIn("banana", output, "Mismatched output")
            self.assertIn("true", output, "Mismatched output")

    def test_query_file(self):
        """Query a SPARQL file"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.QueryFile%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            testdata = str(self.data_path("query/query.rq"))
            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "query",
                    "--dbus-service", bus_name,
                    "--file", testdata,
                ]
            )
            self.assertIn("Hello", output, "Mismatched output")

    def test_query_update(self):
        """Update a local TinySPARQL database."""

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

            self.run_cli(
                [
                    COMMAND_NAME,
                    "query",
                    "--database", tmpdir,
                    "--arg", "title:s:'It came from the sea'",
                    "--update",
                    "INSERT DATA { _:a a nmm:Video ; nie:title ~title }"
                ]
            )

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "query",
                    "--database", tmpdir,
                    "select ?u fts:offsets(?u) nie:title(?u) { ?u fts:match 'It' }"
                ]
            )
            self.assertIn("from the sea", output, "Output not found")

    def test_query_bad_file(self):
        """Query a SPARQL file"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.QueryFile%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            testdata = str(self.data_path("query/nonexistent.rq"))
            ex = None
            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "query",
                        "--dbus-service", bus_name,
                        "--file", testdata,
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Failed to open file", str(ex), "Mismatched output")

    def test_query_bad_sparql(self):
        """Query using bad SPARQL"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.Query%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "query",
                        "--dbus-service", bus_name,
                        "banana"
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Could not run query", str(ex), "Mismatched output")

    def test_query_invalid_arg(self):
        """Query using bad SPARQL"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.Query%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "query",
                        "--dbus-service", bus_name,
                        "--arg", "a:banana",
                        "select (~a as $a) {}"
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Invalid argument string", str(ex), "Mismatched output")

    def test_query_invalid_arg2(self):
        """Query using bad SPARQL"""

        with self.tmpdir() as tmpdir:
            nr = random.randint(0, 65000)
            bus_name = "org.example.Query%d" % nr

            self.run_background(
                [
                    COMMAND_NAME,
                    "endpoint",
                    "--ontology", "nepomuk",
                    "--dbus-service", bus_name,
                ],
                "Listening",
            )

            try:
                output = self.run_cli(
                    [
                        COMMAND_NAME,
                        "query",
                        "--dbus-service", bus_name,
                        "--arg", "a:z:banana",
                        "select (~a as $a) {}"
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Invalid parameter type", str(ex), "Mismatched output")

    def test_query_noargs(self):
        """Call query command with no arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli([COMMAND_NAME, "query"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Usage:", str(ex), "Output not found")

    def test_query_wrongarg(self):
        """Call query command with wrong arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli([COMMAND_NAME, "query", "--banana"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Unknown option", str(ex), "Output not found")

    def test_introspect(self):
        """Call introspect command"""
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

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "introspect",
                    "--database",
                    tmpdir,
                ]
            )
            self.assertIn("rdfs:Class", output, "Output not found")

    def test_introspect_properties(self):
        """Call introspect command"""
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

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "introspect",
                    "--database",
                    tmpdir,
                    "--list-properties",
                    "rdfs:Class",
                ]
            )
            self.assertIn("rdfs:subClassOf", output, "Output not found")

    def test_introspect_tree_print(self):
        """Call introspect command"""
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

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "introspect",
                    "--database",
                    tmpdir,
                    "--list-properties",
                    "--tree",
                    "nmm:MusicPiece",
                ]
            )
            self.assertIn("nie:InformationElement", output, "Output not found")

    def test_introspect_search(self):
        """Call introspect command"""
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

            output = self.run_cli(
                [
                    COMMAND_NAME,
                    "introspect",
                    "--database",
                    tmpdir,
                    "--search",
                    "Class",
                ]
            )
            self.assertIn("rdfs:Class", output, "Output not found")
            self.assertIn("rdfs:subClassOf", output, "Output not found")

    def test_sql(self):
        """Call sql command"""
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

            output = self.run_cli(
                [
                    "tinysparql-sql",
                    "--database",
                    tmpdir,
                    "--query",
                    "SELECT 42",
                ]
            )
            self.assertIn("42", output, "Output not found")

    def test_sql_file(self):
        """Call sql command from file"""
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

            testdata = str(self.data_path("sql/query.sql"))
            output = self.run_cli(
                [
                    "tinysparql-sql",
                    "--database",
                    tmpdir,
                    "--file",
                    testdata,
                ]
            )
            self.assertIn("42", output, "Output not found")

    def test_sql_bad_file(self):
        """Call sql command from file"""
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

            testdata = str(self.data_path("sql/nonexistent.sql"))
            ex = None
            try:
                output = self.run_cli(
                    [
                        "tinysparql-sql",
                        "--database",
                        tmpdir,
                        "--file",
                        testdata,
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Failed to open file", str(ex), "Output not found")

    def test_sql_noargs(self):
        """Call sql command with no arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli(["tinysparql-sql", "sql"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("A database path must be specified", str(ex), "Output not found")

    def test_sql_noargs2(self):
        """Call sql command with no query"""
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

            ex = None
            try:
                output = self.run_cli(
                    [
                        "tinysparql-sql",
                        "--database",
                        tmpdir,
                    ]
                )
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Usage:", str(ex), "Output not found")

    def test_sql_wrongarg(self):
        """Call sql command with wrong arguments."""

        with self.tmpdir() as tmpdir:
            ex = None
            try:
                self.run_cli(["tinysparql-sql", "sql", "--banana"])
            except Exception as e:
                ex = e
            finally:
                self.assertIn("Unknown option", str(ex), "Output not found")

if __name__ == "__main__":
    fixtures.tracker_test_main()
