//! Editor logic for cursey. Nothing in this crate touches the terminal: the
//! UI feeds keys to [`editor::Editor`] and draws whatever its accessors
//! report, so every editing feature can be tested without a screen.

pub mod buffer;
pub mod editor;
pub mod ex;
pub mod keys;
pub mod log;
pub mod motions;
pub mod options;
pub mod registers;
pub mod types;
pub mod utf8;
