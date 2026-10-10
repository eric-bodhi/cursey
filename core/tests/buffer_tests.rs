mod common;

use common::{make_buffer, pos, temp_path};
use cursey_core::buffer::Buffer;
use std::fs;

#[test]
fn buffer_starts_with_one_empty_line() {
    let b = Buffer::new();
    assert_eq!(b.line_count(), 1);
    assert_eq!(b.line(0), "");
    assert!(!b.is_modified());
}

#[test]
fn buffer_insert_single_line() {
    let mut b = make_buffer("hello");
    let end = b.insert(pos(0, 5), " world");
    assert_eq!(b.text(), "hello world");
    assert_eq!(end, pos(0, 11));
}

#[test]
fn buffer_insert_multi_line() {
    let mut b = make_buffer("abcd");
    let end = b.insert(pos(0, 2), "X\nY\nZ");
    assert_eq!(b.text(), "abX\nY\nZcd");
    assert_eq!(end, pos(2, 1));
}

#[test]
fn buffer_erase_across_lines() {
    let mut b = make_buffer("one\ntwo\nthree");
    let removed = b.erase(pos(0, 1), pos(2, 2));
    assert_eq!(removed, "ne\ntwo\nth");
    assert_eq!(b.text(), "oree");
}

#[test]
fn buffer_erase_line_break() {
    let mut b = make_buffer("ab\ncd");
    b.erase(pos(0, 2), pos(1, 0));
    assert_eq!(b.text(), "abcd");
}

#[test]
fn buffer_delete_lines() {
    let mut middle = make_buffer("a\nb\nc");
    assert_eq!(middle.delete_lines(1, 1), "b");
    assert_eq!(middle.text(), "a\nc");

    let mut last = make_buffer("a\nb\nc");
    assert_eq!(last.delete_lines(1, 2), "b\nc");
    assert_eq!(last.text(), "a");

    let mut all = make_buffer("a\nb");
    all.delete_lines(0, 1);
    assert_eq!(all.line_count(), 1);
    assert_eq!(all.text(), "");
}

#[test]
fn buffer_insert_lines() {
    let mut b = make_buffer("a\nc");
    b.insert_lines(1, "b");
    b.insert_lines(3, "d");
    b.insert_lines(0, "start");
    assert_eq!(b.text(), "start\na\nb\nc\nd");
}

#[test]
fn buffer_undo_redo_group() {
    let mut b = make_buffer("hello");
    b.begin_undo_group(pos(0, 0));
    b.insert(pos(0, 5), " world");
    b.erase(pos(0, 0), pos(0, 1));
    b.end_undo_group();
    assert_eq!(b.text(), "ello world");

    assert!(b.undo().is_some());
    assert_eq!(b.text(), "hello");
    assert!(b.undo().is_none());

    assert!(b.redo().is_some());
    assert_eq!(b.text(), "ello world");
    assert!(b.redo().is_none());
}

#[test]
fn buffer_new_edit_clears_redo() {
    let mut b = make_buffer("a");
    b.insert(pos(0, 1), "b");
    b.undo();
    b.insert(pos(0, 1), "c");
    assert!(!b.can_redo());
    assert_eq!(b.text(), "ac");
}

#[test]
fn buffer_modified_tracks_saved_state() {
    let mut b = make_buffer("x");
    assert!(!b.is_modified());
    b.insert(pos(0, 1), "y");
    assert!(b.is_modified());
    b.undo();
    assert!(!b.is_modified()); // back at the saved state
    b.redo();
    b.mark_saved();
    assert!(!b.is_modified());
    b.undo();
    assert!(b.is_modified());
}

#[test]
fn buffer_load_and_save_round_trip() {
    let path = temp_path("buffer_round_trip");
    let path_str = path.to_str().unwrap();
    fs::write(&path, "line one\nline two\n").unwrap();
    let mut b = Buffer::new();
    assert!(b.load(path_str).is_ok());
    assert_eq!(b.line_count(), 2);
    assert_eq!(b.line(1), "line two");

    b.insert(pos(1, 8), "!");
    assert!(b.save(path_str).is_ok());
    assert!(!b.is_modified());
    assert_eq!(fs::read_to_string(&path).unwrap(), "line one\nline two!\n");

    fs::remove_file(&path).unwrap();
}

#[test]
fn buffer_keeps_crlf_line_endings() {
    let path = temp_path("buffer_crlf");
    let path_str = path.to_str().unwrap();
    fs::write(&path, "a\r\nb\r\n").unwrap();
    let mut b = Buffer::new();
    assert!(b.load(path_str).is_ok());
    assert_eq!(b.text(), "a\nb");
    assert!(b.save(path_str).is_ok());
    assert_eq!(fs::read_to_string(&path).unwrap(), "a\r\nb\r\n");
    fs::remove_file(&path).unwrap();
}

#[test]
fn buffer_load_missing_file_fails() {
    let mut b = Buffer::new();
    let error = b.load("/nonexistent/dir/file.txt").unwrap_err();
    assert!(!error.is_empty());
}

#[test]
fn buffer_load_replaces_invalid_utf8() {
    let path = temp_path("buffer_invalid_utf8");
    let path_str = path.to_str().unwrap();
    fs::write(&path, b"caf\xe9\nok\n").unwrap();
    let mut b = Buffer::new();
    assert!(b.load(path_str).is_ok());
    assert!(b.had_invalid_utf8());
    assert_eq!(b.text(), "caf\u{FFFD}\nok");
    b.set_text("fresh");
    assert!(!b.had_invalid_utf8());
    fs::remove_file(&path).unwrap();
}
