#include "tui.h"
#include "../core/editor.h"
#include "../core/keys.h"
#include "../utils/utf8.h"
#include "lex.h"

#include <cctype>
#include <cstdio>
#include <notcurses/notcurses.h>
#include <stdexcept>
#include <string>

namespace {

namespace color {
constexpr uint32_t background = 0x282C34;
constexpr uint32_t foreground = 0xABB2BF;
constexpr uint32_t gutter = 0x5C6370;
constexpr uint32_t selection = 0x3E4F6A;
constexpr uint32_t status_bg = 0x3E4452;
constexpr uint32_t error = 0xE06C75;
} // namespace color

std::size_t digits(std::size_t n) {
    std::size_t d = 1;
    while (n >= 10) {
        n /= 10;
        ++d;
    }
    return d;
}

// Width of the line-number column, including one space of padding.
std::size_t gutter_width(const Editor& editor) {
    if (!editor.options().number) {
        return 0;
    }
    return std::max<std::size_t>(3, digits(editor.buffer().line_count())) + 1;
}

bool in_selection(const Editor::Selection& sel, const Position pos) {
    if (sel.linewise) {
        return pos.row >= sel.start.row && pos.row <= sel.end.row;
    }
    return pos >= sel.start && pos <= sel.end;
}

} // namespace

Tui::Tui() {
    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;
    m_nc = notcurses_init(&opts, stdout);
    if (!m_nc) {
        throw std::runtime_error("failed to initialize notcurses");
    }
    // Let Ctrl-C/Ctrl-Z reach the editor as keys instead of signals.
    notcurses_linesigs_disable(m_nc);
    m_plane = notcurses_stdplane(m_nc);
    ncplane_dim_yx(m_plane, &m_rows, &m_cols);

    uint64_t channels = 0;
    ncchannels_set_bg_rgb(&channels, color::background);
    ncchannels_set_fg_rgb(&channels, color::foreground);
    ncplane_set_base(m_plane, " ", 0, channels);
}

Tui::~Tui() {
    notcurses_stop(m_nc);
    std::fputs("\033[0 q", stdout); // restore the terminal's cursor shape
    std::fflush(stdout);
}

bool Tui::closed() const {
    return m_closed;
}

void Tui::render(Editor& editor) {
    ncplane_dim_yx(m_plane, &m_rows, &m_cols);
    if (m_rows < 3 || m_cols < 8) {
        return; // too small to draw anything useful
    }
    const std::size_t gutter = gutter_width(editor);
    // Tell the editor how much text fits so it can scroll.
    editor.set_view_size(m_rows - 2, m_cols - gutter);

    ncplane_erase(m_plane);
    draw_text(editor, gutter);
    draw_status_line(editor);
    draw_command_line(editor);
    place_cursor(editor, gutter);
    notcurses_render(m_nc);
    update_cursor_shape(editor.mode());
}

void Tui::draw_text(const Editor& editor, const std::size_t gutter) {
    const Buffer& buffer = editor.buffer();
    const std::size_t tabstop = editor.options().tabstop;
    const std::size_t text_cols = m_cols - gutter;
    const std::size_t left = editor.view_left();
    const auto selection = editor.selection();
    const bool highlight = lex::supports_file(editor.filepath());

    for (std::size_t y = 0; y + 2 < m_rows; ++y) {
        const std::size_t row = editor.view_top() + y;
        const int iy = static_cast<int>(y);

        ncplane_set_bg_rgb(m_plane, color::background);
        ncplane_set_fg_rgb(m_plane, color::gutter);
        if (row >= buffer.line_count()) {
            ncplane_putstr_yx(m_plane, iy, 0, "~");
            continue;
        }
        if (gutter > 0) {
            const std::string num = std::to_string(row + 1);
            ncplane_putstr_yx(m_plane, iy,
                              static_cast<int>(gutter - 1 - num.size()),
                              num.c_str());
        }

        const std::string& line = buffer.line(row);
        const auto types =
            highlight ? lex::highlight_line(line) : std::vector<TokenType>{};
        std::size_t col = 0; // display column of the current character
        for (std::size_t i = 0; i < line.size(); i = utf8::next(line, i)) {
            if (col >= left + text_cols) {
                break;
            }
            const char32_t cp = utf8::decode(line, i);
            std::string glyph;
            std::size_t w;
            if (cp == '\t') {
                w = tabstop - col % tabstop;
                glyph.assign(w, ' ');
            } else if (cp < 0x20 || cp == 0x7F) {
                w = 2;
                glyph = {'^', static_cast<char>(cp == 0x7F ? '?' : cp + '@')};
            } else {
                w = static_cast<std::size_t>(utf8::width(cp));
                glyph = line.substr(i, utf8::next(line, i) - i);
            }
            if (col + w > left && col + w <= left + text_cols) {
                const bool selected =
                    selection && in_selection(*selection, {row, i});
                ncplane_set_bg_rgb(m_plane, selected ? color::selection
                                                     : color::background);
                ncplane_set_fg_rgb(m_plane, highlight && i < types.size()
                                                ? lex::color_of(types[i])
                                                : color::foreground);
                if (col < left) {
                    glyph.assign(col + w - left, ' '); // partly scrolled off
                }
                ncplane_putstr_yx(
                    m_plane, iy,
                    static_cast<int>(gutter + (col < left ? 0 : col - left)),
                    glyph.c_str());
            }
            col += w;
        }
        // Show selected empty lines as a single highlighted cell.
        if (line.empty() && selection && in_selection(*selection, {row, 0})) {
            ncplane_set_bg_rgb(m_plane, color::selection);
            ncplane_putstr_yx(m_plane, iy, static_cast<int>(gutter), " ");
        }
    }
}

