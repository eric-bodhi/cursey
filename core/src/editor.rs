use crate::buffer::Buffer;
use crate::ex;
use crate::keys::{
    self, BACKSPACE, CTRL_C, CTRL_D, CTRL_H, CTRL_R, CTRL_U, DELETE, DOWN, END,
    ENTER, ESC, HOME, LEFT, RIGHT, TAB, UP,
};
use crate::motions::{self, CharClass};
use crate::options::Options;
use crate::registers::{Register, Registers};
use crate::types::{Mode, Position};
use crate::utf8;
use regex::Regex;
use std::path::Path;

/// `want_col` value meaning "end of line", set by $.
const WANT_EOL: usize = usize::MAX;

/// A key that inserts itself as text (not a control or special key).
fn is_text_key(k: char) -> bool {
    k >= ' ' && k != BACKSPACE && k < keys::SPECIAL_BASE
}

/// Read an optional count at keys[i]. Returns 0 when there is none. A
/// leading '0' is the "go to column 0" motion, not a count.
fn read_count(keys: &[char], i: &mut usize) -> usize {
    let mut count = 0;
    while let Some(d) = keys.get(*i).and_then(|k| k.to_digit(10)) {
        if count == 0 && d == 0 {
            break;
        }
        count = (count * 10 + d as usize).min(99999);
        *i += 1;
    }
    count
}

fn summary(path: &str, buf: &Buffer) -> String {
    let bytes: usize =
        (0..buf.line_count()).map(|r| buf.line_length(r) + 1).sum();
    format!("\"{path}\" {}L, {bytes}B", buf.line_count())
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Selection {
    pub start: Position, // inclusive
    pub end: Position,   // inclusive
    pub linewise: bool,
}

/// How an operator treats the text up to a motion's target.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum MotionType {
    Exclusive,
    Inclusive,
    Linewise,
}

/// What a motion evaluates to. `pos` is where the cursor goes; `kind`
/// decides which text an operator acts on.
#[derive(Clone, Copy, Debug)]
struct MotionTarget {
    pos: Position,
    kind: MotionType,
    vertical: bool, // j/k: keep the desired column
    to_eol: bool,   // $: j/k then stay at the end of the line
}

impl MotionTarget {
    fn new(pos: Position, kind: MotionType) -> Self {
        Self {
            pos,
            kind,
            vertical: false,
            to_eol: false,
        }
    }
}

/// The keys naming a motion: 'w', or 'f' + arg 'x', or 'g' + arg 'g'.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct MotionKeys {
    key: char,
    arg: char,
}

/// Why the pending keys are not (yet) a command. `Ok(())` means the command
/// ran.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ParseError {
    /// Wait for more keys, e.g. after `d` or `2d`.
    Incomplete,
    /// Throw the keys away.
    Invalid,
}

use ParseError::{Incomplete, Invalid};

type ParseResult = Result<(), ParseError>;

/// The whole editor state machine. It knows nothing about the terminal: the
/// UI feeds it keys with [`handle_key`](Editor::handle_key) and draws
/// whatever the accessors report. That keeps every editing feature testable
/// without a screen (see `tests/`).
#[derive(Debug)]
pub struct Editor {
    buffer: Buffer,
    registers: Registers,
    options: Options,
    filepath: String,

    mode: Mode,
    cursor: Position,
    /// Desired display column for j/k (vim's "curswant"); `WANT_EOL` means
    /// "end of line", set by $.
    want_col: usize,
    visual_anchor: Position,

    pending: Vec<char>, // keys of the unfinished normal command
    cmdline_type: char,
    cmdline: String,

    last_find: Option<MotionKeys>, // for ; and ,
    last_search: String,           // for n and N
    last_search_forward: bool,

    message: String,
    message_error: bool,
    should_quit: bool,

    view_top: usize,
    view_left: usize,
    view_rows: usize,
    view_cols: usize,
}

impl Default for Editor {
    fn default() -> Self {
        Self {
            buffer: Buffer::new(),
            registers: Registers::default(),
            options: Options::default(),
            filepath: String::new(),
            mode: Mode::Normal,
            cursor: Position::default(),
            want_col: 0,
            visual_anchor: Position::default(),
            pending: Vec::new(),
            cmdline_type: ':',
            cmdline: String::new(),
            last_find: None,
            last_search: String::new(),
            last_search_forward: true,
            message: String::new(),
            message_error: false,
            should_quit: false,
            view_top: 0,
            view_left: 0,
            view_rows: 24,
            view_cols: 80,
        }
    }
}

impl Editor {
    pub fn new() -> Self {
        Self::default()
    }

    // ---- Files -------------------------------------------------------------

