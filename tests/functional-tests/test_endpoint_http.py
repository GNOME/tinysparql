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
import random
from urllib.request import urlopen


class TestEndpointHttp(fixtures.TrackerCommandLineTestCase):
    def test_http_endpoint(self):
        """Create a HTTP endpoint for local testing"""

        with self.tmpdir() as tmpdir:
            port = random.randint(32000, 65000)
            address = "http://127.0.0.1:%d/sparql/" % port

            # Create the database
            self.run_background(
                [
                    "tracker3",
                    "endpoint",
                    "--database",
                    tmpdir,
                    "--ontology",
                    "nepomuk",
                    "--http-port",
                    port,
                ],
                "Listening",
            )

            # Sanity check that it works.
            self.run_cli(
                [
                    "tracker3",
                    "sparql",
                    "--remote-service",
                    address,
                    "--query",
                    "ASK { ?u a rdfs:Resource }",
                ]
            )

            # This should return a description of the endpint
            with urlopen(address) as response:
                data = response.read().decode()


if __name__ == "__main__":
    fixtures.tracker_test_main()