void Tui::draw_status_line(const Editor& editor) {
    const int y = static_cast<int>(m_rows - 2);
    ncplane_set_bg_rgb(m_plane, color::status_bg);
    ncplane_set_fg_rgb(m_plane, color::foreground);
    ncplane_putstr_yx(m_plane, y, 0, std::string(m_cols, ' ').c_str());

    std::string name =
        editor.filepath().empty() ? "[No Name]" : editor.filepath();
    if (editor.buffer().is_modified()) {
        name += " [+]";
    }
    ncplane_putstr_yx(m_plane, y, 1, name.c_str());

    const Position cur = editor.cursor();
    const std::size_t total = editor.buffer().line_count();
    std::string where;
    if (total <= m_rows - 2) {
        where = "All";
    } else if (editor.view_top() == 0) {
        where = "Top";
    } else if (editor.view_top() + (m_rows - 2) >= total) {
        where = "Bot";
    } else {
        where = std::to_string(editor.view_top() * 100 / total) + "%";
    }
    const std::string ruler = std::to_string(cur.row + 1) + "," +
                              std::to_string(cur.col + 1) + "   " + where + " ";
    if (ruler.size() < m_cols) {
        ncplane_putstr_yx(m_plane, y, static_cast<int>(m_cols - ruler.size()),
                          ruler.c_str());
    }
}

void Tui::draw_command_line(const Editor& editor) {
    const int y = static_cast<int>(m_rows - 1);
    ncplane_set_bg_rgb(m_plane, color::background);
    ncplane_set_fg_rgb(m_plane, color::foreground);

    std::string text;
    if (editor.mode() == Mode::CommandLine) {
        text = editor.command_line();
    } else if (!editor.message().empty()) {
        text = editor.message();
        if (editor.message_is_error()) {
            ncplane_set_fg_rgb(m_plane, color::error);
        }
    } else {
        switch (editor.mode()) {
        case Mode::Insert:
            text = "-- INSERT --";
            break;
        case Mode::Visual:
            text = "-- VISUAL --";
            break;
        case Mode::VisualLine:
            text = "-- VISUAL LINE --";
            break;
        default:
            break;
        }
    }
    ncplane_putstr_yx(m_plane, y, 0, text.c_str());

    // "showcmd": the keys of an unfinished command, e.g. "2d".
    if (const std::string pending = editor.pending_keys();
        !pending.empty() && m_cols > 20) {
        ncplane_set_fg_rgb(m_plane, color::foreground);
        ncplane_putstr_yx(m_plane, y, static_cast<int>(m_cols - 12),
                          pending.c_str());
    }
}

void Tui::place_cursor(const Editor& editor, const std::size_t gutter) {
    if (editor.mode() == Mode::CommandLine) {
        const std::string text = editor.command_line();
        notcurses_cursor_enable(
            m_nc, static_cast<int>(m_rows - 1),
            static_cast<int>(utf8::display_col(text, text.size(), 8)));
        return;
    }
    const Position cur = editor.cursor();
    const std::size_t col = utf8::display_col(
        editor.buffer().line(cur.row), cur.col, editor.options().tabstop);
    notcurses_cursor_enable(
        m_nc, static_cast<int>(cur.row - editor.view_top()),
        static_cast<int>(gutter + col - editor.view_left()));
}

void Tui::update_cursor_shape(const Mode mode) {
    const Mode shape = mode == Mode::Insert || mode == Mode::CommandLine
                           ? Mode::Insert
                           : Mode::Normal;
    if (m_shape_mode == shape) {
        return;
    }
    m_shape_mode = shape;
    // DECSCUSR: 6 = steady bar, 2 = steady block. notcurses has no API for
    // this, so write it directly after the frame has been flushed.
    std::fputs(shape == Mode::Insert ? "\033[6 q" : "\033[2 q", stdout);
    std::fflush(stdout);
}

std::optional<int> Tui::read_key() {
    ncinput ni{};
    const uint32_t id = notcurses_get(m_nc, nullptr, &ni);
    if (id == static_cast<uint32_t>(-1) || id == NCKEY_EOF) {
        m_closed = true;
        return std::nullopt;
    }
    if (ni.evtype == NCTYPE_RELEASE) {
        return std::nullopt;
    }
    switch (id) {
    case NCKEY_RESIZE:
        notcurses_refresh(m_nc, &m_rows, &m_cols);
        return std::nullopt;
    case NCKEY_ENTER:
    case '\n':
        return key::Enter;
    case NCKEY_BACKSPACE:
    case 0x08:
        return key::Backspace;
    case NCKEY_TAB:
        return key::Tab;
    case NCKEY_ESC:
        return key::Esc;
    case NCKEY_UP:
        return key::Up;
    case NCKEY_DOWN:
        return key::Down;
    case NCKEY_LEFT:
        return key::Left;
    case NCKEY_RIGHT:
        return key::Right;
    case NCKEY_DEL:
        return key::Delete;
    case NCKEY_HOME:
        return key::Home;
    case NCKEY_END:
        return key::End;
    case NCKEY_PGUP:
        return key::PageUp;
    case NCKEY_PGDOWN:
        return key::PageDown;
    default:
        break;
    }
    if (nckey_synthesized_p(id)) {
        return std::nullopt; // mouse events, function keys, ...
    }
    // notcurses reports Ctrl+letter as the letter plus a modifier.
    if (ncinput_ctrl_p(&ni) && id < 0x80 &&
        std::isalpha(static_cast<int>(id))) {
        return key::ctrl(static_cast<char>(std::tolower(static_cast<int>(id))));
    }
    return static_cast<int>(id);
}
