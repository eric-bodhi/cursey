mod common;

use common::{Session, pos, run, temp_path};
use cursey_core::buffer::Buffer;
use cursey_core::editor::Editor;
use cursey_core::keys;
use cursey_core::types::Mode;
use std::fs;

// ---- Movement ------------------------------------------------------------

#[test]
fn editor_hjkl() {
    let mut s = Session::new("abc\ndef\nghi");
    s.keys("jl");
    assert_eq!(s.cursor(), pos(1, 1));
    s.keys("kh");
    assert_eq!(s.cursor(), pos(0, 0));
    s.keys("h"); // cannot go further left
    assert_eq!(s.cursor(), pos(0, 0));
    s.keys("llll"); // stops on the last character
    assert_eq!(s.cursor(), pos(0, 2));
}

#[test]
fn editor_counts() {
    let mut s = Session::new("0123456789\na\nb\nc\nd");
    s.keys("5l");
    assert_eq!(s.cursor(), pos(0, 5));
    s.keys("3j");
    assert_eq!(s.cursor(), pos(3, 0));
}

#[test]
fn editor_vertical_motion_keeps_column() {
    let mut s = Session::new("long line here\nab\nanother long line");
    s.keys("10l");
    s.keys("j");
    assert_eq!(s.cursor(), pos(1, 1)); // clamped on the short line
    s.keys("j");
    assert_eq!(s.cursor(), pos(2, 10)); // column restored
}

#[test]
fn editor_dollar_sticks_to_end_of_line() {
    let mut s = Session::new("abc\nlonger line\nxy");
    s.keys("$j");
    assert_eq!(s.cursor(), pos(1, 10));
    s.keys("j");
    assert_eq!(s.cursor(), pos(2, 1));
}

#[test]
fn editor_line_motions() {
    let mut s = Session::new("   indented text");
    s.keys("$");
    assert_eq!(s.cursor(), pos(0, 15));
    s.keys("0");
    assert_eq!(s.cursor(), pos(0, 0));
    s.keys("^");
    assert_eq!(s.cursor(), pos(0, 3));
}

#[test]
fn editor_gg_and_g() {
    let mut s = Session::new("a\n  b\nc\nd");
    s.keys("G");
    assert_eq!(s.cursor(), pos(3, 0));
    s.keys("gg");
    assert_eq!(s.cursor(), pos(0, 0));
    s.keys("2G");
    assert_eq!(s.cursor(), pos(1, 2));
    s.keys("3gg");
    assert_eq!(s.cursor(), pos(2, 0));
}

#[test]
fn editor_find_char() {
    let mut s = Session::new("a,b,c,d");
    s.keys("f,");
    assert_eq!(s.cursor(), pos(0, 1));
    s.keys(";");
    assert_eq!(s.cursor(), pos(0, 3));
    s.keys(",");
    assert_eq!(s.cursor(), pos(0, 1));
    s.keys("$F,");
    assert_eq!(s.cursor(), pos(0, 5));
    s.keys("0t,;");
    assert_eq!(s.cursor(), pos(0, 2));
}

#[test]
fn editor_search() {
    let mut s = Session::new("one\ntwo foo\nthree foo\nfour");
    s.keys("/foo<CR>");
    assert_eq!(s.cursor(), pos(1, 4));
    s.keys("n");
    assert_eq!(s.cursor(), pos(2, 6));
    s.keys("n"); // wraps around
    assert_eq!(s.cursor(), pos(1, 4));
    assert!(!s.editor.message().is_empty());
    s.keys("N");
    assert_eq!(s.cursor(), pos(2, 6));
    s.keys("gg?two<CR>");
    assert_eq!(s.cursor(), pos(1, 0));
}

#[test]
fn editor_search_not_found() {
    let mut s = Session::new("abc");
    s.keys("/zzz<CR>");
    assert!(s.editor.message_is_error());
    assert_eq!(s.cursor(), pos(0, 0));
}

// ---- Insert mode ---------------------------------------------------------

#[test]
fn editor_insert_and_escape() {
    let mut s = Session::new("world");
    s.keys("ihello <Esc>");
    assert_eq!(s.text(), "hello world");
    assert_eq!(s.editor.mode(), Mode::Normal);
    assert_eq!(s.cursor(), pos(0, 5)); // Esc moves left one
}

#[test]
fn editor_append_variants() {
    assert_eq!(run("abc", "aX<Esc>"), "aXbc");
    assert_eq!(run("abc", "AX<Esc>"), "abcX");
    assert_eq!(run("  abc", "$IX<Esc>"), "  Xabc");
    assert_eq!(run("", "aX<Esc>"), "X");
}

