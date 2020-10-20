# FAQ

## What is Tracker?

It's a search engine, and a database.

Tracker Miner FS indexes content from your home directory automatically, so
applications can provide instant search results when you need them.

See the [overview](overview) for more information.

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

When you add or edit files, Tracker Miner FS will update its index. This should
be very quick, but if a huge number of files are added then it may cause
noticably high CPU and IO usage until the new files have been indexed. This is
normal.

Tracker is not designed to index directories of source code or video game data.
If content like this appears in the locations it is configured to index
then you might see unwanted high resource usage.

If you see high resource usage from Tracker Miner FS even when no files have
changed on disk, this probably indicates a bug in Tracker or one of its
dependencies.  We can [work together](community) to find out what the problem is.

## How can I disable Tracker in GNOME?

Tracker is a core dependency of GNOME, and some things will not work as
expected if you disable it completely.

If you are experiencing performance problems, see [Why does Tracker consume
resources on my PC?](#why-does-tracker-consume-resources-on-my-pc).

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

[control what Tracker indexes]: #how-can-i-control-what-tracker-indexes
