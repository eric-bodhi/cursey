//! Terminal front end built on ratatui + crossterm. It draws the editor's
//! state and turns terminal input into editor keys (see `cursey_core::keys`).
//! All editing logic lives in `Editor`; this type holds no editor state of
//! its own.
//!
//! Screen layout:
//!   rows 0 .. n-3   text, with a line-number gutter on the left
//!   row  n-2        status line (file name, modified flag, cursor position)
//!   row  n-1        command line (messages, mode, ":" commands)

use crate::lex;
use cursey_core::editor::{Editor, Selection};
use cursey_core::keys;
use cursey_core::types::{Mode, Position};
use cursey_core::utf8;
use ratatui::buffer::Buffer as Screen;
use ratatui::crossterm::cursor::SetCursorStyle;
use ratatui::crossterm::event::{
    self, Event, KeyCode, KeyEventKind, KeyModifiers,
};
use ratatui::crossterm::execute;
use ratatui::layout::Rect;
use ratatui::style::{Color, Style};
use ratatui::{DefaultTerminal, Frame};
use std::io;

const BACKGROUND: Color = Color::from_u32(0x282C34);
const FOREGROUND: Color = Color::from_u32(0xABB2BF);
const GUTTER: Color = Color::from_u32(0x5C6370);
const SELECTION: Color = Color::from_u32(0x3E4F6A);
const STATUS_BG: Color = Color::from_u32(0x3E4452);
const ERROR: Color = Color::from_u32(0xE06C75);

fn to_u16(n: usize) -> u16 {
    u16::try_from(n).unwrap_or(u16::MAX)
}

/// Width of the line-number column, including one space of padding.
fn gutter_width(editor: &Editor) -> usize {
    if !editor.options().number {
        return 0;
    }
    editor.buffer().line_count().to_string().len().max(3) + 1
}

fn in_selection(sel: &Selection, pos: Position) -> bool {
    if sel.linewise {
        return (sel.start.row..=sel.end.row).contains(&pos.row);
    }
    (sel.start..=sel.end).contains(&pos)
}

/// Write `text` with its left edge at column `x` of row `y`, clipped to the
/// screen.
fn put(screen: &mut Screen, x: usize, y: usize, text: &str, style: Style) {
    if y < usize::from(screen.area.height) && x < usize::from(screen.area.width)
    {
        screen.set_string(to_u16(x), to_u16(y), text, style);
    }
}

pub struct Tui {
    terminal: DefaultTerminal,
    shape_mode: Option<Mode>,
    closed: bool,
}

impl Tui {
    /// Switch the terminal to raw mode and the alternate screen. In raw mode
    /// Ctrl-C/Ctrl-Z reach the editor as keys instead of signals.
    pub fn new() -> io::Result<Self> {
        Ok(Self {
            terminal: ratatui::try_init()?,
            shape_mode: None,
            closed: false,
        })
    }

    /// The terminal went away (input failed).
    pub fn closed(&self) -> bool {
        self.closed
    }

    pub fn render(&mut self, editor: &mut Editor) -> io::Result<()> {
        self.terminal.draw(|frame| draw(frame, editor))?;
        self.update_cursor_shape(editor.mode())
    }

    fn update_cursor_shape(&mut self, mode: Mode) -> io::Result<()> {
        let shape = if matches!(mode, Mode::Insert | Mode::CommandLine) {
            Mode::Insert
        } else {
            Mode::Normal
        };
        if self.shape_mode == Some(shape) {
            return Ok(());
        }
        self.shape_mode = Some(shape);
        let style = if shape == Mode::Insert {
            SetCursorStyle::SteadyBar
        } else {
            SetCursorStyle::SteadyBlock
        };
        execute!(self.terminal.backend_mut(), style)
    }

