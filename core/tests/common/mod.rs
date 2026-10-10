// Shared by several test crates; not every one uses every helper.
#![allow(dead_code)]

use cursey_core::buffer::Buffer;
use cursey_core::editor::Editor;
use cursey_core::keys;
use cursey_core::types::Position;
use std::path::PathBuf;

pub const fn pos(row: usize, col: usize) -> Position {
    Position::new(row, col)
}

/// An editor with some text in it, driven by vim key notation.
pub struct Session {
    pub editor: Editor,
}

impl Session {
    pub fn new(text: &str) -> Self {
        let mut editor = Editor::new();
        editor.buffer_mut().set_text(text);
        Self { editor }
    }

    pub fn keys(&mut self, notation: &str) -> &mut Self {
        for k in keys::parse(notation) {
            self.editor.handle_key(k);
        }
        self
    }

    pub fn text(&self) -> String {
        self.editor.buffer().text()
    }

    pub fn cursor(&self) -> Position {
        self.editor.cursor()
    }
}

/// Feed `keys` to an editor containing `text` and return the resulting text.
pub fn run(text: &str, keys: &str) -> String {
    let mut s = Session::new(text);
    s.keys(keys);
    s.text()
}

pub fn make_buffer(text: &str) -> Buffer {
    let mut b = Buffer::new();
    b.set_text(text);
    b
}

/// A temp file path unique to this test and process (tests run in
/// parallel).
pub fn temp_path(name: &str) -> PathBuf {
    std::env::temp_dir()
        .join(format!("cursey_{name}_{}.txt", std::process::id()))
}
