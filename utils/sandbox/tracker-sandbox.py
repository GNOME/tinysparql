#!/usr/bin/env python
#
# Copyright (C) 2012-2013 Martyn Russell <martyn@lanedo.com>
# Copyright (C) 2012      Sam Thursfield <sam.thursfield@codethink.co.uk>
#
# This script allows a user to utilise Tracker for local instances by
# specifying an index directory location where the Tracker data is
# stored and a content directory location where the content to be
# indexed is kept. From there, queries or a shell can be launched to
# use that data.
#
# This was initially a shell script by Sam and later converted into a
# more comprehensive python script by Martyn.
#
# Usage:
#  - Create or update an index stored in tracker/ subdir with content in html/
#      tracker-sandbox.py -i tracker -c html -u
#  - Query for 'foo'
#      tracker-sandbox.py -i tracker -c html -q foo
#  - List files in index
#      tracker-sandbox.py -i tracker -c html -l
#  - Start shell with environment set up
#      tracker-sandbox.py -i tracker -c html -s
#  - Test with different prefixes, e.g. /usr/local installs
#      tracker-sandbox.py -i tracker -c html -s -p /usr/local
#  ...
#
# Changes:
#  - If you make _ANY_ changes, please send them in so I can incorporate them.
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

import locale
import os
import subprocess
import optparse
import signal
import sys
import string
import errno
import gi

from multiprocessing import Process

import ConfigParser

from gi.repository import Tracker, GObject

# Script
script_name = 'tracker-sandbox'
script_version = '0.1'
script_about = 'Localised Tracker sandbox for content indexing and search'

index_location_abs = ''
content_location_abs = ''

default_prefix = '/usr'
default_debug_verbosity = 2

# Session
dbus_session_pid = -1
dbus_session_address = ''
dbus_session_file = ''

store_pid = -1
store_proc = None

# Template config file
config_template = """
[General]
Verbosity=0
SchedIdle=0
InitialSleep=0

[Monitors]
EnableMonitors=false

[Indexing]
Throttle=0
IndexOnBattery=true
IndexOnBatteryFirstTime=true
IndexRemovableMedia=false
IndexOpticalDiscs=false
LowDiskSpaceLimit=-1
IndexRecursiveDirectories=;
IndexSingleDirectories=;
IgnoredDirectories=;
IgnoredDirectoriesWithContent=;
IgnoredFiles=
CrawlingInterval=-1
RemovableDaysThreshold=3

[Writeback]
EnableWriteback=false
"""

# Utilities
def mkdir_p(path):
	try:
		os.makedirs(path)
	except OSError as exc: # Python >2.5
		if exc.errno == errno.EEXIST:
			pass
		else:
			raise

def debug(message):
	if opts.debug:
		print(message)

# DB functions (sync for now)
def db_query_have_files():
	# Set this here in case we used 'bus' for an update() before this.
	# os.environ['TRACKER_SPARQL_BACKEND'] = 'direct'

	print 'Using query to check index has data in it...'

	conn = Tracker.SparqlConnection.get(None)
	cursor = conn.query('select count(?urn) where { ?urn a nfo:FileDataObject }', None)

	# Only expect one result here...
	while (cursor.next(None)):
		print '  Currently %d file(s) exist in our index' % (cursor.get_integer(0))

def db_query_list_files():
	# Set this here in case we used 'bus' for an update() before this.
	# os.environ['TRACKER_SPARQL_BACKEND'] = 'direct'

	print 'Using query to list files indexed...'

	conn = Tracker.SparqlConnection.get(None)
	cursor = conn.query('select nie:url(?urn) where { ?urn a nfo:FileDataObject }', None)

	# Only expect one result here...
	while (cursor.next(None)):
		print '  ' + cursor.get_string(0)[0]

def db_query_files_that_match():
	conn = Tracker.SparqlConnection.get(None)
	cursor = conn.query('select nie:url(?urn) where { ?urn a nfo:FileDataObject . ?urn fts:match "%s" }' % (opts.query), None)

	print 'Found:'

	# Only expect one result here...
	while (cursor.next(None)):
		print '  ' + cursor.get_string(0)[0]

# Index functions
def index_clean():
	#tracker-control -r
	debug ('Cleaning index, FIXME: Does nothing.')

def find_libexec_binaries(command):
	binary = os.path.join(opts.prefix, 'libexec', command)
	if not os.path.exists(binary):
		binary = os.path.join(opts.prefix, 'libexec', command)
		if not os.path.exists(binary):
			return None

	return binary