    /// Blocks until a key is pressed. Returns `None` for events that are not
    /// keys (resize, key release, function keys).
    pub fn read_key(&mut self) -> Option<char> {
        let Ok(event) = event::read() else {
            self.closed = true;
            return None;
        };
        // A resize needs no handling: ratatui picks up the new size on the
        // next draw.
        let Event::Key(key) = event else {
            return None;
        };
        if key.kind == KeyEventKind::Release {
            return None;
        }
        let k = match key.code {
            KeyCode::Enter => keys::ENTER,
            KeyCode::Backspace => keys::BACKSPACE,
            KeyCode::Tab => keys::TAB,
            KeyCode::Esc => keys::ESC,
            KeyCode::Up => keys::UP,
            KeyCode::Down => keys::DOWN,
            KeyCode::Left => keys::LEFT,
            KeyCode::Right => keys::RIGHT,
            KeyCode::Delete => keys::DELETE,
            KeyCode::Home => keys::HOME,
            KeyCode::End => keys::END,
            KeyCode::PageUp => keys::PAGE_UP,
            KeyCode::PageDown => keys::PAGE_DOWN,
            // crossterm reports Ctrl+letter as the letter plus a modifier.
            // Raw ^J and ^H bytes arrive that way too and mean Enter and
            // Backspace.
            KeyCode::Char(c)
                if key.modifiers.contains(KeyModifiers::CONTROL)
                    && c.is_ascii_alphabetic() =>
            {
                match keys::ctrl(c.to_ascii_lowercase()) {
                    '\n' => keys::ENTER,
                    '\x08' => keys::BACKSPACE,
                    k => k,
                }
            }
            KeyCode::Char(c) => c,
            _ => return None,
        };
        Some(k)
    }
}

impl Drop for Tui {
    fn drop(&mut self) {
        // Nothing useful to do if restoring the terminal fails.
        let _ = self.terminal.show_cursor();
        let backend = self.terminal.backend_mut();
        let _ = execute!(backend, SetCursorStyle::DefaultUserShape);
        ratatui::restore();
    }
}

fn draw(frame: &mut Frame, editor: &mut Editor) {
    let area = frame.area();
    let rows = usize::from(area.height);
    let cols = usize::from(area.width);
    if rows < 3 || cols < 8 {
        return; // too small to draw anything useful
    }
    let gutter = gutter_width(editor);
    // Tell the editor how much text fits so it can scroll.
    editor.set_view_size(rows - 2, cols.saturating_sub(gutter));

    let screen = frame.buffer_mut();
    screen.set_style(area, Style::new().fg(FOREGROUND).bg(BACKGROUND));
    draw_text(screen, editor, gutter);
    draw_status_line(screen, editor);
    draw_command_line(screen, editor);
    let (x, y) = cursor_position(editor, gutter, rows);
    frame.set_cursor_position((
        to_u16(x).min(area.width - 1),
        to_u16(y).min(area.height - 1),
    ));
}

fn draw_text(screen: &mut Screen, editor: &Editor, gutter: usize) {
    let rows = usize::from(screen.area.height);
    let text_cols = usize::from(screen.area.width).saturating_sub(gutter);
    let buffer = editor.buffer();
    let tabstop = editor.options().tabstop;
    let left = editor.view_left();
    let selection = editor.selection();
    let selected =
        |pos| selection.as_ref().is_some_and(|sel| in_selection(sel, pos));
    let highlight = lex::supports_file(editor.filepath());
    let gutter_style = Style::new().fg(GUTTER).bg(BACKGROUND);

    for y in 0..rows - 2 {
        let row = editor.view_top() + y;
        if row >= buffer.line_count() {
            put(screen, 0, y, "~", gutter_style);
            continue;
        }
        if gutter > 0 {
            let num = (row + 1).to_string();
            put(screen, gutter - 1 - num.len(), y, &num, gutter_style);
        }

        let line = buffer.line(row);
        let types = if highlight {
            lex::highlight_line(line)
        } else {
            Vec::new()
        };
        let mut col = 0; // display column of the current character
        for (i, c) in line.char_indices() {
            if col >= left + text_cols {
                break;
            }
            let (mut glyph, w) = if c == '\t' {
                let w = tabstop - col % tabstop;
                (" ".repeat(w), w)
            } else if c < ' ' || c == '\x7f' {
                let shown = if c == '\x7f' {
                    '?'
                } else {
                    (c as u8 + b'@') as char
                };
                (format!("^{shown}"), 2)
            } else {
                (c.to_string(), utf8::width(c))
            };
            if col + w > left && col + w <= left + text_cols {
                let fg = match types.get(i) {
                    Some(&kind) => Color::from_u32(lex::color_of(kind)),
                    None => FOREGROUND,
                };
                let bg = if selected(Position::new(row, i)) {
                    SELECTION
                } else {
                    BACKGROUND
                };
                if col < left {
                    glyph = " ".repeat(col + w - left); // partly scrolled off
                }
                let x = gutter + col.saturating_sub(left);
                put(screen, x, y, &glyph, Style::new().fg(fg).bg(bg));
            }
            col += w;
        }
        // Show selected empty lines as a single highlighted cell.
        if line.is_empty() && selected(Position::new(row, 0)) {
            put(screen, gutter, y, " ", Style::new().bg(SELECTION));
        }
    }
}