    /// Load `path` into the buffer, or start a new empty file if it does not
    /// exist yet.
    pub fn open(&mut self, path: &str) -> bool {
        self.filepath = path.to_string();
        self.cursor = Position::default();
        self.want_col = 0;
        self.view_top = 0;
        self.view_left = 0;
        if !Path::new(path).exists() {
            self.buffer.set_text("");
            self.show_message(format!("\"{path}\" [New]"));
            return true;
        }
        if let Err(error) = self.buffer.load(path) {
            self.buffer.set_text("");
            self.show_error(error);
            return false;
        }
        let summary = summary(path, &self.buffer);
        if self.buffer.had_invalid_utf8() {
            self.show_error(format!("{summary} [invalid UTF-8 replaced]"));
        } else {
            self.show_message(summary);
        }
        true
    }

    // ---- Operations used by ex commands (ex.rs) ----------------------------

    /// Write the buffer. An empty path means the current file.
    pub fn write(&mut self, path: &str) -> bool {
        let target = if path.is_empty() {
            &self.filepath
        } else {
            path
        };
        if target.is_empty() {
            self.show_error("E32: No file name");
            return false;
        }
        let target = target.to_string();
        if let Err(error) = self.buffer.save(&target) {
            self.show_error(error);
            return false;
        }
        let message = format!("{} written", summary(&target, &self.buffer));
        if self.filepath.is_empty() {
            self.filepath = target;
        }
        self.show_message(message);
        true
    }

    pub fn quit(&mut self, force: bool) {
        if !force && self.buffer.is_modified() {
            self.show_error(
                "E37: No write since last change (add ! to override)",
            );
            return;
        }
        self.should_quit = true;
    }

    /// Open another file. An empty path re-reads the current file.
    pub fn edit(&mut self, path: &str, force: bool) -> bool {
        if !force && self.buffer.is_modified() {
            self.show_error(
                "E37: No write since last change (add ! to override)",
            );
            return false;
        }
        let target = if path.is_empty() {
            &self.filepath
        } else {
            path
        };
        if target.is_empty() {
            self.show_error("E32: No file name");
            return false;
        }
        let target = target.to_string();
        self.open(&target)
    }

    pub fn goto_line(&mut self, row: usize) {
        let row = row.min(self.buffer.last_row());
        self.cursor = Position::new(row, self.buffer.first_non_blank(row));
        self.clamp_cursor();
        self.update_want_col();
    }

    pub fn show_message(&mut self, text: impl Into<String>) {
        self.message = text.into();
        self.message_error = false;
    }

    pub fn show_error(&mut self, text: impl Into<String>) {
        self.message = text.into();
        self.message_error = true;
    }

    // ---- State for the UI --------------------------------------------------

    pub fn buffer(&self) -> &Buffer {
        &self.buffer
    }

    pub fn buffer_mut(&mut self) -> &mut Buffer {
        &mut self.buffer
    }

    pub fn cursor(&self) -> Position {
        self.cursor
    }

    pub fn mode(&self) -> Mode {
        self.mode
    }

    pub fn selection(&self) -> Option<Selection> {
        self.mode.is_visual().then(|| Selection {
            start: self.visual_anchor.min(self.cursor),
            end: self.visual_anchor.max(self.cursor),
            linewise: self.mode == Mode::VisualLine,
        })
    }

    pub fn filepath(&self) -> &str {
        &self.filepath
    }

    pub fn options(&self) -> &Options {
        &self.options
    }

    pub fn options_mut(&mut self) -> &mut Options {
        &mut self.options
    }

    pub fn should_quit(&self) -> bool {
        self.should_quit
    }

    pub fn message(&self) -> &str {
        &self.message
    }

    pub fn message_is_error(&self) -> bool {
        self.message_error
    }

    /// ":wq" / "/pattern" while typing a command line, empty otherwise.
    pub fn command_line(&self) -> String {
        if self.mode != Mode::CommandLine {
            return String::new();
        }
        format!("{}{}", self.cmdline_type, self.cmdline)
    }

    /// Keys typed so far for an unfinished normal-mode command (e.g. "2d").
    pub fn pending_keys(&self) -> String {
        self.pending.iter().map(|&k| keys::to_notation(k)).collect()
    }

    /// The UI reports how many text rows/columns it can show; the editor
    /// keeps the cursor inside that window.
    pub fn set_view_size(&mut self, rows: usize, cols: usize) {
        self.view_rows = rows.max(1);
        self.view_cols = cols.max(1);
        self.scroll_to_cursor();
    }

    pub fn view_top(&self) -> usize {
        self.view_top
    }

    pub fn view_left(&self) -> usize {
        self.view_left
    }

    // ---- Key dispatch ------------------------------------------------------

    pub fn handle_key(&mut self, input: char) {
        // Like vim, Ctrl-C cancels.
        let input = if input == CTRL_C { ESC } else { input };
        if self.mode != Mode::CommandLine {
            self.message.clear();
        }
        match self.mode {
            Mode::Normal => self.handle_normal_key(input),
            Mode::Insert => self.handle_insert_key(input),
            Mode::Visual | Mode::VisualLine => self.handle_visual_key(input),
            Mode::CommandLine => self.handle_command_line_key(input),
        }
        self.clamp_cursor();
        self.scroll_to_cursor();
    }

