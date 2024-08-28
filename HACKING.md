# TinySPARQL hacking guide

This file contains information useful for developers who want to improve
TinySPARQL.

## Automated testing

You can run the TinySPARQL testsuite using the `meson test` command.

TinySPARQL main gitlab repository runs the tests automatically in CI. The
`.gitlab-ci.yml` file controls this. Here are the latest tests that were run:

 * https://gitlab.gnome.org/GNOME/tinysparql/pipelines

Most distros also run the test suite as part of their building process.

## Logging

The following environment variables control logging from code using the
TinySPARQL library:

  * `G_MESSAGES_DEBUG`: controls log output from all GLib-based libraries
    in the current process. Use `G_MESSAGES_DEBUG=Tracker` to see TinySPARQL
    related messages, or `G_MESSAGES_DEBUG=all` to see everything.
  * `TINYSPARQL_DEBUG`: takes a comma-separated list of keywords to enable
    extra debugging output. Use the keyword 'help' for a list of keywords.

    The options include:
      - `collation`
      - `ontology-changes`
      - `sparql`
      - `sql-statements`
      - `fts-integrity`

You can use `TINYSPARQL_DEBUG=tests` to see logging from the test harness,
including full log output from the internal D-Bus daemon for functional-tests.
Note that by default, functional tests filter output from the D-Bus daemon to
only show log messages from the test processes. Anything written directly to
stdout, for example by `g_print()` or by the dbus-daemon itself, will not be
displayed unless `TINYSPARQL_DEBUG=tests` is set.

When working with GitLab CI, you can use the
[Run Pipeline dialog](https://gitlab.gnome.org/GNOME/tinysparql/pipelines/new)
to set the values of these variables and increase the verbosity of the tests in
CI.

## Updating the database internal format

Any code change affecting the internal format of the database, ancillary
tables, indexes, triggers, etc... Should be accompanied by:

 * An update to TrackerDBVersion enum and TRACKER_DB_VERSION_NOW define at
   `src/libtracker-sparql/core/tracker-db-manager.h`
 * Code to transparently update databases from the prior version at
   `tracker_data_manager_update_from_version()`
 * An additional set of version tests at
   `tests/core/tracker-initialization-test.c` for the new old version. Created
   with the nepomuk ontology, and the fts/non-fts ontologies available at
   `tests/core/initialization`, e.g.:
   ```
   $ cd /tmp
   $ tinysparql3 endpoint -p .../tests/core/initialization/fts -d .
   $ xz -z meta.db
   $ mv meta.db.xz .../fts.db.26.xz
   ```

This is not required for changes to the ontologies (core, nepomuk).
