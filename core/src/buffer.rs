use crate::types::Position;
use crate::utf8;
use std::fs::{self, File};
use std::io::{self, Write};

/// The text of one file plus its undo history.
///
/// Text is a vector of lines (without '\n'); there is always at least one
/// line. Every modification goes through the two primitives [`insert`] and
/// [`erase`]. Each call is recorded as an [`Edit`], so undo/redo is just
/// "apply the inverse edits" - any new editing command gets undo support for
/// free.
///
/// [`insert`]: Buffer::insert
/// [`erase`]: Buffer::erase
#[derive(Debug)]
pub struct Buffer {
    lines: Vec<String>,

    undo_stack: Vec<UndoGroup>,
    redo_stack: Vec<UndoGroup>,
    open_group: Option<UndoGroup>,
    group_depth: u32,
    next_group_id: u64,
    saved_state: u64,
    revision: u64,

    crlf: bool,         // file used "\r\n" line endings
    invalid_utf8: bool, // file had bytes that are not UTF-8 (now U+FFFD)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum EditKind {
    Insert,
    Erase,
}

#[derive(Debug)]
struct Edit {
    kind: EditKind,
    pos: Position,
    text: String,
}

#[derive(Debug, Default)]
struct UndoGroup {
    edits: Vec<Edit>,
    cursor_before: Position,
    id: u64,
}

/// Position just after `text` if it were inserted at `pos`.
fn end_of_inserted(pos: Position, text: &str) -> Position {
    match text.rfind('\n') {
        None => Position::new(pos.row, pos.col + text.len()),
        Some(last_nl) => Position::new(
            pos.row + text.matches('\n').count(),
            text.len() - last_nl - 1,
        ),
    }
}

/// An I/O error as a vim-style message: "Permission denied", without the
/// " (os error 13)" suffix Rust adds.
fn describe(err: &io::Error) -> String {
    let text = err.to_string();
    match text.find(" (os error") {
        Some(i) => text[..i].to_string(),
        None => text,
    }
}

impl Default for Buffer {
    fn default() -> Self {
        Self {
            lines: vec![String::new()],
            undo_stack: Vec::new(),
            redo_stack: Vec::new(),
            open_group: None,
            group_depth: 0,
            next_group_id: 1,
            saved_state: 0,
            revision: 0,
            crlf: false,
            invalid_utf8: false,
        }
    }
}

impl Buffer {
    pub fn new() -> Self {
        Self::default()
    }

    /// Replace the whole text (and forget undo history).
    pub fn set_text(&mut self, text: &str) {
        self.lines = text.split('\n').map(str::to_string).collect();
        self.undo_stack.clear();
        self.redo_stack.clear();
        self.open_group = None;
        self.group_depth = 0;
        self.saved_state = 0;
        self.revision += 1;
        self.invalid_utf8 = false;
    }

    pub fn text(&self) -> String {
        self.lines.join("\n")
    }

    // ---- File I/O. Errors are vim-style messages.

    pub fn load(&mut self, path: &str) -> Result<(), String> {
        let bytes = fs::read(path)
            .map_err(|e| format!("\"{path}\" {}", describe(&e)))?;
        let (mut content, invalid_utf8) = match String::from_utf8(bytes) {
            Ok(s) => (s, false),
            Err(e) => {
                (String::from_utf8_lossy(e.as_bytes()).into_owned(), true)
            }
        };

        // A trailing newline terminates the last line; it does not start a
        // new (empty) one.
        if content.ends_with('\n') {
            content.pop();
        }
        let crlf = content.contains("\r\n");
        if crlf {
            content.retain(|c| c != '\r');
        }
        self.set_text(&content);
        self.crlf = crlf;
        self.invalid_utf8 = invalid_utf8;
        Ok(())
    }

    pub fn save(&mut self, path: &str) -> Result<(), String> {
        let mut file = File::create(path).map_err(|e| {
            format!(
                "\"{path}\" E212: Can't open file for writing: {}",
                describe(&e)
            )
        })?;
        let eol = if self.crlf { "\r\n" } else { "\n" };
        let mut content = String::new();
        // An empty buffer is written as an empty file.
        if !(self.lines.len() == 1 && self.lines[0].is_empty()) {
            for line in &self.lines {
                content.push_str(line);
                content.push_str(eol);
            }
        }
        file.write_all(content.as_bytes())
            .and_then(|()| file.flush())
            .map_err(|_| format!("\"{path}\" E514: Write error"))?;
        self.mark_saved();
        Ok(())
    }

