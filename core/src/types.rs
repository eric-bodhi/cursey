/// A location in the buffer. Both fields are 0-indexed; `col` is a *byte*
/// offset into the line (lines are stored as UTF-8), so it always points at
/// the first byte of a character. Convert to display columns with
/// [`crate::utf8::display_col`].
///
/// Ordering is lexicographic: row first, then column.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Position {
    pub row: usize,
    pub col: usize,
}

impl Position {
    pub const fn new(row: usize, col: usize) -> Self {
        Self { row, col }
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Mode {
    #[default]
    Normal,
    Insert,
    Visual,
    VisualLine,
    /// ':' commands and '/' '?' searches.
    CommandLine,
}

impl Mode {
    pub const fn is_visual(self) -> bool {
        matches!(self, Mode::Visual | Mode::VisualLine)
    }
}
