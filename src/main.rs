mod lex;
mod tui;

use cursey_core::editor::Editor;
use std::io;
use std::process::ExitCode;
use tui::Tui;

fn main() -> ExitCode {
    let args: Vec<_> = std::env::args_os().collect();
    if args.len() > 2 {
        eprintln!("Usage: {} [filename]", args[0].to_string_lossy());
        return ExitCode::FAILURE;
    }

    let mut editor = Editor::new();
    if let Some(path) = args.get(1) {
        let Some(path) = path.to_str() else {
            eprintln!("cursey: file name is not valid UTF-8");
            return ExitCode::FAILURE;
        };
        editor.open(path);
    }

    if let Err(e) = run(&mut editor) {
        eprintln!("cursey: {e}");
        return ExitCode::FAILURE;
    }
    ExitCode::SUCCESS
}

/// Main loop: render -> read key -> editor.handle_key(). The terminal is
/// restored when `tui` is dropped, before main prints any error.
fn run(editor: &mut Editor) -> io::Result<()> {
    let mut tui = Tui::new()?;
    while !editor.should_quit() && !tui.closed() {
        tui.render(editor)?;
        if let Some(key) = tui.read_key() {
            editor.handle_key(key);
        }
    }
    Ok(())
}