    /// Whether the last [`load`](Buffer::load) had to replace bytes that were
    /// not valid UTF-8.
    pub fn had_invalid_utf8(&self) -> bool {
        self.invalid_utf8
    }

    pub fn line_count(&self) -> usize {
        self.lines.len()
    }

    pub fn line(&self, row: usize) -> &str {
        &self.lines[row]
    }

    pub fn line_length(&self, row: usize) -> usize {
        self.lines[row].len()
    }

    pub fn last_row(&self) -> usize {
        self.lines.len() - 1
    }

    /// One past the last character.
    pub fn end_position(&self) -> Position {
        Position::new(self.last_row(), self.lines[self.last_row()].len())
    }

    /// Byte offset of the first non-blank character (or the line length).
    pub fn first_non_blank(&self, row: usize) -> usize {
        let line = &self.lines[row];
        line.find(|c| c != ' ' && c != '\t').unwrap_or(line.len())
    }

    /// `pos` moved inside the text and onto the start of a character.
    fn clamp(&self, pos: Position) -> Position {
        if pos.row >= self.lines.len() {
            return self.end_position();
        }
        Position::new(pos.row, utf8::floor(&self.lines[pos.row], pos.col))
    }

    /// Text in [start, end); lines are joined with '\n'.
    pub fn get_text(&self, start: Position, end: Position) -> String {
        let (start, end) = (self.clamp(start), self.clamp(end));
        if end <= start {
            return String::new();
        }
        if start.row == end.row {
            return self.lines[start.row][start.col..end.col].to_string();
        }
        let mut out = self.lines[start.row][start.col..].to_string();
        for line in &self.lines[start.row + 1..end.row] {
            out.push('\n');
            out.push_str(line);
        }
        out.push('\n');
        out.push_str(&self.lines[end.row][..end.col]);
        out
    }

    // ---- Primitive edits

    /// Insert `text` (may contain '\n') at `pos`. Returns the position right
    /// after the inserted text.
    pub fn insert(&mut self, pos: Position, text: &str) -> Position {
        let pos = self.clamp(pos);
        if text.is_empty() {
            return pos;
        }
        self.record(Edit {
            kind: EditKind::Insert,
            pos,
            text: text.to_string(),
        });
        self.raw_insert(pos, text)
    }

    /// Remove the text in [start, end) and return it. To remove a line
    /// break, `end` must be at column 0 of the next line.
    pub fn erase(&mut self, start: Position, end: Position) -> String {
        let start = self.clamp(start);
        let removed = self.raw_erase(start, end);
        if !removed.is_empty() {
            self.record(Edit {
                kind: EditKind::Erase,
                pos: start,
                text: removed.clone(),
            });
        }
        removed
    }

    // ---- Convenience edits built on the primitives

    pub fn replace(&mut self, start: Position, end: Position, text: &str) {
        self.erase(start, end);
        self.insert(start, text);
    }

    pub fn replace_line(&mut self, row: usize, text: &str) {
        if self.lines[row] != text {
            let end = Position::new(row, self.lines[row].len());
            self.replace(Position::new(row, 0), end, text);
        }
    }

    /// Delete whole lines [first, last] (keeps at least one empty line).
    pub fn delete_lines(&mut self, first: usize, last: usize) -> String {
        let last = last.min(self.last_row());
        if first > last {
            return String::new();
        }
        let last_end = Position::new(last, self.lines[last].len());
        let removed = self.get_text(Position::new(first, 0), last_end);
        if last < self.last_row() {
            // Remove the lines together with their trailing line breaks.
            self.erase(Position::new(first, 0), Position::new(last + 1, 0));
        } else if first > 0 {
            // Deleting through the last line: eat the line break before
            // `first`.
            let prev_end =
                Position::new(first - 1, self.lines[first - 1].len());
            self.erase(prev_end, last_end);
        } else {
            // Deleting everything: leave a single empty line.
            self.erase(Position::new(0, 0), self.end_position());
        }
        removed
    }

    /// Insert whole lines before `row` (row == line_count() appends).
    pub fn insert_lines(&mut self, row: usize, text: &str) {
        if row >= self.lines.len() {
            self.insert(self.end_position(), &format!("\n{text}"));
        } else {
            self.insert(Position::new(row, 0), &format!("{text}\n"));
        }
    }