#[test]
fn editor_open_line() {
    assert_eq!(run("a\nc", "ob<Esc>"), "a\nb\nc");
    assert_eq!(run("b\nc", "Oa<Esc>"), "a\nb\nc");
    // Autoindent copies the current line's indentation.
    assert_eq!(run("    a", "ob<Esc>"), "    a\n    b");
}

#[test]
fn editor_insert_enter_and_backspace() {
    assert_eq!(run("abcd", "lli<CR><Esc>"), "ab\ncd");
    assert_eq!(run("ab\ncd", "ji<BS><Esc>"), "abcd");
    assert_eq!(run("abc", "A<BS><BS><Esc>"), "a");
    assert_eq!(run("ab\ncd", "A<Del><Esc>"), "abcd");
}

#[test]
fn editor_insert_tab_expands() {
    assert_eq!(run("x", "i<Tab><Esc>"), "    x");
    let mut s = Session::new("x");
    s.editor.options_mut().expandtab = false;
    s.keys("i<Tab><Esc>");
    assert_eq!(s.text(), "\tx");
}

#[test]
fn editor_insert_utf8() {
    let mut s = Session::new("ab");
    s.keys("aé<Esc>");
    assert_eq!(s.text(), "aéb");
    assert_eq!(s.cursor(), pos(0, 1));
    s.keys("lx");
    assert_eq!(s.text(), "aé");
}

// ---- Operators -----------------------------------------------------------

#[test]
fn editor_delete_word() {
    assert_eq!(run("foo bar baz", "dw"), "bar baz");
    assert_eq!(run("foo bar baz", "2dw"), "baz");
    assert_eq!(run("foo bar baz", "d2w"), "baz");
    // The last word on a line: stop at the end of the line.
    assert_eq!(run("foo bar\nnext", "wdw"), "foo \nnext");
}

#[test]
fn editor_delete_motions() {
    assert_eq!(run("foo bar baz", "wde"), "foo  baz");
    assert_eq!(run("foo bar baz", "$db"), "foo bar z");
    assert_eq!(run("foo bar baz", "wd$"), "foo ");
    assert_eq!(run("foo bar baz", "wD"), "foo ");
    assert_eq!(run("foo bar baz", "$d0"), "z");
    assert_eq!(run("a,b,c", "dt,"), ",b,c");
    assert_eq!(run("a,b,c", "df,"), "b,c");
}

#[test]
fn editor_delete_lines() {
    assert_eq!(run("a\nb\nc", "dd"), "b\nc");
    assert_eq!(run("a\nb\nc", "jdd"), "a\nc");
    assert_eq!(run("a\nb\nc", "Gdd"), "a\nb");
    assert_eq!(run("a\nb\nc", "2dd"), "c");
    assert_eq!(run("a\nb\nc", "dj"), "c");
    assert_eq!(run("a\nb\nc", "Gdk"), "a");
    assert_eq!(run("a\nb\nc", "dG"), "");
    assert_eq!(run("a\nb\nc", "jdgg"), "c");
    assert_eq!(run("only", "dd"), "");
}

#[test]
fn editor_dd_cursor_position() {
    let mut s = Session::new("a\n  b\nc");
    s.keys("dd");
    assert_eq!(s.cursor(), pos(0, 2)); // first non-blank of new line
    s.keys("Gdd");
    assert_eq!(s.cursor(), pos(0, 2));
}

#[test]
fn editor_x_and_capital_x() {
    assert_eq!(run("abc", "x"), "bc");
    assert_eq!(run("abc", "3x"), "");
    assert_eq!(run("abc", "$x"), "ab");
    assert_eq!(run("abc", "$X"), "ac");
    assert_eq!(run("", "x"), "");
    let mut s = Session::new("abc");
    s.keys("$x");
    assert_eq!(s.cursor(), pos(0, 1)); // stays on a character
}

#[test]
fn editor_change() {
    assert_eq!(run("foo bar", "cwbaz<Esc>"), "baz bar");
    assert_eq!(run("foo bar", "c2wx<Esc>"), "x");
    assert_eq!(run("a b", "cwX<Esc>"), "X b"); // single-letter word
    assert_eq!(run("foo bar", "wCx<Esc>"), "foo x");
    assert_eq!(run("foo bar", "sX<Esc>"), "Xoo bar");
    assert_eq!(run("    foo\nbar", "ccX<Esc>"), "    X\nbar");
    assert_eq!(run("foo\nbar", "SX<Esc>"), "X\nbar");
}

