# FAQ

## What is Tracker?

It's a search engine, and a database.

Tracker Miner FS indexes content from your home directory automatically, so
applications can provide instant search results when you need them.

See the [overview](../overview) for more information.

## What files will Tracker index?

The default configuration of Tracker Miner FS is to look at files and folders
in your XDG content directories such as `Documents`, `Music`, `Pictures` and
`Videos`.  It also looks at files in your home directory and `Downloads`
directory, but it doesn't recurse into folders there.

You might want to [control what Tracker indexes] so that it finds files in
other places too.

## Does Tracker recursively index my home directory?

Not by default. See [What files will Tracker index](#what-files-will-tracker-index).

## How can I control what Tracker indexes?

In GNOME, you can use the Search Settings panel to control what Tracker
Miner FS indexes. See [GNOME's
documentation](https://help.gnome.org/users/gnome-help/unstable/files-search.html.en).

You can control Tracker Miner FS's configuration directly using
[dconf-editor](https://wiki.gnome.org/Apps/DconfEditor) or the `gsettings` CLI
tool.
The relevant schemas are `org.freedesktop.Tracker.Miner.Files` and
`org.freedesktop.Tracker.Extract`.

To tell Tracker Miner FS to ignore a directory and all its contents, you can
create an empty file named `.nomedia` inside the directory. This trick also
works [on Android](https://www.lifewire.com/nomedia-file-4172882) devices.
Files named `.trackerignore`, `.git` and `.hg` have the same effect. You can
configure this behaviour with the org.freedesktop.Tracker.Miner.Files
`ignored-directories-with-content` GSettings key.

## When I search, I don't see all the results I expect. Why?

If a file doesn't appear in search results, first check that the location is
indexed. From the commandline, you can run:

    $ tracker3 info $FILENAME

This will show all the information stored about the file, if there is any.

If the file should be indexed but nothing is shown, you can check
if there was an error while indexing. Use this command:

    $ tracker3 status $FILENAME

## Why does Tracker consume resources on my PC?

Memory management in Linux is [complex](http://opsmonkey.blogspot.com/2007/01/linux-memory-overcommit.html).
It's normal that processes may appear to use 1GB or more of RAM -- this number
doesn't directly correspond to physical memory, and it's only a problem if you
get to a low-memory situation and the kernel is unable to
reclaim the memory. Tracker Miner FS integrates with the [low-memory-monitor service](https://www.hadess.net/2019/08/low-memory-monitor-new-project.html)
when it's available, and has a [30 second timeout](https://gitlab.gnome.org/GNOME/tracker-miners/-/commit/ccb0b4ebbff4dfacf17ea67ce56bb27c39741811)
that asks the system to reclaim the spare memory when indexing is finished.

When you add or edit files, Tracker Miner FS will update its index. This should
be very quick, but if a huge number of files are added then it may cause
noticably high CPU and IO usage until the new files have been indexed. This is
normal.

Tracker is not designed to index directories of source code or video game data.
If content like this appears in the locations it is configured to index
then you might see unwanted high resource usage.

If you see high resource usage from Tracker Miner FS even when no files have
changed on disk, this probably indicates a bug in Tracker or one of its
dependencies.
See [How can I help debug problems with Tracker services?](#how-can-i-help-debug-problems-with-tracker-services).

## How can I disable Tracker in GNOME?

Tracker is a core dependency of GNOME, and some things will not work as
expected if you disable it completely.

If you are experiencing performance problems, see [Why does Tracker consume
resources on my PC?].

In case of a bug you may need to temporarily stop Tracker Miner FS indexing.
The simplest way is to [edit the
configuration](#how-can-i-control-what-tracker-indexes) so that no directories
are indexed. This should bring resource usage to zero.

If the Tracker Miner FS database is using a lot of disk space, you can run
`tracker3 reset --filesystem` to delete everything stored there.

## Can Tracker help me organize my music collection?

At present, Tracker simply reads the metadata stores in your music files (often
called 'tags').

You may be able to use Gnome Music to correct this metadata using the
Musicbrainz database.

Programs that fix tags and organize music collections on disk, such as
[Beets](http://beets.io/), work well with Tracker.

## How can I help debug problems with Tracker services?

We are always happy when someone wants to help with development, so [tell us
what you're debugging](../community) and you might get useful pointers.

If you see prolonged spikes in CPU and/or IO usage, you can use these commands
to find out what the Tracker daemons are doing. You'll need to use a Terminal
program to run these commands.

  * **Check Tracker daemon status**: You can view live status messages from
    Tracker daemons by running: `tracker3 status`. This may explain the current
    status of the daemon, and the problems it found. The command:
    `tracker3 daemon status --follow` may also be useful to see what the
    daemon is doing currently.

  * **Check the system log**: You can view error logs from Tracker daemons by
    running this command:

        journalctl --user --unit=tracker-miner-fs-3.service --unit=tracker-extract-3.service --priority=7
      
  * **Run the daemon manually**: To see debug output from a Tracker daemon,
    you'll need to kill the running process and start a new one from the
    console with appropriate flags. On a systemd system, you can do this:

        systemctl mask --user --now tracker-miner-fs-3.service
    
    Then, start the Tracker service manually, setting appropriate
    TRACKER_DEBUG settings.

        env TRACKER_DEBUG=config,sparql /usr/libexec/tracker-miner-fs-3

    You can also use debugging tools like GDB in this way. See the
    [HACKING.md file](https://gitlab.gnome.org/GNOME/tracker/-/blob/master/HACKING.md)
    for more details!


[Why does Tracker consume resources on my PC?]: #why-does-tracker-consume-resources-on-my-pc
[control what Tracker indexes]: #how-can-i-control-what-tracker-indexes

## Which versions of Tracker are supported upstream?

Tracker developers advise all users and distributors to use the latest stable
release of Tracker. The behavior of older stable releases staying correct and
stable can not be guaranteed, thus they become unsupported.

There are two main reasons for this:

  * The seccomp jail set up in `tracker-extract-3` will catch non-observed syscalls
    and make the process quit. However updates in any of the dependencies used for
    metadata extraction (or any of their subdependencies) may introduce the usage
    of different syscalls that might not be observed by the seccomp jail.

    This may happen between micro release updates, or due to different compilation
    flags.

  * SQLite has a history of API/ABI breaks and other regressions. This may sound
    anecdotal and unlikely, but Tracker uses SQLite API and logic much more
    extensively than most other users, there is a close to 100% chance that these
    will affect Tracker in some way.

    This most often happens between major releases, but distributors also do have
    a history to push these major version updates to stable/LTS distributions.

The implications of both is the same, handling those situations do not just
require incessant updates, but also require active attention. Tracker maintainers
backport these fixes on a best effort basis, but do not have the bandwidth to
test all combinations induced by different distributions/versions across
multiple Tracker branches.