    fn raw_insert(&mut self, pos: Position, text: &str) -> Position {
        let pos = self.clamp(pos);
        self.revision += 1;
        let line = &mut self.lines[pos.row];
        let Some(nl) = text.find('\n') else {
            line.insert_str(pos.col, text);
            return Position::new(pos.row, pos.col + text.len());
        };

        // Split the line: [head][first piece] ... [last piece][tail]
        let tail = line.split_off(pos.col);
        line.push_str(&text[..nl]);

        let mut new_lines: Vec<String> =
            text[nl + 1..].split('\n').map(str::to_string).collect();
        let row = pos.row + new_lines.len();
        let last = new_lines.last_mut().expect("split yields a piece");
        let end = Position::new(row, last.len());
        last.push_str(&tail);
        self.lines.splice(pos.row + 1..pos.row + 1, new_lines);
        end
    }

    fn raw_erase(&mut self, start: Position, end: Position) -> String {
        let (start, end) = (self.clamp(start), self.clamp(end));
        if end <= start {
            return String::new();
        }
        self.revision += 1;
        let removed = self.get_text(start, end);
        if start.row == end.row {
            self.lines[start.row].replace_range(start.col..end.col, "");
            return removed;
        }
        let tail = self.lines[end.row][end.col..].to_string();
        let line = &mut self.lines[start.row];
        line.truncate(start.col);
        line.push_str(&tail);
        self.lines.drain(start.row + 1..=end.row);
        removed
    }

    // ---- Undo

    fn record(&mut self, edit: Edit) {
        self.redo_stack.clear();
        if let Some(group) = &mut self.open_group {
            group.edits.push(edit);
            return;
        }
        // An edit outside of any group becomes its own undo step.
        let group = UndoGroup {
            cursor_before: edit.pos,
            id: self.next_group_id,
            edits: vec![edit],
        };
        self.next_group_id += 1;
        self.undo_stack.push(group);
    }

    /// Edits made between begin/end form one undo step. Nesting is allowed;
    /// only the outermost [`end_undo_group`](Buffer::end_undo_group) commits.
    pub fn begin_undo_group(&mut self, cursor: Position) {
        if self.group_depth == 0 {
            self.open_group = Some(UndoGroup {
                cursor_before: cursor,
                ..UndoGroup::default()
            });
        }
        self.group_depth += 1;
    }

    pub fn end_undo_group(&mut self) {
        if self.group_depth == 0 {
            return;
        }
        self.group_depth -= 1;
        if self.group_depth == 0
            && let Some(mut group) = self.open_group.take()
            && !group.edits.is_empty()
        {
            group.id = self.next_group_id;
            self.next_group_id += 1;
            self.undo_stack.push(group);
        }
    }

    /// Returns the cursor position to restore, or `None` if there is nothing
    /// to undo.
    pub fn undo(&mut self) -> Option<Position> {
        let group = self.undo_stack.pop()?;
        for edit in group.edits.iter().rev() {
            match edit.kind {
                EditKind::Insert => {
                    self.raw_erase(
                        edit.pos,
                        end_of_inserted(edit.pos, &edit.text),
                    );
                }
                EditKind::Erase => {
                    self.raw_insert(edit.pos, &edit.text);
                }
            }
        }
        let cursor = group.cursor_before;
        self.redo_stack.push(group);
        Some(self.clamp(cursor))
    }

    pub fn redo(&mut self) -> Option<Position> {
        let group = self.redo_stack.pop()?;
        for edit in &group.edits {
            match edit.kind {
                EditKind::Insert => {
                    self.raw_insert(edit.pos, &edit.text);
                }
                EditKind::Erase => {
                    self.raw_erase(
                        edit.pos,
                        end_of_inserted(edit.pos, &edit.text),
                    );
                }
            }
        }
        let cursor = group.edits[0].pos;
        self.undo_stack.push(group);
        Some(self.clamp(cursor))
    }

    pub fn can_undo(&self) -> bool {
        !self.undo_stack.is_empty()
    }

    pub fn can_redo(&self) -> bool {
        !self.redo_stack.is_empty()
    }

    fn state_id(&self) -> u64 {
        self.undo_stack.last().map_or(0, |group| group.id)
    }

    /// Modified relative to the last load/save (undoing back to the saved
    /// state makes the buffer unmodified again).
    pub fn is_modified(&self) -> bool {
        let pending = self
            .open_group
            .as_ref()
            .is_some_and(|g| !g.edits.is_empty());
        pending || self.state_id() != self.saved_state
    }

    pub fn mark_saved(&mut self) {
        self.saved_state = self.state_id();
    }

    /// Incremented on every edit; cheap way for others to notice changes.
    pub fn revision(&self) -> u64 {
        self.revision
    }
}
