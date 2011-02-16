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
	public bool show_row_hint { get; set; }
	public bool show_subtext { get; set; }
	public bool show_fixed_height { get; set; }
	private bool is_selected;
	private bool is_valid;
	private int fixed_height = -1;

	public override void get_size (Gtk.Widget     widget,
	                               Gdk.Rectangle? cell_area,
	                               out int        x_offset,
	                               out int        y_offset,
	                               out int        width,
	                               out int        height) {
		// First time only, get the minimum fixed height we can use
		if (fixed_height == -1) {
			Pango.Context c = widget.get_pango_context ();
			Pango.Layout layout = new Pango.Layout (c);

			var style = widget.get_style ();
			Pango.FontDescription fd = style.font_desc;

			layout.set_text ("Foo\nBar", -1);
			layout.set_font_description (fd);
			layout.get_pixel_size (null, out fixed_height);
		}

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

		uint start = 0;

		if (text != null) {
			start = (uint) text.length;
		}

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

		// Force all renderers to be the same height so subtext doesn't make some
		// rows look inconsistent with others height wise.
		if (show_fixed_height) {
			this.height = fixed_height;
		} else {
			this.height = -1;
		}

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
