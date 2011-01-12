/*
 * Copyright (C) 2008-2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

[DBus (name = "org.freedesktop.Tracker1.Status")]
public class Tracker.Status : Object {
	public const string PATH = "/org/freedesktop/Tracker1/Status";

	const int PROGRESS_TIMEOUT_S = 5;

	class WaitContext : Object {
		public SourceFunc callback;
	}

	double _progress;
	string status = "Idle";
	uint timer_id;
	List<WaitContext> wait_list;

	/**
	 * TrackerStatus::progress:
	 * @notifier: the TrackerStatus
	 * @status: store status
	 * @progress: a #gdouble indicating store progress, from 0 to 1.
	 *
	 * the ::progress signal will be emitted by TrackerStatus
	 * to indicate progress about the store process. @status will
	 * contain a translated string with the current status and @progress
	 * will indicate how much has been processed so far.
	 **/
	public signal void progress (string status, double progres);

	~Status () {
		if (timer_id != 0) {
			Source.remove (timer_id);
		}
	}

	bool
	busy_notification_timeout () {
		progress (status, _progress);

		timer_id = 0;

		return false;
	}

	static bool first_time = true;

	void callback (string status, double progress) {
		this._progress = progress;

		if (progress == 1 && wait_list != null) {
			/* notify clients that tracker-store is no longer busy */

			wait_list.reverse ();
			foreach (var context in wait_list) {
				context.callback ();
			}

			wait_list = null;
		}

		if (status != this.status) {
			this.status = status;
		}

		if (timer_id == 0) {
			if (first_time) {
				this.timer_id = Idle.add (busy_notification_timeout);
				first_time = false;
			} else {
				timer_id = Timeout.add_seconds (PROGRESS_TIMEOUT_S, busy_notification_timeout);
			}
		}

		while (MainContext.default ().iteration (false)) {
		}
	}

	[DBus (visible = false)]
	public BusyCallback get_callback () {
		return callback;
	}

	public double get_progress  () {
		return this._progress;
	}

	public string get_status  () {
		return this.status;
	}

	public async void wait () throws Error {
		if (_progress == 1) {
			/* tracker-store is idle */
		} else {
			var context = new WaitContext ();
			context.callback = wait.callback;
			wait_list.prepend (context);
			yield;
		}
	}
}