#[test]
fn editor_change_is_one_undo_step() {
    let mut s = Session::new("foo bar");
    s.keys("cwhello there<Esc>");
    assert_eq!(s.text(), "hello there bar");
    s.keys("u");
    assert_eq!(s.text(), "foo bar");
}

#[test]
fn editor_yank_and_paste() {
    assert_eq!(run("foo bar", "ywP"), "foo foo bar");
    assert_eq!(run("foo bar", "yw$p"), "foo barfoo ");
    assert_eq!(run("a\nb", "yyp"), "a\na\nb");
    assert_eq!(run("a\nb", "yyjP"), "a\na\nb");
    assert_eq!(run("a\nb", "Yjp"), "a\nb\na");
    assert_eq!(run("a\nb", "yj2p"), "a\na\nb\na\nb\nb");
    assert_eq!(run("ab", "xp"), "ba");
    assert_eq!(run("a\nb", "ddp"), "b\na");
}

#[test]
fn editor_yank_does_not_modify() {
    let mut s = Session::new("abc");
    s.keys("yy");
    assert!(!s.editor.buffer().is_modified());
}

#[test]
fn editor_named_registers() {
    let mut s = Session::new("one\ntwo");
    s.keys("\"ayyj\"byy");
    s.keys("\"ap\"bp");
    assert_eq!(s.text(), "one\ntwo\none\ntwo");
    // Deleting into the black hole register keeps the unnamed register.
    let mut t = Session::new("keep\ntrash");
    t.keys("yyj\"_ddP");
    assert_eq!(t.text(), "keep\nkeep");
}

#[test]
fn editor_paste_empty_register_errors() {
    let mut s = Session::new("abc");
    s.keys("\"zp");
    assert!(s.editor.message_is_error());
    assert_eq!(s.text(), "abc");
}

// ---- Other commands ------------------------------------------------------

#[test]
fn editor_join() {
    assert_eq!(run("a\n   b", "J"), "a b");
    assert_eq!(run("a\nb\nc", "3J"), "a b c");
    assert_eq!(run("a \nb", "J"), "a b");
    assert_eq!(run("a\n", "J"), "a");
    assert_eq!(run("f(\n)", "J"), "f()");
}

#[test]
fn editor_replace_char() {
    assert_eq!(run("abc", "rx"), "xbc");
    assert_eq!(run("abc", "2rx"), "xxc");
    assert_eq!(run("abc", "5rx"), "abc"); // not enough characters
    assert_eq!(run("abc", "r<Esc>"), "abc");
}

#[test]
fn editor_undo_redo() {
    let mut s = Session::new("abc");
    s.keys("x");
    s.keys("x");
    assert_eq!(s.text(), "c");
    s.keys("u");
    assert_eq!(s.text(), "bc");
    s.keys("u");
    assert_eq!(s.text(), "abc");
    assert!(!s.editor.buffer().is_modified());
    s.keys("<C-r>");
    assert_eq!(s.text(), "bc");
    s.keys("2u");
    assert_eq!(s.text(), "abc");
}

#[test]
fn editor_undo_restores_cursor() {
    let mut s = Session::new("a\nb\nc");
    s.keys("jdd");
    assert_eq!(s.cursor(), pos(1, 0));
    s.keys("ggu");
    assert_eq!(s.text(), "a\nb\nc");
    assert_eq!(s.cursor(), pos(1, 0));
}

#[test]
fn editor_escape_cancels_pending() {
    let mut s = Session::new("abc");
    s.keys("d<Esc>x");
    assert_eq!(s.text(), "bc");
    assert_eq!(s.editor.pending_keys(), "");
    s.keys("2");
    assert_eq!(s.editor.pending_keys(), "2");
}

#[test]
fn editor_invalid_command_is_ignored() {
    let mut s = Session::new("abc");
    s.keys("dQx");
    assert_eq!(s.text(), "bc");
}

// ---- Visual mode ---------------------------------------------------------

#[test]
fn editor_visual_delete() {
    assert_eq!(run("hello world", "vlld"), "lo world");
    assert_eq!(run("hello world", "wvbd"), "orld"); // backwards selection
    assert_eq!(run("ab\ncd\nef", "lvjd"), "a\nef");
}

#[test]
fn editor_visual_line() {
    assert_eq!(run("a\nb\nc\nd", "jVjd"), "a\nd");
    assert_eq!(run("a\nb\nc", "Vjyjjp"), "a\nb\nc\na\nb");
}