def index_update():
	debug('Updating index ...')
	debug('--')

	# FIXME: Need to start tracker-extract to make sure extended
	# metadata is created, but the problem is, after miner-fs
	# stops, we return to the prompt, so how do we handle that?
	#
	# We need to listen to signals from tracker-extract and then
	# quit after some inactivity I think ... OR listen to
	# GraphUpdated and when there are no more objects without a
	# data-source, we know all data was indexed.

	# Start tracker-miner-fs
	binary = find_libexec_binaries ('tracker-miner-fs')
	if binary == None:
		print 'Could not find "tracker-miner-fs" in $prefix/lib{exec} directories'
		print 'Is Tracker installed properly?'
		sys.exit(1)

	try:
		# Mine data WITHOUT being a daemon, exit when done. Ignore desktop files
		subprocess.check_output([binary, "--no-daemon", "--disable-miner=applications"])
	except subprocess.CalledProcessError, e:
		print 'Could not run %s, %s' % (binary, e.output)
		sys.exit(1)

	debug('--')

	# We've now finished updating the index now OR we completely failed
	print 'Index now up to date!'

	# Check we have data in our index...
	db_query_have_files()

def index_shell():
	print 'Starting shell... (type "exit" to finish)'
	print

	os.system("/bin/bash")

# Environment / Clean up
def dbus_session_get_from_content(content):
	global dbus_session_address
	global dbus_session_pid

	if len(content) < 1:
		print 'Content was empty ... can not get DBus session information from empty string'
		return False
	
	dbus_session_address = content.splitlines()[0]
	dbus_session_pid = int(content.splitlines()[1])

	if dbus_session_address == '':
 		print 'DBus session file was corrupt (no address), please remove "%s"' % (dbus_session_file)
		sys.exit(1)
	if dbus_session_pid < 0:
 		print 'DBus session file was corrupt (no PID), please remove "%s"' % (dbus_session_file)
		sys.exit(1)

	return True

def dbus_session_file_get():
	try:
		f = open(dbus_session_file, 'r')
		content = f.read()
		f.close()
	except IOError as e:
		# Expect this if we have a new session to set up
		return False
	except:
		print "Unexpected error:", sys.exc_info()[0]
		raise

	return dbus_session_get_from_content(content)

def dbus_session_file_set():
	mkdir_p(os.environ['XDG_RUNTIME_DIR'])

	content = '%s\n%s' % (dbus_session_address, dbus_session_pid)
	f = open(dbus_session_file, 'w')
	f.write(content)
	f.close()

def environment_unset():
	debug('Cleaning up files ...')

	if not dbus_session_file == '':
		debug('  Removing DBus session file')
		os.unlink(dbus_session_file)

	debug('Cleaning up processes ...')

	if dbus_session_pid > 0:
		debug('  Killing DBus session')
		try:
			os.kill(dbus_session_pid, signal.SIGTERM)
		except (SystemError, OSError): # (3, 'No such process') old python-schedutils incorrectly raised SystemError
			debug('    Process %d not found', dbus_session_pid)


	if not opts.update:
		return

	# FIXME: clean up tracker-store, can't use tracker-control for this,
	#        that kills everything it finds in /proc sadly.
	if store_pid > 0:
		debug('  Killing Tracker store')
		os.kill(store_pid, signal.SIGTERM)

def environment_set_and_add_path(env, prefix, suffix):
	new = os.path.join(prefix, suffix)

	if os.environ.has_key(env):
		existing = os.environ[env]
		full = '%s:%s' % (new, existing)
	else:
		full = new

	os.environ[env] = full

def environment_set():
	# Environment
	global dbus_session_address
	global dbus_session_pid
	global dbus_session_file
	global index_location_abs
	global content_location_abs
	global default_debug_verbosity

	index_location_abs = os.path.abspath (opts.index_location)

	if opts.update:
		# Only needed for updating index
		content_location_abs = os.path.abspath (opts.content_location)

	# Data
	os.environ['XDG_DATA_HOME'] = '%s/data/' % index_location_abs
	os.environ['XDG_CONFIG_HOME'] = '%s/config/' % index_location_abs
	os.environ['XDG_CACHE_HOME'] = '%s/cache/' % index_location_abs
	os.environ['XDG_RUNTIME_DIR'] = '%s/run/' % index_location_abs

	# Prefix - only set if non-standard
	if opts.prefix != default_prefix:
		environment_set_and_add_path ('PATH', opts.prefix, 'bin')
		environment_set_and_add_path ('LD_LIBRARY_PATH', opts.prefix, 'lib')
		environment_set_and_add_path ('XDG_DATA_DIRS', opts.prefix, 'share')

		os.environ['TRACKER_DB_ONTOLOGIES_DIR'] = os.path.join(opts.prefix, 'share', 'tracker', 'ontologies')
		os.environ['TRACKER_EXTRACTOR_RULES_DIR'] = os.path.join(opts.prefix, 'share', 'tracker', 'extract-rules')
		os.environ['TRACKER_LANGUAGE_STOPWORDS_DIR'] = os.path.join(opts.prefix, 'share', 'tracker', 'languages')

	# Preferences
	os.environ['TRACKER_USE_CONFIG_FILES'] = 'yes'

	#if opts.debug:
	#	os.environ['TRACKER_USE_LOG_FILES'] = 'yes'

	if opts.debug:
		os.environ['G_MESSAGES_DEBUG'] = 'all'
		os.environ['TRACKER_VERBOSITY'] = '%d' % default_debug_verbosity
		os.environ['DBUS_VERBOSE'] = '1'
	else:
		os.environ['TRACKER_VERBOSITY'] = '0'

	debug('Using prefix location "%s"' % opts.prefix)
	debug('Using index location "%s"' % index_location_abs)

	if opts.update:
		debug('Using content location "%s"' % content_location_abs)

		# Make sure File System miner is configured correctly
		config_dir = os.path.join(os.environ['XDG_CONFIG_HOME'], 'tracker')
		config_filename = os.path.join(config_dir, 'tracker-miner-fs.cfg')

		debug('Using config file "%s"' % config_filename)

		# Only update config if we're updating the database
		mkdir_p(config_dir)

		if not os.path.exists(config_filename):
			f = open(config_filename, 'w')
			f.write(config_template)
			f.close()

			debug('  New file written')

		# Set content path
		config = ConfigParser.ConfigParser()
		config.optionxform = str
		config.read(config_filename)
		config.set('Indexing', 'IndexRecursiveDirectories', content_location_abs + ";")

		with open(config_filename, 'wb') as f:
			config.write(f)

	# Ensure directory exists
	# DBus specific instance
	dbus_session_file = os.path.join(os.environ['XDG_RUNTIME_DIR'], 'dbus-session')

	if dbus_session_file_get() == False:
		output = subprocess.check_output(["dbus-daemon",
						  "--session",
						  "--print-address=1",
						  "--print-pid=1",
						  "--fork"])

		dbus_session_get_from_content(output)
		dbus_session_file_set()
		debug('Using new D-Bus session with address "%s" with PID %d' % (dbus_session_address, dbus_session_pid))
	else:
		debug('Using existing D-Bus session with address "%s" with PID %d' % (dbus_session_address, dbus_session_pid))

	# Important, other subprocesses must use our new bus
	os.environ['DBUS_SESSION_BUS_ADDRESS'] = dbus_session_address

