//! Keys are `char`s holding a Unicode codepoint. Control characters use their
//! ASCII codes (Ctrl-R == '\x12') and non-character keys (arrows, ...) live in
//! the Unicode private use area, so every key sequence can be stored as UTF-8
//! text (that is how macros are kept in registers).

pub const TAB: char = '\t';
pub const ENTER: char = '\r';
pub const ESC: char = '\x1b';
pub const BACKSPACE: char = '\x7f';

pub const SPECIAL_BASE: char = '\u{F0000}';
pub const UP: char = '\u{F0001}';
pub const DOWN: char = '\u{F0002}';
pub const LEFT: char = '\u{F0003}';
pub const RIGHT: char = '\u{F0004}';
pub const DELETE: char = '\u{F0005}';
pub const HOME: char = '\u{F0006}';
pub const END: char = '\u{F0007}';
pub const PAGE_UP: char = '\u{F0008}';
pub const PAGE_DOWN: char = '\u{F0009}';

/// The control character for an ASCII letter: `ctrl('r') == '\x12'`.
pub const fn ctrl(c: char) -> char {
    (c as u8 & 0x1f) as char
}

// Named so they can be used as `match` patterns.
pub const CTRL_C: char = ctrl('c');
pub const CTRL_D: char = ctrl('d');
pub const CTRL_H: char = ctrl('h');
pub const CTRL_R: char = ctrl('r');
pub const CTRL_U: char = ctrl('u');

const NAMED_KEYS: [(&str, char); 17] = [
    ("esc", ESC),
    ("cr", ENTER),
    ("enter", ENTER),
    ("return", ENTER),
    ("bs", BACKSPACE),
    ("tab", TAB),
    ("space", ' '),
    ("lt", '<'),
    ("up", UP),
    ("down", DOWN),
    ("left", LEFT),
    ("right", RIGHT),
    ("del", DELETE),
    ("home", HOME),
    ("end", END),
    ("pageup", PAGE_UP),
    ("pagedown", PAGE_DOWN),
];

/// Look up the key named inside `<...>`, e.g. "Esc" or "C-r".
fn named_key(name: &str) -> Option<char> {
    let name = name.to_ascii_lowercase();
    if let Some(c) = name.strip_prefix("c-")
        && let [b] = c.as_bytes()
    {
        return Some(ctrl(*b as char));
    }
    NAMED_KEYS.iter().find(|(n, _)| *n == name).map(|&(_, k)| k)
}

/// Parse vim key notation: "dw", "ihello<Esc>", "<C-r>", "<CR>", "<lt>".
/// Unknown `<...>` sequences are taken literally.
pub fn parse(notation: &str) -> Vec<char> {
    let mut keys = Vec::new();
    let mut i = 0;
    while let Some(c) = notation[i..].chars().next() {
        if c == '<'
            && let Some(len) = notation[i + 1..].find('>')
            && let Some(k) = named_key(&notation[i + 1..i + 1 + len])
        {
            keys.push(k);
            i += len + 2;
            continue;
        }
        keys.push(c);
        i += c.len_utf8();
    }
    keys
}

/// Inverse of [`parse`], for showing pending keys / debugging.
pub fn to_notation(k: char) -> String {
    let name = match k {
        ESC => "<Esc>",
        ENTER => "<CR>",
        BACKSPACE => "<BS>",
        TAB => "<Tab>",
        UP => "<Up>",
        DOWN => "<Down>",
        LEFT => "<Left>",
        RIGHT => "<Right>",
        DELETE => "<Del>",
        HOME => "<Home>",
        END => "<End>",
        PAGE_UP => "<PageUp>",
        PAGE_DOWN => "<PageDown>",
        '\x01'..='\x1f' => return format!("^{}", (k as u8 + b'@') as char),
        _ => return k.to_string(),
    };
    name.to_string()
}

/// Raw UTF-8 round trip (used for macro registers).
pub fn to_text(keys: &[char]) -> String {
    keys.iter().collect()
}

pub fn from_text(text: &str) -> Vec<char> {
    text.chars().collect()
}