    fn handle_normal_key(&mut self, input: char) {
        if input == ESC {
            self.pending.clear();
            return;
        }
        self.pending.push(input);

        // Every normal command is one undo step. Commands that enter insert
        // mode leave the group open until <Esc>, so the typed text joins the
        // step.
        self.buffer.begin_undo_group(self.cursor);
        let keys = std::mem::take(&mut self.pending);
        let result = self.run_normal_command(&keys);
        self.pending = keys;
        if self.mode != Mode::Insert {
            self.buffer.end_undo_group();
        }
        if result != Err(Incomplete) {
            self.pending.clear();
        }
    }

    fn handle_visual_key(&mut self, input: char) {
        if input == ESC {
            self.pending.clear();
            self.mode = Mode::Normal;
            return;
        }
        self.pending.push(input);
        self.buffer.begin_undo_group(self.cursor);
        let keys = std::mem::take(&mut self.pending);
        let result = self.run_visual_command(&keys);
        self.pending = keys;
        if self.mode != Mode::Insert {
            self.buffer.end_undo_group();
        }
        if result != Err(Incomplete) {
            self.pending.clear();
        }
    }

    fn handle_insert_key(&mut self, input: char) {
        let Position { row, col } = self.cursor;
        match input {
            ESC => {
                self.mode = Mode::Normal;
                self.buffer.end_undo_group();
                self.cursor.col = utf8::prev(self.cur_line(), col);
            }
            ENTER => {
                let indent = if self.options.autoindent {
                    self.indent_of(row)
                } else {
                    String::new()
                };
                self.cursor =
                    self.buffer.insert(self.cursor, &format!("\n{indent}"));
            }
            BACKSPACE | CTRL_H => {
                if col > 0 {
                    let prev = utf8::prev(self.cur_line(), col);
                    self.buffer.erase(Position::new(row, prev), self.cursor);
                    self.cursor.col = prev;
                } else if row > 0 {
                    // Join with the previous line.
                    let join = Position::new(
                        row - 1,
                        self.buffer.line_length(row - 1),
                    );
                    self.buffer.erase(join, self.cursor);
                    self.cursor = join;
                }
            }
            DELETE => {
                if col < self.cur_line().len() {
                    let next = utf8::next(self.cur_line(), col);
                    self.buffer.erase(self.cursor, Position::new(row, next));
                } else if row < self.buffer.last_row() {
                    self.buffer.erase(self.cursor, Position::new(row + 1, 0));
                }
            }
            TAB => {
                let text = if self.options.expandtab {
                    let ts = self.options.tabstop;
                    let display = utf8::display_col(self.cur_line(), col, ts);
                    " ".repeat(ts - display % ts)
                } else {
                    "\t".to_string()
                };
                self.cursor = self.buffer.insert(self.cursor, &text);
            }
            LEFT => self.cursor.col = utf8::prev(self.cur_line(), col),
            RIGHT => self.cursor.col = utf8::next(self.cur_line(), col),
            UP | DOWN => {
                if input == UP && row > 0 {
                    self.cursor.row -= 1;
                } else if input == DOWN && row < self.buffer.last_row() {
                    self.cursor.row += 1;
                }
                self.cursor.col = self.col_for_want(self.cursor.row);
                return; // keep the desired column
            }
            HOME => self.cursor.col = 0,
            END => self.cursor.col = self.cur_line().len(),
            _ => {
                if is_text_key(input) {
                    let mut utf8 = [0; 4];
                    self.cursor = self
                        .buffer
                        .insert(self.cursor, input.encode_utf8(&mut utf8));
                }
            }
        }
        self.update_want_col();
    }

    fn handle_command_line_key(&mut self, input: char) {
        match input {
            ESC => self.mode = Mode::Normal,
            ENTER => {
                self.mode = Mode::Normal;
                self.execute_command_line();
            }
            BACKSPACE | CTRL_H => {
                if self.cmdline.pop().is_none() {
                    self.mode = Mode::Normal;
                }
            }
            CTRL_U => self.cmdline.clear(),
            _ => {
                if is_text_key(input) {
                    self.cmdline.push(input);
                }
            }
        }
    }

    // ---- Normal-mode grammar -----------------------------------------------
    //
    //   ["x] [count] command
    //   ["x] [count] operator [count] motion
    //   ["x] [count] operator operator          (dd, cc, yy: whole lines)
    //
    // The pending keys are re-parsed from the start on every key press. That
    // keeps the parser stateless: it either finds a complete command, needs
    // more keys, or rejects the sequence.

