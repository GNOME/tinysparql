# tracker-await-file
# Copyright (C) 2023, Sam Thursfield <sam@afuera.me.uk>
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

import gi

gi.require_version("Tracker", "3.0")
from gi.repository import Tracker

from argparse import ArgumentParser
from pathlib import Path
import logging
import os
import sys

from trackertestutils.storehelper import await_insert

log = logging.getLogger(__name__)


FILESYSTEM_GRAPH = "http://tracker.api.gnome.org/ontology/v3/tracker#FileSystem"


def argument_parser():
    parser = ArgumentParser(description="tracker-await-file utility")
    parser.add_argument('--debug', dest='debug', action='store_true',
                        help="Enable detailed logging to stderr")
    parser.add_argument('--allow-missing', action='store_true',
                        help="Don't raise an error if a file doesn't exist")
    parser.add_argument('--timeout', type=float, default=10.0,
                        help="Timeout in seconds to wait before raising error")
    parser.add_argument('file', nargs='+', help="File to await")
    return parser


class MinerFsHelper:
    def __init__(self):
        self.conn = Tracker.SparqlConnection.bus_new(
            service_name="org.freedesktop.Tracker3.Miner.Files",
            object_path=None,
            dbus_connection=None
        )

    def await_file(self, path: Path, timeout_seconds=2):
        """
        Return the resource representing `path`, awaiting its insertion if needed.
        """

        graph = FILESYSTEM_GRAPH

        expected = [
            "a nfo:FileDataObject",
            # This will be set once the miner-fs has processed the file.
            f"nie:url <{path.as_uri()}>",
            # This will be set once the extractor has processed the file.
            "tracker:extractorHash ?hash"
        ]
        predicates = "; ".join(expected)

        # This context manager will exit if a graph update arrives for a
        # resource matching `predicates`.
        await_ctx_mgr = await_insert(self.conn, graph, predicates, timeout=timeout_seconds, _check_inserted=False)
        with await_ctx_mgr as resource:
            query_existing = ' '.join([
                'SELECT ?urn tracker:id(?urn) '
                f' FROM NAMED <{graph}>',
                ' WHERE { '
                '   ?urn a rdfs:Resource ; ',
                predicates,
                '}'
            ])
            log.debug("Running query: %s", query_existing)
            cursor = self.conn.query(query_existing)
            if cursor.next():
                log.debug("Existing resource was returned: %s", resource)
                resource.urn = cursor.get_string(0)[0]
                resource.id = cursor.get_integer(1)
                return resource
            log.debug("No results for query")
        return resource


def main():
    args = argument_parser().parse_args()

    if args.debug:
        logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)

    helper = MinerFsHelper()

    for file in args.file:
        path = Path(file)

        if args.allow_missing or path.exists():
            resource = helper.await_file(path, timeout_seconds=args.timeout)
            print(f"File {file} is indexed as {resource.urn}")
        else:
            raise RuntimeError(f"File {path} does not exist, and `--allow-missing` was not passed.")


try:
    main()
except RuntimeError as e:
    sys.stderr.write("ERROR: {}\n".format(e))
    sys.exit(1)
