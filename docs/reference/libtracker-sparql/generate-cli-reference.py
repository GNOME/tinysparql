#!/usr/bin/env python3
# Tracker commandline reference markdown generator.
#
# Copyright 2020, Sam Thursfield <sam@afuera.me.uk>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.

import argparse
import contextlib
import logging
import pathlib
import shutil
import subprocess
import sys
import tempfile

log = logging.getLogger('generate-cli-reference.py')

asciidoc = shutil.which('asciidoc')
mkdocs = shutil.which('mkdocs')
xmlto = shutil.which('xmlto')


def argument_parser():
    parser = argparse.ArgumentParser(
        description="Tracker website build script")
    parser.add_argument('--template', required=True, metavar='TEMPLATE',
                        help="Template")
    parser.add_argument('--output', required=True, metavar='OUTPUT',
                        help="Output file")
    parser.add_argument('--debug', dest='debug', action='store_true',
                        help="Enable detailed logging to stderr")
    parser.add_argument('--man-pages', nargs='+', required=True,
                        help="List of Asciidoc manual page source")
    return parser


class Manpages():
    def run(self, command):
        command = [str(c) for c in command]
        log.debug("Running: %s", ' '.join(command))
        subprocess.run(command, check=True)

    def generate_manpage_xml(self, in_path, out_path):
        """Generate a docbook XML file for an Asciidoc manpage source file."""
        self.run([asciidoc, '--backend', 'docbook', '--doctype', 'manpage',
                 '--out-file', out_path, in_path])

    def generate_manpage_html(self, in_path, out_path):
        """Generate a HTML page from a docbook XML file"""
        self.run([xmlto, 'xhtml-nochunks', '-o', out_path, in_path])

    def generate_toplevel_markdown(self, in_path, out_path, html_files):
        """Generate the commandline reference page."""

        includes = []
        for html_file in sorted(html_files):
            path = pathlib.Path(html_file)
            filename = path.name;
            title = filename.split('.')[0]
            if title.startswith('tracker3-'):
                title = 'tracker3 ' + title[9:]
            includes.append("## %s\n\n%s\n\n" % (title, path.read_text()))

        text = in_path.read_text()
        text = text.format(
            includes='\n'.join(includes)
        )
        out_path.write_text(text)


@contextlib.contextmanager
def tmpdir():
    path = pathlib.Path(tempfile.mkdtemp())
    log.debug("Created temporary directory %s", path)
    try:
        yield path
    finally:
        log.debug("Removed temporary directory %s", path)
        shutil.rmtree(path)


def main():
    args = argument_parser().parse_args()
    template_file = pathlib.Path(args.template)
    output_file = pathlib.Path(args.output)

    if args.debug:
        logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
    else:
        logging.basicConfig(stream=sys.stderr, level=logging.INFO)

    if output_file.exists():
        output_file.unlink()

    if asciidoc is None:
        raise RuntimeError("The 'asciidoc' tool is required.")
    if xmlto is None:
        raise RuntimeError("The 'xmlto' tool is required.")

    with tmpdir() as workdir:
        manpages = Manpages()

        htmldir = workdir
        htmlfiles = []

        for man_page in args.man_pages:
            asciidocpath = pathlib.Path(man_page)
            xmlpath = workdir.joinpath(asciidocpath.stem + '.xml')

            manpages.generate_manpage_xml(asciidocpath, xmlpath)
            manpages.generate_manpage_html(xmlpath, htmldir)

            htmlpath = htmldir.joinpath(xmlpath.with_suffix('.html').name)
            htmlfiles.append(htmlpath)

        manpages.generate_toplevel_markdown(template_file, output_file, htmlfiles)


try:
    main()
except (RuntimeError, subprocess.CalledProcessError) as e:
    sys.stderr.write(f"ERROR: {e}\n")
    sys.exit(1)