    fn parse_motion(
        keys: &[char],
        i: &mut usize,
    ) -> Result<MotionKeys, ParseError> {
        let mut next = || {
            let k = keys.get(*i).copied().ok_or(Incomplete)?;
            *i += 1;
            Ok(k)
        };
        let key = next()?;
        match key {
            'g' => {
                let arg = next()?;
                if matches!(arg, 'g' | 'e' | 'E') {
                    Ok(MotionKeys { key, arg })
                } else {
                    Err(Invalid)
                }
            }
            'f' | 'F' | 't' | 'T' => {
                let arg = next()?;
                if is_text_key(arg) {
                    Ok(MotionKeys { key, arg })
                } else {
                    Err(Invalid)
                }
            }
            'h' | 'j' | 'k' | 'l' | 'w' | 'W' | 'b' | 'B' | 'e' | 'E' | '0'
            | '^' | '$' | 'G' | 'n' | 'N' | ';' | ',' | '+' | '-' | ' '
            | ENTER | BACKSPACE | LEFT | RIGHT | UP | DOWN | HOME | END => {
                Ok(MotionKeys { key, arg: '\0' })
            }
            _ => Err(Invalid),
        }
    }

    /// Read the optional `"x` register prefix. Returns the register name.
    fn parse_register(
        keys: &[char],
        i: &mut usize,
    ) -> Result<char, ParseError> {
        if keys[0] != '"' {
            return Ok('"');
        }
        let name = keys.get(1).copied().ok_or(Incomplete)?;
        if !Registers::is_valid(name) {
            return Err(Invalid);
        }
        *i = 2;
        Ok(name)
    }

    fn run_normal_command(&mut self, keys: &[char]) -> ParseResult {
        let mut i = 0;
        let reg = Self::parse_register(keys, &mut i)?;
        let count1 = read_count(keys, &mut i);
        let k = keys.get(i).copied().ok_or(Incomplete)?;
        i += 1;
        let count = count1.max(1);

        // Shorthands that are defined in terms of an operator + motion.
        let alias = match k {
            'x' | DELETE => "dl",
            'X' => "dh",
            'D' => "d$",
            'C' => "c$",
            's' => "cl",
            'S' => "cc",
            'Y' => "yy",
            _ => "",
        };
        if !alias.is_empty() {
            let mut expanded = keys[..i - 1].to_vec();
            expanded.extend(alias.chars());
            return self.run_normal_command(&expanded);
        }

        // Operators: d c y
        if matches!(k, 'd' | 'c' | 'y') {
            let count2 = read_count(keys, &mut i);
            let next = keys.get(i).copied().ok_or(Incomplete)?;
            let total = count * count2.max(1);
            let has_count = count1 > 0 || count2 > 0;
            if next == k {
                // dd / cc / yy: `total` whole lines starting at the cursor.
                let last =
                    (self.cursor.row + total - 1).min(self.buffer.last_row());
                let target = MotionTarget::new(
                    Position::new(last, 0),
                    MotionType::Linewise,
                );
                self.apply_operator(k, self.cursor, target, reg);
                return Ok(());
            }
            let spec = Self::parse_motion(keys, &mut i)?;
            let target = if k == 'c' && matches!(spec.key, 'w' | 'W') {
                // Special case: "cw" changes to the end of the word, like
                // "ce".
                Some(self.change_word_target(total, spec.key == 'W'))
            } else {
                self.eval_motion(spec, total, has_count, true)
            };
            let target = target.ok_or(Invalid)?;
            self.apply_operator(k, self.cursor, target, reg);
            return Ok(());
        }

        let Position { row, col } = self.cursor;
        match k {
            'i' => self.enter_insert(self.cursor),
            'a' => {
                let after = utf8::next(self.cur_line(), col);
                self.enter_insert(Position::new(row, after));
            }
            'I' => {
                let first = self.buffer.first_non_blank(row);
                self.enter_insert(Position::new(row, first));
            }
            'A' => {
                let end = self.buffer.line_length(row);
                self.enter_insert(Position::new(row, end));
            }
            'o' | 'O' => self.open_line(k == 'o'),
            'p' | 'P' => self.paste(reg, k == 'P', count),
            'J' => self.join_lines(count),
            'r' => {
                let ch = keys.get(i).copied().ok_or(Incomplete)?;
                if !self.replace_chars(count, ch) {
                    return Err(Invalid);
                }
            }
            'u' => (0..count).for_each(|_| self.undo()),
            CTRL_R => (0..count).for_each(|_| self.redo()),
            'v' | 'V' => {
                self.mode = if k == 'v' {
                    Mode::Visual
                } else {
                    Mode::VisualLine
                };
                self.visual_anchor = self.cursor;
            }
            ':' | '/' | '?' => self.begin_command_line(k),
            CTRL_D | CTRL_U => self.scroll_half_page(k == CTRL_D, count1),
            _ => {
                // Anything else must be a motion.
                i -= 1;
                let spec = Self::parse_motion(keys, &mut i)?;
                let target = self
                    .eval_motion(spec, count, count1 > 0, false)
                    .ok_or(Invalid)?;
                self.move_to(target);
            }
        }
        Ok(())
    }