#[test]
fn editor_visual_change_and_swap() {
    assert_eq!(run("foo bar", "veciX<Esc>"), "iX bar");
    let mut s = Session::new("abcdef");
    s.keys("lvll");
    s.keys("o");
    assert_eq!(s.cursor(), pos(0, 1));
    s.keys("hd");
    assert_eq!(s.text(), "ef");
}

#[test]
fn editor_visual_escape() {
    let mut s = Session::new("abc");
    s.keys("vl");
    assert!(s.editor.selection().is_some());
    s.keys("<Esc>");
    assert_eq!(s.editor.mode(), Mode::Normal);
    assert!(s.editor.selection().is_none());
    assert_eq!(s.text(), "abc");
}

// ---- Ex commands ---------------------------------------------------------

#[test]
fn editor_goto_line_command() {
    let mut s = Session::new("a\nb\nc\nd");
    s.keys(":3<CR>");
    assert_eq!(s.cursor(), pos(2, 0));
    s.keys(":$<CR>");
    assert_eq!(s.cursor(), pos(3, 0));
}

#[test]
fn editor_quit_refuses_unsaved_changes() {
    let mut s = Session::new("abc");
    s.keys("x:q<CR>");
    assert!(!s.editor.should_quit());
    assert!(s.editor.message_is_error());
    s.keys(":q!<CR>");
    assert!(s.editor.should_quit());
}

#[test]
fn editor_quit_when_clean() {
    let mut s = Session::new("abc");
    s.keys(":q<CR>");
    assert!(s.editor.should_quit());
}

#[test]
fn editor_write_and_quit() {
    let path = temp_path("editor_write_and_quit");
    let path_str = path.to_str().unwrap();
    let _ = fs::remove_file(&path);
    let mut editor = Editor::new();
    editor.open(path_str);
    for k in keys::parse("ihello<Esc>:wq<CR>") {
        editor.handle_key(k);
    }
    assert!(editor.should_quit());
    let mut reread = Buffer::new();
    assert!(reread.load(path_str).is_ok());
    assert_eq!(reread.text(), "hello");
    fs::remove_file(&path).unwrap();
}

#[test]
fn editor_open_warns_about_invalid_utf8() {
    let path = temp_path("editor_invalid_utf8");
    let path_str = path.to_str().unwrap();
    fs::write(&path, b"caf\xe9\n").unwrap();
    let mut editor = Editor::new();
    assert!(editor.open(path_str));
    assert!(editor.message_is_error());
    assert!(editor.message().contains("invalid UTF-8"));
    assert_eq!(editor.buffer().text(), "caf\u{FFFD}");
    fs::remove_file(&path).unwrap();
}

#[test]
fn editor_set_option() {
    let mut s = Session::new("x");
    s.keys(":set ts=8 noet<CR>");
    assert_eq!(s.editor.options().tabstop, 8);
    assert!(!s.editor.options().expandtab);
    s.keys(":set bogus<CR>");
    assert!(s.editor.message_is_error());
    s.keys(":se ts?<CR>");
    assert_eq!(s.editor.message(), "tabstop=8");
}

#[test]
fn editor_unknown_command() {
    let mut s = Session::new("x");
    s.keys(":frobnicate<CR>");
    assert!(s.editor.message_is_error());
}

#[test]
fn editor_command_line_backspace_cancels() {
    let mut s = Session::new("x");
    s.keys(":<BS>");
    assert_eq!(s.editor.mode(), Mode::Normal);
    s.keys(":ab<BS>");
    assert_eq!(s.editor.command_line(), ":a");
}

// ---- Scrolling -----------------------------------------------------------

#[test]
fn editor_scrolls_to_keep_cursor_visible() {
    let text: String = (0..100).map(|i| format!("line {i}\n")).collect();
    let mut s = Session::new(&text);
    s.editor.set_view_size(10, 80);
    s.keys("50G");
    let top = s.editor.view_top();
    assert!(49 >= top && 49 < top + 10);
    s.keys("gg");
    assert_eq!(s.editor.view_top(), 0);
}

#[test]
fn editor_half_page_scroll() {
    let text = "line\n".repeat(100);
    let mut s = Session::new(&text);
    s.editor.set_view_size(20, 80);
    s.keys("<C-d>");
    assert_eq!(s.cursor().row, 10);
    s.keys("<C-u>");
    assert_eq!(s.cursor().row, 0);
}
