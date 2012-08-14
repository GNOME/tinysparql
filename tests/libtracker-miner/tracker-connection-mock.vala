using GLib;
using Tracker;


public class TrackerMockResults : Tracker.Sparql.Cursor {
	int rows;
	int current_row = -1;
	string[,] results;
	string[] var_names;
	Sparql.ValueType[] types;
	int cols;

	public TrackerMockResults (owned string[,] results, int rows, int cols, string[] var_names, Sparql.ValueType[] types) {
		this.rows = rows;
		this.cols = cols;
		this.results = (owned) results;
		this.types = types;
		this.var_names = var_names;
	}

	public override int n_columns { get { return cols; } }

	public override Sparql.ValueType get_value_type (int column)
	requires (current_row >= 0) {
		return this.types[column];
	}

	public override unowned string? get_variable_name (int column)
	requires (current_row >= 0) {
		return this.var_names[column];
	}

	public override unowned string? get_string (int column, out long length = null)
	requires (current_row >= 0) {
		unowned string str;

		str = results[current_row, column];

		length = str.length;

		return str;
	}

	public override bool next (Cancellable? cancellable = null) throws GLib.Error {
		if (current_row >= rows - 1) {
			return false;
		}
		current_row++;
		return true;
	}

	public override async bool next_async (Cancellable? cancellable = null) throws GLib.Error {
		/* This cursor isn't blocking, it's fine to just call next here */
		return next (cancellable);
	}

	public override void rewind () {
		current_row = 0;
	}
}




public class TrackerMockConnection : Sparql.Connection {

    TrackerMockResults results = null;
    TrackerMockResults hardcoded = new TrackerMockResults ({{"11", "12"}, {"21", "22"}}, 2, 2,
                                                           {"artist", "album"},
                                                           {Sparql.ValueType.STRING, Sparql.ValueType.STRING});

	public override Sparql.Cursor query (string sparql,
                                  Cancellable? cancellable = null)
    throws Sparql.Error, IOError, DBusError {
        if (this.results != null) {
            return results;
        } else {
            return hardcoded;
        }
    }


	public async override Sparql.Cursor query_async (string sparql, Cancellable? cancellable = null)
    throws Sparql.Error, IOError, DBusError {
        if (this.results != null) {
            return results;
        } else {
            return hardcoded;
        }
    }


    public void set_results (TrackerMockResults results) {
        this.results = results;
    }

}