    fn run_visual_command(&mut self, keys: &[char]) -> ParseResult {
        let mut i = 0;
        let reg = Self::parse_register(keys, &mut i)?;
        let count1 = read_count(keys, &mut i);
        let k = keys.get(i).copied().ok_or(Incomplete)?;

        let sel = self.selection().expect("visual mode has a selection");
        let apply_to_selection = |editor: &mut Self, op: char| {
            editor.mode = Mode::Normal;
            let kind = if sel.linewise {
                MotionType::Linewise
            } else {
                MotionType::Inclusive
            };
            let target = MotionTarget::new(sel.end, kind);
            editor.apply_operator(op, sel.start, target, reg);
        };

        match k {
            'd' | 'x' | DELETE => apply_to_selection(self, 'd'),
            'y' => apply_to_selection(self, 'y'),
            'c' | 's' => apply_to_selection(self, 'c'),
            'o' => {
                std::mem::swap(&mut self.cursor, &mut self.visual_anchor);
                self.update_want_col();
            }
            'v' | 'V' => {
                let wanted = if k == 'v' {
                    Mode::Visual
                } else {
                    Mode::VisualLine
                };
                self.mode = if self.mode == wanted {
                    Mode::Normal
                } else {
                    wanted
                };
            }
            ':' => {
                self.mode = Mode::Normal;
                self.begin_command_line(':');
            }
            _ => {
                let spec = Self::parse_motion(keys, &mut i)?;
                let target = self
                    .eval_motion(spec, count1.max(1), count1 > 0, false)
                    .ok_or(Invalid)?;
                self.move_to(target);
            }
        }
        Ok(())
    }

    // ---- Motions -----------------------------------------------------------

    fn eval_motion(
        &mut self,
        spec: MotionKeys,
        count: usize,
        has_count: bool,
        for_operator: bool,
    ) -> Option<MotionTarget> {
        let cur = self.cursor;
        let n = count;
        let last_row = self.buffer.last_row();
        let mut t = MotionTarget::new(cur, MotionType::Exclusive);

        let first_non_blank_of =
            |buf: &Buffer, row| Position::new(row, buf.first_non_blank(row));

        match spec.key {
            'h' | LEFT | BACKSPACE => {
                let line = self.buffer.line(cur.row);
                for _ in 0..count {
                    if t.pos.col == 0 {
                        break;
                    }
                    t.pos.col = utf8::prev(line, t.pos.col);
                }
            }
            'l' | RIGHT | ' ' => {
                let line = self.buffer.line(cur.row);
                for _ in 0..count {
                    if t.pos.col >= line.len() {
                        break;
                    }
                    t.pos.col = utf8::next(line, t.pos.col);
                }
                // Outside of operators, l stops on the last character.
                if !for_operator && t.pos.col >= line.len() {
                    t.pos.col = utf8::prev(line, line.len());
                }
            }
            'j' | DOWN | 'k' | UP => {
                let down = matches!(spec.key, 'j' | DOWN);
                if (down && cur.row == last_row) || (!down && cur.row == 0) {
                    return None;
                }
                t.pos.row = if down {
                    (cur.row + n).min(last_row)
                } else {
                    cur.row - cur.row.min(n)
                };
                t.pos.col = self.col_for_want(t.pos.row);
                t.kind = MotionType::Linewise;
                t.vertical = true;
            }
            '+' | ENTER => {
                t.pos = first_non_blank_of(
                    &self.buffer,
                    (cur.row + n).min(last_row),
                );
                t.kind = MotionType::Linewise;
            }
            '-' => {
                t.pos =
                    first_non_blank_of(&self.buffer, cur.row - cur.row.min(n));
                t.kind = MotionType::Linewise;
            }
            'w' | 'W' => {
                for _ in 0..count {
                    t.pos = motions::word_forward(
                        &self.buffer,
                        t.pos,
                        spec.key == 'W',
                    );
                }
                // "dw" on the last word of a line stops at the end of that
                // line instead of eating the line break.
                if for_operator
                    && t.pos.row > cur.row
                    && t.pos.col <= self.buffer.first_non_blank(t.pos.row)
                {
                    let eol = Position::new(
                        t.pos.row - 1,
                        self.buffer.line_length(t.pos.row - 1),
                    );
                    if eol > cur {
                        t.pos = eol;
                    }
                }
            }
            'b' | 'B' => {
                for _ in 0..count {
                    t.pos = motions::word_backward(
                        &self.buffer,
                        t.pos,
                        spec.key == 'B',
                    );
                }
            }
            'e' | 'E' => {
                for _ in 0..count {
                    t.pos =
                        motions::word_end(&self.buffer, t.pos, spec.key == 'E');
                }
                t.kind = MotionType::Inclusive;
            }
            'g' => {
                if spec.arg == 'g' {
                    let row = if has_count { n - 1 } else { 0 };
                    t.pos = first_non_blank_of(&self.buffer, row.min(last_row));
                    t.kind = MotionType::Linewise;
                } else {
                    // ge / gE
                    for _ in 0..count {
                        t.pos = motions::word_end_backward(
                            &self.buffer,
                            t.pos,
                            spec.arg == 'E',
                        );
                    }
                    t.kind = MotionType::Inclusive;
                }
            }
            'G' => {
                let row = if has_count {
                    (n - 1).min(last_row)
                } else {
                    last_row
                };
                t.pos = first_non_blank_of(&self.buffer, row);
                t.kind = MotionType::Linewise;
            }
            '0' | HOME => t.pos.col = 0,
            '^' => t.pos = first_non_blank_of(&self.buffer, cur.row),
            '$' | END => {
                t.pos.row = (cur.row + n - 1).min(last_row);
                let target_line = self.buffer.line(t.pos.row);
                t.pos.col = utf8::prev(target_line, target_line.len());
                t.kind = MotionType::Inclusive;
                t.to_eol = true;
            }
            'f' | 'F' | 't' | 'T' | ';' | ',' => {
                let repeat = matches!(spec.key, ';' | ',');
                let mut find = spec;
                if repeat {
                    find = self.last_find?;
                    if spec.key == ',' {
                        // Same search, opposite direction.
                        find.key = if find.key.is_ascii_lowercase() {
                            find.key.to_ascii_uppercase()
                        } else {
                            find.key.to_ascii_lowercase()
                        };
                    }
                } else {
                    self.last_find = Some(spec);
                }
                let forward = matches!(find.key, 'f' | 't');
                let till = matches!(find.key, 't' | 'T');
                t.pos.col = motions::find_in_line(
                    self.buffer.line(cur.row),
                    cur.col,
                    find.arg,
                    forward,
                    till,
                    count,
                    repeat,
                )?;
                t.kind = if forward {
                    MotionType::Inclusive
                } else {
                    MotionType::Exclusive
                };
            }
            'n' | 'N' => {
                if self.last_search.is_empty() {
                    self.show_error("E35: No previous regular expression");
                    return None;
                }
                let re = match Regex::new(&self.last_search) {
                    Ok(re) => re,
                    Err(_) => {
                        self.show_error(format!(
                            "E383: Invalid search string: {}",
                            self.last_search
                        ));
                        return None;
                    }
                };
                let forward = (spec.key == 'n') == self.last_search_forward;
                let mut wrapped = false;
                let mut pos = cur;
                for _ in 0..count {
                    let (found, wrapped_now) =
                        self.find_match(&re, forward, pos)?;
                    pos = found;
                    wrapped |= wrapped_now;
                }
                if wrapped {
                    self.show_message(if forward {
                        "search hit BOTTOM, continuing at TOP"
                    } else {
                        "search hit TOP, continuing at BOTTOM"
                    });
                }
                t.pos = pos;
            }
            _ => return None,
        }
        Some(t)
    }

