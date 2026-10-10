//! Debug logger. Disabled unless the CURSEY_LOG environment variable names a
//! file to write to, e.g. `CURSEY_LOG=/tmp/cursey.log cargo run -- file.txt`.
//! (The editor owns the terminal, so printing to stdout/stderr is not an
//! option while it runs.)

use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

fn log_file() -> Option<&'static Mutex<File>> {
    static FILE: OnceLock<Option<Mutex<File>>> = OnceLock::new();
    FILE.get_or_init(|| {
        let path = std::env::var_os("CURSEY_LOG").filter(|p| !p.is_empty())?;
        let file = OpenOptions::new().create(true).append(true).open(path);
        file.ok().map(Mutex::new)
    })
    .as_ref()
}

/// Append `message` to the log, prefixed with the Unix time in seconds.
pub fn log_debug(message: &str) {
    let Some(file) = log_file() else {
        return;
    };
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_or(0, |d| d.as_secs());
    if let Ok(mut file) = file.lock() {
        // Nothing useful to do if logging itself fails.
        let _ = writeln!(file, "[{secs}] {message}");
    }
}
