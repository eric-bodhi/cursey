//! Ex (":") commands. To add one, write a handler here and add a row to the
//! `COMMANDS` table.

use crate::editor::Editor;

struct ExArgs<'a> {
    bang: bool,   // "q!" / "e!"
    arg: &'a str, // everything after the command name, trimmed
}

struct ExCommand {
    name: &'static str, // full name, e.g. "write"
    min_len: usize,     // shortest accepted abbreviation, e.g. 1 for "w"
    run: fn(&mut Editor, &ExArgs),
}

fn cmd_write(editor: &mut Editor, args: &ExArgs) {
    editor.write(args.arg);
}

fn cmd_quit(editor: &mut Editor, args: &ExArgs) {
    editor.quit(args.bang);
}

fn cmd_write_quit(editor: &mut Editor, args: &ExArgs) {
    if editor.write(args.arg) {
        editor.quit(args.bang);
    }
}

/// :x - like :wq, but only writes when there are changes.
fn cmd_exit(editor: &mut Editor, args: &ExArgs) {
    if editor.buffer().is_modified() && !editor.write(args.arg) {
        return;
    }
    editor.quit(args.bang);
}

fn cmd_edit(editor: &mut Editor, args: &ExArgs) {
    editor.edit(args.arg, args.bang);
}

fn cmd_set(editor: &mut Editor, args: &ExArgs) {
    if args.arg.is_empty() {
        editor.show_error("E471: Argument required");
        return;
    }
    let mut shown = None;
    // Several options may be given at once: ":set ts=8 noet".
    for option in args.arg.split(' ').filter(|s| !s.is_empty()) {
        match editor.options_mut().set(option) {
            Ok(Some(text)) => shown = Some(text),
            Ok(None) => {}
            Err(error) => {
                editor.show_error(error);
                return;
            }
        }
    }
    if let Some(text) = shown {
        editor.show_message(text);
    }
}

static COMMANDS: [ExCommand; 6] = [
    ExCommand {
        name: "write",
        min_len: 1,
        run: cmd_write,
    },
    ExCommand {
        name: "quit",
        min_len: 1,
        run: cmd_quit,
    },
    ExCommand {
        name: "wq",
        min_len: 2,
        run: cmd_write_quit,
    },
    ExCommand {
        name: "xit",
        min_len: 1,
        run: cmd_exit,
    },
    ExCommand {
        name: "edit",
        min_len: 1,
        run: cmd_edit,
    },
    ExCommand {
        name: "set",
        min_len: 2,
        run: cmd_set,
    },
];

fn find_command(name: &str) -> Option<&'static ExCommand> {
    COMMANDS
        .iter()
        .find(|cmd| name.len() >= cmd.min_len && cmd.name.starts_with(name))
}

/// Run one command line, without the leading ':' (e.g. "wq", "set ts=8",
/// "42").
pub fn execute(editor: &mut Editor, line: &str) {
    let mut line = line.trim();
    while let Some(rest) = line.strip_prefix(':') {
        line = rest.trim();
    }
    if line.is_empty() {
        return;
    }
    let not_a_command = || format!("E492: Not an editor command: {line}");

    // ":42" jumps to line 42, ":$" to the last line.
    if line == "$" {
        let last = editor.buffer().last_row();
        editor.goto_line(last);
        return;
    }
    if line.starts_with(|c: char| c.is_ascii_digit()) {
        if !line.bytes().all(|b| b.is_ascii_digit()) {
            editor.show_error(not_a_command());
            return;
        }
        let n = line.bytes().fold(0usize, |n, b| {
            (n * 10 + usize::from(b - b'0')).min(1_000_000_000)
        });
        editor.goto_line(n.saturating_sub(1));
        return;
    }

    let name_end = line
        .find(|c: char| !c.is_ascii_alphabetic())
        .unwrap_or(line.len());
    let (name, rest) = line.split_at(name_end);
    let (bang, rest) = match rest.strip_prefix('!') {
        Some(rest) => (true, rest),
        None => (false, rest),
    };
    let args = ExArgs {
        bang,
        arg: rest.trim(),
    };

    let Some(cmd) = find_command(name) else {
        editor.show_error(not_a_command());
        return;
    };
    (cmd.run)(editor, &args);
}