    fn move_to(&mut self, target: MotionTarget) {
        self.cursor = target.pos;
        self.clamp_cursor();
        if target.to_eol {
            self.want_col = WANT_EOL; // j/k keep sticking to the end of the line
        } else if !target.vertical {
            self.update_want_col();
        }
    }

    fn change_word_target(&self, count: usize, bigword: bool) -> MotionTarget {
        let line = self.cur_line();
        let class_at =
            |col| motions::char_class(utf8::decode(line, col), bigword);
        if self.cursor.col >= line.len()
            || class_at(self.cursor.col) == CharClass::Blank
        {
            // On whitespace "cw" behaves like "dw" (changes just the blanks).
            let mut t = MotionTarget::new(self.cursor, MotionType::Exclusive);
            for _ in 0..count {
                t.pos = motions::word_forward(&self.buffer, t.pos, bigword);
            }
            if t.pos.row > self.cursor.row {
                t.pos = Position::new(self.cursor.row, line.len());
            }
            return t;
        }
        // If already on the last character of a word, the first step stays.
        let next = utf8::next(line, self.cursor.col);
        let at_word_end =
            next >= line.len() || class_at(next) != class_at(self.cursor.col);
        let mut pos = self.cursor;
        for k in 0..count {
            if k == 0 && at_word_end {
                continue;
            }
            pos = motions::word_end(&self.buffer, pos, bigword);
        }
        MotionTarget::new(pos, MotionType::Inclusive)
    }

    // ---- Operators ---------------------------------------------------------