# Entry point/start
if __name__ == "__main__":
	locale.setlocale(locale.LC_ALL, '')

	# Parse command line
	usage_oneline  = '%s -i <DIR> -c <DIR> [OPTION...]' % (os.path.basename(sys.argv[0]))
	usage = '\n  %s - %s' % (usage_oneline, script_about)
	usage_invalid = 'Usage:\n  %s' % (usage_oneline)

	popt = optparse.OptionParser(usage)
	popt.add_option('-v', '--version',
			action = 'count',
			dest = 'version',
			help = 'show version information')
	popt.add_option('-d', '--debug',
			action = 'count',
			dest = 'debug',
			help = 'show additional debugging')
	popt.add_option('-p', '--prefix',
			action = 'store',
			metavar = 'PATH',
			dest = 'prefix',
			default = default_prefix,
			help = 'use a non-standard prefix (default="%s")' % default_prefix)
	popt.add_option('-i', '--index',
			action = 'store',
			metavar = 'DIR',
			dest = 'index_location',
			help = 'directory storing the index')
	popt.add_option('-c', '--content',
			action = 'store',
			metavar = 'DIR',
			dest = 'content_location',
			help = 'directory storing the content which is indexed')
	popt.add_option('-u', '--update',
			action = 'count',
			dest = 'update',
			help = 'update index/database from content')
	popt.add_option('-l', '--list-files',
			action = 'count',
			dest = 'list_files',
			help = 'list files indexed')
	popt.add_option('-s', '--shell',
			action = 'count',
			dest = 'shell',
			help = 'start a shell with the environment set up')
	popt.add_option('-q', '--query',
			action = 'store',
			metavar = 'CRITERIA',
			dest = 'query',
			help = 'what content to look for in files')

	(opts, args) = popt.parse_args()

	if opts.version:
		print '%s %s\n%s\n' % (script_name, script_version, script_about)
		sys.exit(0)

	if not opts.index_location and not opts.content_location:
		print 'Expected index (-i) or content (-c) locations to be specified'
		print usage_invalid
		sys.exit(1)

	if opts.update and (not opts.index_location or not opts.content_location):
		print 'Expected index (-i) and content (-c) locations to be specified'
		print 'These arguments are required to update the index databases'
		sys.exit(1)

	if (opts.query or opts.query or opts.list_files or opts.shell) and not opts.index_location:
		print 'Expected index location (-i) to be specified'
		print 'This arguments is required to use the content that has been indexed'
		sys.exit(1)

	if not opts.update and not opts.query and not opts.list_files and not opts.shell:
		print 'No action specified (e.g. update (-u), shell (-s), list files (-l), etc)\n'
		print '%s %s\n%s\n' % (script_name, script_version, script_about)
		print usage_invalid
		sys.exit(1)

	# Set up environment variables and foo needed to get started.
	environment_set()

	try:
		if opts.update:
			index_update()

		if opts.list_files:
			db_query_list_files()

		if opts.shell:
			index_shell()
			sys.exit(0)

		if opts.query:
			if not os.path.exists(index_location_abs):
				print 'Can not query yet, index has not been created, see --update or -u'
				print usage_invalid
				sys.exit(1)

			db_query_files_that_match()

	except KeyboardInterrupt:
		print 'Handling Ctrl+C'

	environment_unset()
