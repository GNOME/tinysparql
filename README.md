# TinySPARQL

The TinySPARQL library offers a complete RDF triplestore with SPARQL 1.1
interface and a minimal footprint. It allows creating local databases in memory
or the filesystem, and accessing/creating endpoints for federated queries.

This library is implemented in C, but can be used from many languages through
[GObject introspection](https://gi.readthedocs.io/en/latest/). The database
engine is implemented on top of [SQLite](https://sqlite.org) and is optimized
for desktop, embedded and mobile usecases.

Along with the library, there is also a set of commandline utilities that
expose most of the functionality offered by the library API.

See the [available documentation](https://gnome.pages.gitlab.gnome.org/tinysparql).

# Contact

TinySPARQL is a free software project developed in the open by volunteers. You
can ask questions about it at:

  * [Matrix](https://matrix.to/#/#tracker:gnome.org)
  * [GNOME Discourse](https://discourse.gnome.org/tag/tracker)

# Reporting issues

If you found an issue in TinySPARQL, a bug report at
https://gitlab.gnome.org/GNOME/tinysparql/issues will be welcome. Please
see the [GNOME bug reporting guidelines](https://handbook.gnome.org/issues/reporting.html)
for reported bugs.

# Contributing

Your contributions are greatly appreciated! TinySPARQL uses the Meson
build system, so its [general instructions](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project)
apply to it. We recommend the use of [`meson devenv`](https://mesonbuild.com/Commands.html#devenv)
to test local changes without modifications to your system. If you want to test the changes
more globally, we recommend installing into a Toolbx container, or using JHBuild to set up a
development environment that does not modify your system.

Please follow the [GNOME guidelines](https://handbook.gnome.org/development/change-submission.html)
to contribute your changes upstream.

For more information on the code itself, see the [hacking documentation](HACKING.md).