    fn apply_operator(
        &mut self,
        op: char,
        from: Position,
        target: MotionTarget,
        reg: char,
    ) {
        if target.kind == MotionType::Linewise {
            let first = from.row.min(target.pos.row);
            let last = from.row.max(target.pos.row);
            let last_end = Position::new(last, self.buffer.line_length(last));
            let text = Register {
                text: self.buffer.get_text(Position::new(first, 0), last_end),
                linewise: true,
            };
            match op {
                'y' => {
                    self.registers.yank(reg, &text);
                    self.cursor.row = first;
                }
                'd' => {
                    self.registers.del(reg, &text);
                    self.buffer.delete_lines(first, last);
                    self.cursor.row = first.min(self.buffer.last_row());
                    self.cursor.col =
                        self.buffer.first_non_blank(self.cursor.row);
                }
                'c' => {
                    self.registers.del(reg, &text);
                    let indent = if self.options.autoindent {
                        self.indent_of(first)
                    } else {
                        String::new()
                    };
                    self.buffer.replace(
                        Position::new(first, 0),
                        last_end,
                        &indent,
                    );
                    self.enter_insert(Position::new(first, indent.len()));
                }
                _ => {}
            }
            self.clamp_cursor();
            self.update_want_col();
            return;
        }

        let start = from.min(target.pos);
        let mut end = from.max(target.pos);
        if target.kind == MotionType::Inclusive {
            end.col = utf8::next(self.buffer.line(end.row), end.col);
        } else if end.col == 0 && end.row > start.row {
            // An exclusive motion ending at column 0 does not include the
            // line break before it (e.g. "d}" or "dw" at the end of a
            // paragraph).
            end = Position::new(
                end.row - 1,
                self.buffer.line_length(end.row - 1),
            );
        }
        let text = Register {
            text: self.buffer.get_text(start, end),
            linewise: false,
        };

        match op {
            'y' => {
                self.registers.yank(reg, &text);
                self.cursor = start;
            }
            'd' => {
                self.registers.del(reg, &text);
                self.buffer.erase(start, end);
                self.cursor = start;
            }
            'c' => {
                self.registers.del(reg, &text);
                self.buffer.erase(start, end);
                self.enter_insert(start);
            }
            _ => {}
        }
        self.clamp_cursor();
        self.update_want_col();
    }

    // ---- Commands ----------------------------------------------------------

    fn enter_insert(&mut self, pos: Position) {
        self.cursor = pos;
        self.mode = Mode::Insert;
        self.update_want_col();
    }

    fn open_line(&mut self, below: bool) {
        let row = self.cursor.row;
        let indent = if self.options.autoindent {
            self.indent_of(row)
        } else {
            String::new()
        };
        if below {
            let end = Position::new(row, self.buffer.line_length(row));
            self.buffer.insert(end, &format!("\n{indent}"));
            self.enter_insert(Position::new(row + 1, indent.len()));
        } else {
            self.buffer
                .insert(Position::new(row, 0), &format!("{indent}\n"));
            self.enter_insert(Position::new(row, indent.len()));
        }
    }

    fn paste(&mut self, reg: char, before: bool, count: usize) {
        let Some(contents) = self.registers.get(reg) else {
            self.show_error(format!("E353: Nothing in register {reg}"));
            return;
        };
        let mut text = String::new();
        for k in 0..count {
            if contents.linewise && k > 0 {
                text.push('\n');
            }
            text.push_str(&contents.text);
        }

        if contents.linewise {
            let row = if before {
                self.cursor.row
            } else {
                self.cursor.row + 1
            };
            self.buffer.insert_lines(row, &text);
            self.cursor = Position::new(row, self.buffer.first_non_blank(row));
        } else {
            let line = self.cur_line();
            let col = if before || line.is_empty() {
                self.cursor.col
            } else {
                utf8::next(line, self.cursor.col)
            };
            let at = Position::new(self.cursor.row, col);
            let end = self.buffer.insert(at, &text);
            // Cursor on the last pasted character (or its start if
            // multi-line).
            self.cursor = if text.contains('\n') {
                at
            } else {
                Position::new(
                    end.row,
                    utf8::prev(self.buffer.line(end.row), end.col),
                )
            };
        }
        self.clamp_cursor();
        self.update_want_col();
    }

    fn join_lines(&mut self, count: usize) {
        let joins = count.max(2) - 1;
        for _ in 0..joins {
            if self.cursor.row >= self.buffer.last_row() {
                break;
            }
            let row = self.cursor.row;
            let len = self.buffer.line_length(row);
            let next_start = self.buffer.first_non_blank(row + 1);
            self.buffer.erase(
                Position::new(row, len),
                Position::new(row + 1, next_start),
            );
            // Separate with one space unless the line ends in whitespace or
            // the joined text is empty or starts with ')'.
            let line = self.buffer.line(row).as_bytes();
            let needs_space = len > 0
                && line[len - 1] != b' '
                && line[len - 1] != b'\t'
                && len < line.len()
                && line[len] != b')';
            if needs_space {
                self.buffer.insert(Position::new(row, len), " ");
            }
            self.cursor.col = len;
        }
        self.update_want_col();
    }

    fn replace_chars(&mut self, count: usize, ch: char) -> bool {
        if !is_text_key(ch) {
            return false; // e.g. r<Esc> cancels
        }
        let line = self.cur_line();
        let mut end = self.cursor.col;
        for _ in 0..count {
            if end >= line.len() {
                return false; // not enough characters
            }
            end = utf8::next(line, end);
        }
        let text: String = std::iter::repeat_n(ch, count).collect();
        let row = self.cursor.row;
        self.buffer
            .replace(self.cursor, Position::new(row, end), &text);
        // Cursor on the last replaced character.
        self.cursor.col =
            utf8::prev(self.buffer.line(row), self.cursor.col + text.len());
        self.update_want_col();
        true
    }

