using GLib;

public class TrackerMinerMock : GLib.Object {

    public bool is_paused ;
    public string pause_reason { get; set; default = ""; }
    public string name { get; set; default = ""; } 
    public string[] apps {get; set; }
    public string[] reasons {get; set; }

    public signal void progress (string miner, string status, double progress);
    public signal void paused ();
    public signal void resumed ();

    public TrackerMinerMock (string name) {
        this.name = name;
        this.apps = {};
        this.reasons = {};
    }

    public void set_paused (bool paused) { this.is_paused = paused; }
    public bool get_paused () { return this.is_paused ; }

    public void pause (string app, string reason) {

        if (this.apps == null) {
            this.apps = { app };
        } else {
            //this.apps += app;
        }

        if (this.reasons == null) {
            this.reasons = { reason };
        } else {
            //this.reasons += reason;
        }
        this.is_paused = true;
        this.paused ();
    }

    public void resume () {
        this.apps = null;
        this.reasons = null;
        this.is_paused = false;
        this.resumed ();
    }

}