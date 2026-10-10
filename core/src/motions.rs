//! Cursor motions as pure functions of (buffer, position), so they are easy to
//! unit test. Motions that depend on editor state (j/k with the desired
//! column, searches) live in the Editor.

use crate::buffer::Buffer;
use crate::types::Position;
use crate::utf8;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CharClass {
    Blank,
    Punct,
    Word,
}

fn is_blank(c: char) -> bool {
    matches!(c, ' ' | '\t' | '\n' | '\0')
}

/// Character at `p`; the end of a line reads as a virtual '\n'.
fn char_at(buf: &Buffer, p: Position) -> char {
    let line = buf.line(p.row);
    if p.col < line.len() {
        utf8::decode(line, p.col)
    } else {
        '\n'
    }
}

/// Step to the next character position. Empty lines have exactly one
/// position (column 0); non-empty lines have one per character.
fn next_pos(buf: &Buffer, p: &mut Position) -> bool {
    let line = buf.line(p.row);
    if p.col < line.len() {
        let n = utf8::next(line, p.col);
        if n < line.len() {
            p.col = n;
            return true;
        }
    }
    if p.row >= buf.last_row() {
        return false;
    }
    *p = Position::new(p.row + 1, 0);
    true
}

fn prev_pos(buf: &Buffer, p: &mut Position) -> bool {
    if p.col > 0 {
        p.col = utf8::prev(buf.line(p.row), p.col);
        return true;
    }
    if p.row == 0 {
        return false;
    }
    p.row -= 1;
    let line = buf.line(p.row);
    p.col = utf8::prev(line, line.len());
    true
}

fn is_empty_line(buf: &Buffer, p: Position) -> bool {
    buf.line_length(p.row) == 0
}

fn class_at(buf: &Buffer, p: Position, big: bool) -> CharClass {
    char_class(char_at(buf, p), big)
}

/// `bigword` (W/B/E) treats every non-blank character as part of a word.
pub fn char_class(c: char, bigword: bool) -> CharClass {
    if is_blank(c) {
        CharClass::Blank
    } else if bigword || c == '_' || !c.is_ascii() || c.is_ascii_alphanumeric()
    {
        CharClass::Word
    } else {
        CharClass::Punct
    }
}

// One step of w / e / b / ge. Callers loop for counts.

pub fn word_forward(buf: &Buffer, mut p: Position, big: bool) -> Position {
    let start_line = buf.line(p.row);
    if p.col < start_line.len() {
        // Skip the rest of the current word.
        let c0 = class_at(buf, p, big);
        if c0 != CharClass::Blank {
            while p.col < start_line.len() && class_at(buf, p, big) == c0 {
                p.col = utf8::next(start_line, p.col);
            }
        }
    }
    // Skip whitespace and line breaks; an empty line counts as a word.
    loop {
        let line = buf.line(p.row);
        if p.col >= line.len() {
            if p.row >= buf.last_row() {
                return Position::new(p.row, line.len());
            }
            p = Position::new(p.row + 1, 0);
            if buf.line_length(p.row) == 0 {
                return p;
            }
            continue;
        }
        if class_at(buf, p, big) != CharClass::Blank {
            return p;
        }
        p.col = utf8::next(line, p.col);
    }
}

pub fn word_end(buf: &Buffer, mut p: Position, big: bool) -> Position {
    if !next_pos(buf, &mut p) {
        return p;
    }
    while class_at(buf, p, big) == CharClass::Blank {
        if !next_pos(buf, &mut p) {
            return p;
        }
    }
    let c0 = class_at(buf, p, big);
    loop {
        let mut q = p;
        if !next_pos(buf, &mut q)
            || q.row != p.row
            || class_at(buf, q, big) != c0
        {
            return p;
        }
        p = q;
    }
}

pub fn word_backward(buf: &Buffer, mut p: Position, big: bool) -> Position {
    if !prev_pos(buf, &mut p) {
        return p;
    }
    while class_at(buf, p, big) == CharClass::Blank {
        if is_empty_line(buf, p) || !prev_pos(buf, &mut p) {
            return p;
        }
    }
    let c0 = class_at(buf, p, big);
    loop {
        let mut q = p;
        if !prev_pos(buf, &mut q)
            || q.row != p.row
            || class_at(buf, q, big) != c0
        {
            return p;
        }
        p = q;
    }
}

pub fn word_end_backward(buf: &Buffer, mut p: Position, big: bool) -> Position {
    // Move to the start of the current word first.
    let c0 = class_at(buf, p, big);
    if c0 != CharClass::Blank {
        loop {
            let mut q = p;
            if !prev_pos(buf, &mut q)
                || q.row != p.row
                || class_at(buf, q, big) != c0
            {
                break;
            }
            p = q;
        }
    }
    if !prev_pos(buf, &mut p) {
        return Position::new(0, 0);
    }
    while class_at(buf, p, big) == CharClass::Blank && !is_empty_line(buf, p) {
        if !prev_pos(buf, &mut p) {
            return p;
        }
    }
    p
}

/// f/F/t/T: search `line` for the count-th `target` after (forward) or before
/// `col`. `till` stops one character short. `skip_adjacent` is used when
/// repeating t/T with ; so it does not get stuck.
pub fn find_in_line(
    line: &str,
    col: usize,
    target: char,
    forward: bool,
    till: bool,
    count: usize,
    skip_adjacent: bool,
) -> Option<usize> {
    let mut pos = col;
    for n in 0..count {
        let mut from = pos;
        if n == 0 && till && skip_adjacent {
            from = if forward {
                utf8::next(line, from)
            } else {
                utf8::prev(line, from)
            };
        }
        pos = if forward {
            let start = utf8::next(line, from);
            start + line[start..].find(target)?
        } else if from == 0 {
            return None;
        } else {
            // Matches starting at or before the previous character.
            let end = utf8::next(line, utf8::prev(line, from));
            line[..end].rfind(target)?
        };
    }
    if till {
        pos = if forward {
            utf8::prev(line, pos)
        } else {
            utf8::next(line, pos)
        };
    }
    Some(pos)
}