fn draw_status_line(screen: &mut Screen, editor: &Editor) {
    let rows = usize::from(screen.area.height);
    let cols = usize::from(screen.area.width);
    let y = rows - 2;
    let style = Style::new().fg(FOREGROUND).bg(STATUS_BG);
    screen.set_style(Rect::new(0, to_u16(y), screen.area.width, 1), style);

    let mut name = if editor.filepath().is_empty() {
        "[No Name]".to_string()
    } else {
        editor.filepath().to_string()
    };
    if editor.buffer().is_modified() {
        name.push_str(" [+]");
    }
    put(screen, 1, y, &name, style);

    let cur = editor.cursor();
    let total = editor.buffer().line_count();
    let text_rows = rows - 2;
    let top = editor.view_top();
    let place = if total <= text_rows {
        "All".to_string()
    } else if top == 0 {
        "Top".to_string()
    } else if top + text_rows >= total {
        "Bot".to_string()
    } else {
        format!("{}%", top * 100 / total)
    };
    let ruler = format!("{},{}   {place} ", cur.row + 1, cur.col + 1);
    if ruler.len() < cols {
        put(screen, cols - ruler.len(), y, &ruler, style);
    }
}

fn draw_command_line(screen: &mut Screen, editor: &Editor) {
    let rows = usize::from(screen.area.height);
    let cols = usize::from(screen.area.width);
    let y = rows - 1;
    let style = Style::new().fg(FOREGROUND).bg(BACKGROUND);

    let (text, text_style) = if editor.mode() == Mode::CommandLine {
        (editor.command_line(), style)
    } else if !editor.message().is_empty() {
        let fg = if editor.message_is_error() {
            ERROR
        } else {
            FOREGROUND
        };
        (editor.message().to_string(), style.fg(fg))
    } else {
        let mode = match editor.mode() {
            Mode::Insert => "-- INSERT --",
            Mode::Visual => "-- VISUAL --",
            Mode::VisualLine => "-- VISUAL LINE --",
            _ => "",
        };
        (mode.to_string(), style)
    };
    put(screen, 0, y, &text, text_style);

    // "showcmd": the keys of an unfinished command, e.g. "2d".
    let pending = editor.pending_keys();
    if !pending.is_empty() && cols > 20 {
        put(screen, cols - 12, y, &pending, style);
    }
}

/// Screen (column, row) of the cursor.
fn cursor_position(
    editor: &Editor,
    gutter: usize,
    rows: usize,
) -> (usize, usize) {
    if editor.mode() == Mode::CommandLine {
        let text = editor.command_line();
        return (utf8::display_col(&text, text.len(), 8), rows - 1);
    }
    let cur = editor.cursor();
    let col = utf8::display_col(
        editor.buffer().line(cur.row),
        cur.col,
        editor.options().tabstop,
    );
    (
        (gutter + col).saturating_sub(editor.view_left()),
        cur.row.saturating_sub(editor.view_top()),
    )
}
