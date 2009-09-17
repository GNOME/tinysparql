using Panel;

public class MainApplet : Panel.Applet {

    public static bool factory (Applet applet, string iid) {
        ((MainApplet) applet).create ();
        return true;
    }

    private void create () {
        var label = new Gtk.Label ("Search:");
        var entry = new Gtk.Entry ();
        var hbox = new Gtk.HBox (false, 6);

        hbox.add (label);
        hbox.add (entry);
        add (hbox);

        string menu_definition = 
            "<popup name=\"button3\">" +
                "<menuitem debuname=\"About\" verb=\"About\" _label=\"_About...\" pixtype=\"stock\" pixname=\"gnome-stock-about\"/>" +
            "</popup>";

        var verb = BonoboUI.Verb ();
        verb.cname = "About";
        verb.cb = on_about_clicked;

        var verbs = new BonoboUI.Verb[] { verb };
        setup_menu (menu_definition, verbs, null);

        show_all();
    }
        
    private static void on_about_clicked (BonoboUI.Component component,
                                          void* user_data, string cname) {
        var dialog = new Gtk.MessageDialog (
            null,
            Gtk.DialogFlags.DESTROY_WITH_PARENT,
            Gtk.MessageType.ERROR,
            Gtk.ButtonsType.CLOSE,
            "About");
        dialog.secondary_text = "About dialog";
        dialog.run ();
        dialog.destroy ();
    }

    public static int main (string[] args) {
        var program = Gnome.Program.init ("GNOME_Search_Bar_Applet", "0", Gnome.libgnomeui_module,
                                          args, "sm-connect", false);
        return Applet.factory_main ("OAFIID:GNOME_Search_Bar_Applet_Factory",
                                    typeof (MainApplet), factory);
    }
}