    fn undo(&mut self) {
        match self.buffer.undo() {
            Some(pos) => self.cursor = pos,
            None => self.show_message("Already at oldest change"),
        }
        self.clamp_cursor();
        self.update_want_col();
    }

    fn redo(&mut self) {
        match self.buffer.redo() {
            Some(pos) => self.cursor = pos,
            None => self.show_message("Already at newest change"),
        }
        self.clamp_cursor();
        self.update_want_col();
    }

    fn scroll_half_page(&mut self, down: bool, count: usize) {
        let amount = if count > 0 {
            count
        } else {
            (self.view_rows / 2).max(1)
        };
        let last = self.buffer.last_row();
        if down {
            self.view_top = (self.view_top + amount).min(last);
            self.cursor.row = (self.cursor.row + amount).min(last);
        } else {
            self.view_top -= self.view_top.min(amount);
            self.cursor.row -= self.cursor.row.min(amount);
        }
        self.cursor.col = self.col_for_want(self.cursor.row);
    }

    // ---- Command line and search -------------------------------------------

    fn begin_command_line(&mut self, kind: char) {
        self.mode = Mode::CommandLine;
        self.cmdline_type = kind;
        self.cmdline.clear();
    }

    fn execute_command_line(&mut self) {
        let line = std::mem::take(&mut self.cmdline);
        if self.cmdline_type == ':' {
            self.buffer.begin_undo_group(self.cursor);
            ex::execute(self, &line);
            self.buffer.end_undo_group();
            return;
        }
        // '/' or '?': an empty pattern reuses the last one.
        if !line.is_empty() {
            self.last_search = line;
        }
        self.last_search_forward = self.cmdline_type == '/';
        let next = MotionKeys {
            key: 'n',
            arg: '\0',
        };
        if let Some(target) = self.eval_motion(next, 1, false, false) {
            self.move_to(target);
        }
    }

    /// The next match of `re` after (or before) `from`, wrapping around the
    /// buffer. The flag says whether the search wrapped.
    fn find_match(
        &mut self,
        re: &Regex,
        forward: bool,
        from: Position,
    ) -> Option<(Position, bool)> {
        let lines = self.buffer.line_count();
        // Visit every line once starting at the cursor line, then the cursor
        // line again (the part on the other side of the cursor).
        for step in 0..=lines {
            let row = if forward {
                (from.row + step) % lines
            } else {
                (from.row + lines - step % lines) % lines
            };
            // All match start columns in a line (non-overlapping, left to
            // right).
            let mut cols =
                re.find_iter(self.buffer.line(row)).map(|m| m.start());
            let hit = if forward {
                cols.find(|&c| step > 0 || c > from.col)
            } else {
                cols.filter(|&c| step > 0 || c < from.col).last()
            };
            if let Some(col) = hit {
                let wrapped = if forward {
                    from.row + step >= lines
                } else {
                    step > from.row
                };
                return Some((Position::new(row, col), wrapped));
            }
        }
        self.show_error(format!("E486: Pattern not found: {}", re.as_str()));
        None
    }

    // ---- Cursor helpers ----------------------------------------------------

    fn cur_line(&self) -> &str {
        self.buffer.line(self.cursor.row)
    }

    fn clamp_cursor(&mut self) {
        self.cursor.row = self.cursor.row.min(self.buffer.last_row());
        let line = self.buffer.line(self.cursor.row);
        let mut col = self.cursor.col;
        // In insert mode the cursor may sit after the last character.
        if matches!(self.mode, Mode::Insert | Mode::CommandLine) {
            col = col.min(line.len());
        } else if col >= line.len() {
            col = utf8::prev(line, line.len());
        }
        // Never point into the middle of a UTF-8 sequence.
        self.cursor.col = utf8::floor(line, col);
    }

    fn update_want_col(&mut self) {
        self.want_col = utf8::display_col(
            self.cur_line(),
            self.cursor.col,
            self.options.tabstop,
        );
    }

    fn col_for_want(&self, row: usize) -> usize {
        utf8::byte_col_at(
            self.buffer.line(row),
            self.want_col,
            self.options.tabstop,
        )
    }

    fn indent_of(&self, row: usize) -> String {
        self.buffer.line(row)[..self.buffer.first_non_blank(row)].to_string()
    }

    fn scroll_to_cursor(&mut self) {
        // Keep `scrolloff` lines of context above and below the cursor.
        let so = self.options.scrolloff.min((self.view_rows - 1) / 2);
        let row = self.cursor.row;
        if row < self.view_top + so {
            self.view_top = row - row.min(so);
        } else if row + so >= self.view_top + self.view_rows {
            self.view_top = row + so + 1 - self.view_rows;
        }
        self.view_top = self.view_top.min(self.buffer.last_row());

        let col = utf8::display_col(
            self.cur_line(),
            self.cursor.col,
            self.options.tabstop,
        );
        if col < self.view_left {
            self.view_left = col;
        } else if col >= self.view_left + self.view_cols {
            self.view_left = col + 1 - self.view_cols;
        }
    }
}
