# Tracker

Tracker is an efficient search engine and
[triplestore](https://en.wikipedia.org/wiki/Triplestore) for desktop, embedded
and mobile.

The Tracker project is divided into two main repositories:

  * [Tracker SPARQL](https://gitlab.gnome.org/GNOME/tracker) contains the
    triplestore database, provided as the `libtracker-sparql` library
    and implemented using [SQLite](http://sqlite.org/). This repo also contains
    the database ontologies and the commandline user interface (`tracker3`).

  * [Tracker Miners](https://gitlab.gnome.org/GNOME/tracker-miners) contains
    the indexer daemon (*tracker-miner-fs-3*) and tools to extract metadata
    from many different filetypes.

More information on Tracker can be found at:

  * <https://gnome.pages.gitlab.gnome.org/tracker/>
  * <https://wiki.gnome.org/Projects/Tracker>

Source code and issue tracking:

  * <https://gitlab.gnome.org/GNOME/tracker>
  * <https://gitlab.gnome.org/GNOME/tracker-miners>

All discussion related to Tracker happens on:

  * <https://discourse.gnome.org/tag/tracker>

IRC channel #tracker on:

  * [irc.gimp.net](irc://irc.gimp.net)

Related projects:

  * [GNOME Online Miners](https://gitlab.gnome.org/GNOME/gnome-online-miners/)
    extends Tracker to allow searching and indexing some kinds of online
    content.

## Goals and Non-goals

Here's a summary of the goals and non-goals of the Tracker project.

 1. Provide real-time searching and browsing of desktop content.
 2. Provide searchable data storage for desktop apps.
 3. Allow full-text search within common document types.
 4. Allow advanced queries using a standard query language.
 5. Be secure and private by default.
 6. Be efficient enough for desktop and mobile use.
 7. Be maintainable by a small team.

These are some things we currently don't want to do in Tracker itself.

 * Depend on features of a specific filesystem. Tracker should work on
   all commonly used filesystems.
 * Index source code. There are plenty of IDEs and source-code indexing tools already.
 * Index user data in network / cloud services. This is out of scope for the core of
   Tracker but can be done by third party projects -- see
   [Gnome Online Miners](https://wiki.gnome.org/Projects/GnomeOnlineMiners).
 * Index data on the Web.
 * Provide our own GUI components. Tracker is a middleware component, not an end user
   application.

## Developing Tracker

If you want to help develop and improve Tracker, great! Remember that Tracker
is a middleware component, designed to be integrated into larger codebases. To
fully test a change you may need to build and test Tracker as part of another
project.

For the GNOME desktop, consider using the documented [Building a System
Component](https://wiki.gnome.org/Newcomers/BuildSystemComponent) workflow.

It's also possible to build Tracker on its own and install it inside your home
directory for testing purposes.  Read on for instructions on how to do this.

### Compilation

Tracker uses the [Meson build system](http://mesonbuild.com), which you must
have installed in order to build Tracker.

We recommend that you build tracker core as a subproject of tracker-miners.
You can do this by cloning both repos, then creating a symlink in the
`subprojects/` directory of tracker-miners.git to the tracker.git checkout.

    git clone https://gitlab.gnome.org/GNOME/tracker.git
    git clone https://gitlab.gnome.org/GNOME/tracker-miners.git

    mkdir tracker-miners/subprojects
    ln -s ../../tracker tracker-miners/subprojects/

Now you can run the commands below to build Tracker and install it in a
new, isolated prefix named `opt/tracker` inside your home folder.

> NOTE: If you see 'dependency not found' errors from Meson, that means there
> is a package missing on your computer that you need to install so you can
> compile Tracker. On Ubuntu/Debian, you can run `apt build-dep tracker-miners`
> and on Fedora `dnf build-dep tracker-miners` to install all the necessary
> packages.

    cd tracker-miners
    meson ./build --prefix=$HOME/opt/tracker -Dtracker_core=subproject
    cd build
    ninja install

### Running the testsuite

At this point you can run the Tracker test suite from the `build` directory:

    meson test --print-errorlogs

### Using the run-uninstalled script

Tracker normally runs automatically, indexing content in the background so that
search results are available quickly when needed.

When developing and testing Tracker you will normally want it to run in the
foreground instead. You also probably want to run it from a build tree, rather
than installing it somewhere everytime you make a change, and you certainly
should isolates your development version from the real Tracker database in your
home directory.

There is a tool to help with this, which is part of the 'trackertestutils'
Python module.  You can run the tool using a helper script generated in the
tracker-miners.git build process named 'run-uninstalled'.

Check the helper script is set up correctly by running this from your
tracker-miners.git build tree:

    ./run-uninstalled --help

If run with no arguments, the script will start an interactive shell. Any
arguments after a `--` sentinel are treated as a command to run in a non-interactive
shell.

Now check that it runs the correct version of the Tracker CLI:

    ./run-uninstalled -- tracker3 --version

Let's try and index some content. (Subtitute ~/Music for any other location
where you have interesting data). We need to explicitly tell the script to wait
for the miners to finish, or it will exit too soon. (This is a workaround for
[issue #122](https://gitlab.gnome.org/GNOME/tracker/issues/122).)

    ./run-uninstalled --wait-for-miner=Files --wait-for-miner=Extract -- tracker3 index --add ~/Music

Let's see what files were found!

    ./run-uninstalled  -- tracker3 sparql --dbus-service=org.freedesktop.Tracker3.Miner.Files -q 'SELECT ?url { ?u nie:url ?url }'

Or, you can try a full-text search ...

    ./run-uninstalled -- tracker3 search "bananas"

There are many more things you can do with the script.

For more information about developing Tracker, look at
https://wiki.gnome.org/Projects/Tracker and HACKING.md.
