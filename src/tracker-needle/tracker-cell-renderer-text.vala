//
// Copyright 2010, Martyn Russell <martyn@lanedo.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//

class Tracker.CellRendererText : Gtk.CellRendererText {
	// FIXME: Chain text and set is_valid to false when changes
	//public string text { get; set; }
	public string subtext { get; set; }
	public bool show_subtext { get; set; }
	private bool is_selected;
	private bool is_valid;

	public override void get_size (Gtk.Widget     widget,
	                               Gdk.Rectangle? cell_area,
	                               out int        x_offset,
	                               out int        y_offset,
	                               out int        width,
	                               out int        height) {
		update_text (widget, is_selected);

		base.get_size (widget, cell_area, out x_offset, out y_offset, out width, out height);
	}
	
	public override void render (Gdk.Window            window,
	                             Gtk.Widget            widget,
	                             Gdk.Rectangle         background_area,
	                             Gdk.Rectangle         cell_area,
	                             Gdk.Rectangle         expose_area,
	                             Gtk.CellRendererState flags) {
		bool selected = Gtk.CellRendererState.SELECTED in flags;

		update_text (widget, selected);
		
		base.render (window, widget, background_area, cell_area, expose_area, flags);
	}

	private void update_text (Gtk.Widget widget,
	                          bool       selected) {
		if (is_valid && is_selected == selected) {
			return;
		}

		var style = widget.get_style ();
		var attr_list = new Pango.AttrList ();
		uint start = (uint) text.length;
		
//		var attr_style = new Pango.Style (Pango.Style.ITALIC);
//		attr_style.start_index = start
//		attr_style.end_index = -1;
//		attr_list.insert (attr_style);

		if (!selected) {
			var c = style.text_aa[Gtk.StateType.NORMAL];
			var attr_color = Pango.attr_foreground_new (c.red, c.green, c.blue);
			attr_color.start_index = start;
			attr_color.end_index = -1;
			attr_list.insert ((owned) attr_color);
		}

		Pango.FontDescription fd = style.font_desc;
		var attr_size = new Pango.AttrSize ((int) (fd.get_size () / 1.2));
		
		attr_size.start_index = start;
		attr_size.end_index = -1;
		attr_list.insert ((owned) attr_size);

		string str;

		if (subtext == null || subtext.length < 1 || show_subtext) {
			str = text;
		} else {
			str = "%s\n%s".printf (text, subtext);
		}

		debug ("str:'%s'\n", str);
		this.visible = true;
		this.weight = Pango.Weight.NORMAL;
		this.text = str;
		this.attributes = attr_list;
		this.xpad = 0;
		this.ypad = 1;

		is_selected = selected;
		// FIXME: Chain text and set is_valid here to true
		//is_valid = true;
	}
}
