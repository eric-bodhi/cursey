mod common;

use common::{make_buffer, pos};
use cursey_core::motions;

#[test]
fn motion_word_forward() {
    let b = make_buffer("foo bar.baz\n\n  qux");
    let mut p = pos(0, 0);
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(0, 4)); // bar
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(0, 7)); // .
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(0, 8)); // baz
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(1, 0)); // empty line counts as a word
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(2, 2)); // qux
    p = motions::word_forward(&b, p, false);
    assert_eq!(p, pos(2, 5)); // end of buffer
}

#[test]
fn motion_bigword_forward() {
    let b = make_buffer("foo bar.baz qux");
    let p = motions::word_forward(&b, pos(0, 4), true);
    assert_eq!(p, pos(0, 12));
}

#[test]
fn motion_word_backward() {
    let b = make_buffer("foo bar.baz\n\n  qux");
    let mut p = pos(2, 2);
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(1, 0));
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(0, 8));
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(0, 7));
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(0, 4));
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(0, 0));
    p = motions::word_backward(&b, p, false);
    assert_eq!(p, pos(0, 0)); // stays at the start
}

#[test]
fn motion_word_end() {
    let b = make_buffer("foo bar\nbaz");
    let mut p = pos(0, 0);
    p = motions::word_end(&b, p, false);
    assert_eq!(p, pos(0, 2));
    p = motions::word_end(&b, p, false);
    assert_eq!(p, pos(0, 6));
    p = motions::word_end(&b, p, false);
    assert_eq!(p, pos(1, 2));
}

#[test]
fn motion_word_end_backward() {
    let b = make_buffer("foo bar\nbaz");
    let mut p = pos(1, 1);
    p = motions::word_end_backward(&b, p, false);
    assert_eq!(p, pos(0, 6));
    p = motions::word_end_backward(&b, p, false);
    assert_eq!(p, pos(0, 2));
}

#[test]
fn motion_words_utf8() {
    // "é" is two bytes; motions must not land inside it.
    let b = make_buffer("café au lait");
    let p = motions::word_forward(&b, pos(0, 0), false);
    assert_eq!(p, pos(0, 6));
    assert_eq!(motions::word_end(&b, pos(0, 0), false), pos(0, 3));
}

#[test]
fn motion_find_in_line() {
    use motions::find_in_line;
    let line = "a,b,c,d";
    assert_eq!(find_in_line(line, 0, ',', true, false, 1, false), Some(1));
    assert_eq!(find_in_line(line, 0, ',', true, false, 2, false), Some(3));
    assert_eq!(find_in_line(line, 0, ',', true, true, 1, false), Some(0));
    assert_eq!(find_in_line(line, 6, ',', false, false, 1, false), Some(5));
    assert_eq!(find_in_line(line, 6, ',', false, true, 1, false), Some(6));
    assert_eq!(find_in_line(line, 0, 'x', true, false, 1, false), None);
    // Repeating "t," with ; must not get stuck just before the match.
    assert_eq!(find_in_line(line, 0, ',', true, true, 1, true), Some(2));
}
