[indent=4]

/*
 * Copyright (C) 2009, Jamie McCracken (jamiecc at gnome org)
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
    Cairo
    TrackerUtils


class TrackerMetadataTile : EventBox
    uri : string
    category : Categories
    image : Image
    name_link : LinkButton
    table : Table


    /* metadata fields */
    info_label1 : Label
    info_value1 : Label

    info_label2 : Label
    info_value2 : Label

    info_label3 : Label
    info_value3 : Label

    info_label4 : Label
    info_value4 : Label

    info_label5 : Label
    info_value5 : Label

    info_label6 : Label
    info_value6 : Label

    info_label7 : Label
    info_value7 : Label

    info_label8 : Label
    info_value8 : Label

    _result_grid : TrackerResultGrid


    prop Query : TrackerQuery

    prop ResultGrid : TrackerResultGrid
        get
            return _result_grid
        set
            if value is not null
                _result_grid = value

                _result_grid.SelectionChanged += def (path)
                    LoadUri (path)


    init

        set_app_paintable (true)
        
        expose_event += expose
   
        border_width = 1

        table = new Table (3, 7, false)
        table.set_col_spacings (6)
        table.set_row_spacings (6)

        add (table)

        image = new Image.from_icon_name ("text-x-generic", IconSize.DIALOG)
        image.set_pixel_size (62)
        table.attach (image, 0, 1, 0, 3, AttachOptions.FILL, AttachOptions.FILL, 12, 0)

        name_link = new LinkButton ("")
        name_link.xalign = 0
        table.attach (name_link, 1, 7, 0, 1, AttachOptions.FILL, AttachOptions.FILL, 0, 0)

        info_label1 = CreateLabel (N_("Type:"), false)
        AttachToTable (info_label1, 1, 2, 1, 2, false)

        info_value1 = CreateLabel ("-", true)
        AttachToTable (info_value1, 2, 3, 1, 2, true)

        info_label2 = CreateLabel (N_("Size:"), false)
        AttachToTable (info_label2, 3, 4, 1, 2, false)

        info_value2 = CreateLabel ("-", true)
        AttachToTable (info_value2, 4, 5, 1, 2, true)

        info_label3 = CreateLabel (N_("Modified:"), false)
        AttachToTable (info_label3, 5, 6, 1, 2, false)

        info_value3 = CreateLabel ("-", true)
        AttachToTable (info_value3, 6, 7, 1, 2, true)

        info_label4 = CreateLabel (N_("Title:"), false)
        AttachToTable (info_label4, 1, 2, 2, 3, false)

        info_value4 = CreateLabel ("-", true)
        AttachToTable (info_value4, 2, 3, 2, 3, true)

        info_label5 = CreateLabel (N_("Author/Artist:"), false)
        AttachToTable (info_label5, 3, 4, 2, 3, false)

        info_value5 = CreateLabel ("-", true)
        AttachToTable (info_value5, 4, 5, 2, 3, true)

        info_label6 = CreateLabel ("Comments:", false)
        AttachToTable (info_label6, 5, 6, 2, 3, false)

        info_value6 = CreateLabel ("-", true)
        AttachToTable (info_value6, 6, 7, 2, 3, true)

        //show_all ()

    def private expose (e : Gdk.EventExpose) : bool
    
        var cr = Gdk.cairo_create (self.window)

        var style = self.get_style ()
        var step1 = style.base [StateType.NORMAL]
        var step2 = style.bg [StateType.SELECTED]
        
        w,h : double
        w = self.allocation.width
        h = self.allocation.height

	    /* clear window to base[NORMAL] */
        cr.set_operator (Operator.SOURCE)
        Gdk.cairo_set_source_color (cr, step1)
        cr.paint ()
        cr.move_to (0, 0)
        cr.set_line_width (1.0)
        cr.set_operator (Operator.OVER)

        /* main gradient */
        var pat = new Pattern.linear (0.0, 0.0, 0.0, h)
        
        pat.add_color_stop_rgba (0.0, step2.red/65535.0,
	                                  step2.green/65535.0,
	                                  step2.blue/65535.0,
	                                  0.05)
	                                   
        pat.add_color_stop_rgba (1.0, step2.red/65535.0,
	                                  step2.green/65535.0,
	                                  step2.blue/65535.0,
	                                  0.5)

        cr.rectangle (0, 0, w, h)
        cr.set_source (pat)
        cr.fill ()
	
        /* border line */
        cr.set_source_rgba (step2.red/65535.0,
	                        step2.green/65535.0,
	                        step2.blue/65535.0,
	                        0.7)
        cr.move_to (0, 0)
        cr.line_to (w, 0)
        cr.stroke ()

        /* highlight line */
        cr.set_source_rgba (1.0, 1.0, 1.0, 0.5)
        cr.move_to (0, 1)
        cr.line_to (w, 1)
        cr.stroke ()

        return super.expose_event (e) 


    def private AttachToTable (lab : Label, l : int, r : int, t : int, b : int, e : bool)
        if e is true
            table.attach (lab, l, r, t, b, AttachOptions.FILL | AttachOptions.EXPAND , AttachOptions.FILL, 0, 0)
        else
            table.attach (lab, l, r, t, b, AttachOptions.FILL, AttachOptions.FILL, 0, 0)

    def private CreateLabel (s : string, e : bool) : Label
        var l = new Label (s)
        l.xalign = 0
        l.set_use_markup (true)

        if e is true
            l.ellipsize = Pango.EllipsizeMode.END

        return l


    def ClearLabels ()
        info_value1.set_text ("")
        info_value2.set_text ("")
        info_value3.set_text ("")
        info_value4.set_text ("")
        info_value5.set_text ("")
        info_value6.set_text ("")
        name_link.uri = ""
        name_link.label = ""




    def LoadUri (path : TreePath?)
        ClearLabels ()

        if path is null
            image.set_from_icon_name ("text-x-generic", IconSize.DIALOG)
            return

        iter : TreeIter
        uri : weak string
        display_name : weak string
        icon : Gdk.Pixbuf

        _result_grid.store.get_iter (out iter, path)
        _result_grid.store.get (iter, ResultColumns.Uri, out uri, ResultColumns.Icon, out icon, ResultColumns.DisplayName, out display_name)

        image.set_from_pixbuf (icon)

        var file = File.new_for_uri (uri)
        var filepath = file.get_basename ()
        name_link.uri = uri
        name_link.label = filepath

        // get metadata
        // var query = "SELECT ?mimetype ?size ?mtime WHERE {<%s> nie:byteSize ?size; nie:contentLastModified ?mtime; nie:mimeType ?mimeType.}".printf(uri)
        var query = "SELECT ?mimetype WHERE {<%s> nie:mimeType ?mimetype.}".printf(uri)
        if Query is not null
            var result = Query.Query (query)

            if result is not null and  result [0,0] is not null
                var val1 = "<b>%s</b>".printf (result [0,0])

                info_value1.set_markup (val1)
                info_value1.xalign = 0

            try
                var info =  file.query_info ("standard::size,time::modified", \
                                              FileQueryInfoFlags.NONE, null)

                var val2 = "<b>%s</b>".printf (info.get_size ().to_string ())

                info_value2.set_markup (val2)

                tm : TimeVal
                info.get_modification_time (out tm)

                var val3 = "<b>%s</b>".printf (tm.to_iso8601 ())

                info_value3.set_markup (val3)

            except e:Error
                print "Could not get file info for %s", uri
