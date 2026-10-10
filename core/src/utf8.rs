//! Minimal UTF-8 helpers. Lines are stored as `String`s; these functions make
//! sure the cursor always lands on the start of a character. They accept any
//! byte index, even one inside a character, and never panic.

use unicode_width::UnicodeWidthChar;

/// Byte index of the character after the one at `i` (clamped to `s.len()`).
pub fn next(s: &str, i: usize) -> usize {
    if i >= s.len() {
        return s.len();
    }
    (i + 1..s.len())
        .find(|&j| s.is_char_boundary(j))
        .unwrap_or(s.len())
}

/// Byte index of the character before `i` (0 if already at the start).
pub fn prev(s: &str, i: usize) -> usize {
    let i = i.min(s.len());
    (0..i).rev().find(|&j| s.is_char_boundary(j)).unwrap_or(0)
}

/// `i` moved back to the start of the character containing it.
pub fn floor(s: &str, i: usize) -> usize {
    if i >= s.len() {
        return s.len();
    }
    (0..=i).rev().find(|&j| s.is_char_boundary(j)).unwrap_or(0)
}

/// The character starting at byte `i`, or '\0' at the end of the string.
pub fn decode(s: &str, i: usize) -> char {
    s.get(i..)
        .and_then(|rest| rest.chars().next())
        .unwrap_or('\0')
}

/// Terminal cell width of a character (0, 1 or 2). Tabs are handled by
/// callers; other control characters are shown as `^X`.
pub fn width(c: char) -> usize {
    if c < ' ' || c == '\x7f' {
        return 2;
    }
    c.width().unwrap_or(1)
}

/// Display column of byte offset `byte_col`, expanding tabs to `tabstop`.
pub fn display_col(line: &str, byte_col: usize, tabstop: usize) -> usize {
    let mut col = 0;
    for (i, c) in line.char_indices() {
        if i >= byte_col {
            break;
        }
        col += if c == '\t' {
            tabstop - col % tabstop
        } else {
            width(c)
        };
    }
    col
}

/// Byte offset of the character covering display column `target` (or the
/// end of the line if the line is shorter).
pub fn byte_col_at(line: &str, target: usize, tabstop: usize) -> usize {
    let mut col = 0;
    for (i, c) in line.char_indices() {
        let w = if c == '\t' {
            tabstop - col % tabstop
        } else {
            width(c)
        };
        if col + w > target {
            return i;
        }
        col += w;
    }
    line.len()
}
