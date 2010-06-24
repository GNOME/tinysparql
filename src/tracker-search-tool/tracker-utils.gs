[indent=4]

/*
 * Copyright (C) 2009, Jamie McCracken (jamiemcc at gnome org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */


uses
    Gtk



enum Categories
    Application
    Contact
    Email
    EmailAttachment
    File
    Folder
    Audio
    Video
    Image
    Document
    Text
    Development
    Web
    WebHistory

[CCode (cname = "TRACKER_UI_DIR")]
const  extern static  UIDIR : string

[CCode (cname = "SRCDIR")]
const  extern static  SRCDIR : string


class TrackerUtils
/* static methods only as this is a utility class that wont ever get substantiated */

    def static EscapeSparql (sparql : string, add_wildcard : bool) : string
        var str = new StringBuilder ()

        if sparql is null
            return ""

        var len = sparql.length

        if len < 3
            return sparql

        p : char*  = sparql

        while *p is not '\0'
            if *p is '"'
                str.append ("\\\"")
            else if *p is '\\'
                str.append ("\\\\")
            else
                str.append_c (*p)
            p++

        if add_wildcard
            p--
            if *p is not ' '
                str.append_c ('*')


        return str.str



    def static LaunchApp (uri : string) : bool
        app_info : AppInfo
        context : AppLaunchContext

        var file = File.new_for_uri (uri)

        app_info = new DesktopAppInfo.from_filename (file.get_path ())
        if app_info is null
            return OpenUri (uri, false)

        context = new Gdk.AppLaunchContext ()
        try
            app_info.launch (null, context)
        except e:Error
            var msg = new MessageDialog (null, DialogFlags.MODAL, MessageType.ERROR, ButtonsType.OK, \
                                         N_("Could not launch \"%s\"\nError: %s\n"), app_info.get_display_name (), e.message)
            msg.run ();
            return false

        return true


    def static OpenUri (uri : string, is_dir :bool) : bool
        app_info : AppInfo

        var file = File.new_for_uri (uri)

        try
            app_info = file.query_default_handler (null)
        except e:Error
            var msg = new MessageDialog (null, DialogFlags.MODAL, MessageType.ERROR, ButtonsType.OK, \
                                         N_("Could not get application info for %s\nError: %s\n"), uri, e.message)
            msg.run ();
            return false

        try
            app_info.launch_default_for_uri (uri, null)
        except e: Error
            var msg = new MessageDialog (null, DialogFlags.MODAL, MessageType.ERROR, ButtonsType.OK, \
                                         N_("Could not lauch %s\nError: %s\n"), uri, e.message)
            msg.run ();
            return false

        return true


    def static inline GetThemePixbufByName (icon_name : string, size : int, screen : Gdk.Screen) :  Gdk.Pixbuf?

        var icon = new ThemedIcon (icon_name);

        return GetThemeIconPixbuf (icon, size, screen)


    def static GetThumbNail (info : FileInfo, thumb_size : int, icon_size : int, screen : Gdk.Screen) : Gdk.Pixbuf?

        pixbuf : Gdk.Pixbuf = null

        try
            var thumbpath = info.get_attribute_byte_string (FILE_ATTRIBUTE_THUMBNAIL_PATH)

            if thumbpath is not null
                pixbuf = new Gdk.Pixbuf.from_file_at_size (thumbpath, thumb_size, thumb_size)
        except e: Error
            pass

        if pixbuf is null
            pixbuf = GetThemeIconPixbuf (info.get_icon (), icon_size, screen)

        if pixbuf is null
            pixbuf = GetThemePixbufByName ("text-x-generic", icon_size, screen)

        return pixbuf


    def static GetThemeIconPixbuf (icon : Icon, size : int, screen : Gdk.Screen) : Gdk.Pixbuf?

        icon_info : IconInfo

        var theme = IconTheme.get_for_screen (screen)

        icon_info = theme.lookup_by_gicon (icon, size, IconLookupFlags.USE_BUILTIN)

        try
            return icon_info.load_icon ()
        except e: Error
            return null
            
            
    /* formatting methods */
    def static FormatFileSize (size : int64) : string
        displayed_size : double
        
        if size < 1024
            return "%u bytes".printf ((uint)size)

        if size < 1048576
            displayed_size = (double) size / 1024
            return "%.1f KB".printf (displayed_size)
            
        if size < 1073741824
            displayed_size = (double) size / 1048576
            return "%.1f MB".printf (displayed_size)
            
        displayed_size = (double) size / 1073741824
        return "%.1f GB".printf (displayed_size)



    
