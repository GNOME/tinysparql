#
# Copyright (C) 2010, Nokia <jean-luc.lamadon@nokia.com>
# Copyright (C) 2019, Sam Thursfield <sam@afuera.me.uk>
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

import json
import logging
import os
import pathlib
import sys


DEFAULT_TIMEOUT = 10


if 'TRACKER_FUNCTIONAL_TEST_CONFIG' not in os.environ:
    raise RuntimeError("The TRACKER_FUNCTIONAL_TEST_CONFIG environment "
                       "variable must be set to point to the location of "
                       "the generated configuration.json file.")

with open(os.environ['TRACKER_FUNCTIONAL_TEST_CONFIG']) as f:
    config = json.load(f)


def ontologies_dir():
    return config['TEST_ONTOLOGIES_DIR']


def get_environment_boolean(variable):
    '''Parse a yes/no boolean passed through the environment.'''

    value = os.environ.get(variable, 'no').lower()
    if value in ['no', '0', 'false']:
        return False
    elif value in ['yes', '1', 'true']:
        return True
    else:
        raise RuntimeError('Unexpected value for %s: %s' %
                           (variable, value))


def get_environment_int(variable, default=0):
    try:
        return int(os.environ.get(variable))
    except (TypeError, ValueError):
        return default


if get_environment_boolean('TRACKER_TESTS_VERBOSE'):
    # Output all logs to stderr
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
else:
    # Output some messages from D-Bus daemon to stderr by default. In practice,
    # only errors and warnings should be output here unless the environment
    # contains G_MESSAGES_DEBUG= and/or TRACKER_VERBOSITY=1 or more.
    handler_stderr = logging.StreamHandler(stream=sys.stderr)
    handler_stderr.addFilter(logging.Filter('trackertestutils.dbusdaemon.stderr'))
    handler_stdout = logging.StreamHandler(stream=sys.stderr)
    handler_stdout.addFilter(logging.Filter('trackertestutils.dbusdaemon.stdout'))
    logging.basicConfig(level=logging.INFO,
                        handlers=[handler_stderr, handler_stdout],
                        format='%(message)s')
