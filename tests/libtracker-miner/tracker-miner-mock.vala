//
// Copyright (C) 2010, Nokia
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

using GLib;

public class TrackerMinerMock : GLib.Object {

    public bool is_paused ;
    public string pause_reason { get; set; default = ""; }
    public string name { get; set; default = ""; } 
    public string[] apps { get { return _apps; } }
    public string[] reasons { get { return _apps; } }

    public signal void progress (string miner, string status, double progress);
    public signal void paused ();
    public signal void resumed ();

    string[] _apps;
    string[] _reasons;

    public TrackerMinerMock (string name) {
        this.name = name;
        this._apps = {};
        this._reasons = {};
    }

    public void set_paused (bool paused) { this.is_paused = paused; }
    public bool get_paused () { return this.is_paused ; }

    public void pause (string app, string reason) {

        if (this._apps.length == 0) {
            this._apps = { app };
        } else {
            this._apps += app;
        }

        if (this._reasons.length == 0) {
            this._reasons = { reason };
        } else {
            this._reasons += reason;
        }
        this.is_paused = true;
        this.paused ();
    }

    public void resume () {
        this._apps = null;
        this._reasons = null;
        this.is_paused = false;
        this.resumed ();
    }

}
