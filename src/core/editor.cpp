#include "editor.h"
#include "../commands/ex_commands.h"
#include "../utils/utf8.h"
#include "keys.h"
#include "motions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <string_view>

namespace {

constexpr std::size_t want_eol = std::string::npos;

bool is_digit(const int k) {
    return k >= '0' && k <= '9';
}

// A key that inserts itself as text (not a control or special key).
bool is_text_key(const int k) {
    return k >= 0x20 && k != key::Backspace && k < key::SpecialBase;
}

// Read an optional count at keys[i]. Returns 0 when there is none. A leading
// '0' is the "go to column 0" spec, not a count.
int read_count(const std::vector<int>& keys, std::size_t& i) {
    int count = 0;
    while (i < keys.size() && is_digit(keys[i]) &&
           !(count == 0 && keys[i] == '0')) {
        count = std::min(count * 10 + (keys[i] - '0'), 99999);
        ++i;
    }
    return count;
}

std::string summary(const std::string& path, const Buffer& buf) {
    std::size_t bytes = 0;
    for (std::size_t r = 0; r < buf.line_count(); ++r) {
        bytes += buf.line_length(r) + 1;
    }
    return "\"" + path + "\" " + std::to_string(buf.line_count()) + "L, " +
           std::to_string(bytes) + "B";
}

} // namespace

Editor::Editor() = default;

// ---- Files
// -------------------------------------------------------------------

bool Editor::open(const std::string& path) {
    m_filepath = path;
    m_cursor = {};
    m_want_col = 0;
    m_view_top = 0;
    m_view_left = 0;
    if (!std::filesystem::exists(path)) {
        m_buffer.set_text("");
        show_message("\"" + path + "\" [New]");
        return true;
    }
    std::string error;
    if (!m_buffer.load(path, error)) {
        m_buffer.set_text("");
        show_error(error);
        return false;
    }
    show_message(summary(path, m_buffer));
    return true;
}

bool Editor::write(const std::string& path) {
    const std::string target = path.empty() ? m_filepath : path;
    if (target.empty()) {
        show_error("E32: No file name");
        return false;
    }
    std::string error;
    if (!m_buffer.save(target, error)) {
        show_error(error);
        return false;
    }
    if (m_filepath.empty()) {
        m_filepath = target;
    }
    show_message(summary(target, m_buffer) + " written");
    return true;
}

void Editor::quit(const bool force) {
    if (!force && m_buffer.is_modified()) {
        show_error("E37: No write since last change (add ! to override)");
        return;
    }
    m_should_quit = true;
}

bool Editor::edit(const std::string& path, const bool force) {
    if (!force && m_buffer.is_modified()) {
        show_error("E37: No write since last change (add ! to override)");
        return false;
    }
    const std::string target = path.empty() ? m_filepath : path;
    if (target.empty()) {
        show_error("E32: No file name");
        return false;
    }
    return open(target);
}

void Editor::goto_line(const std::size_t row) {
    m_cursor = {std::min(row, m_buffer.last_row()), 0};
    m_cursor.col = m_buffer.first_non_blank(m_cursor.row);
    clamp_cursor();
    update_want_col();
}

void Editor::show_message(std::string text) {
    m_message = std::move(text);
    m_message_error = false;
}

void Editor::show_error(std::string text) {
    m_message = std::move(text);
    m_message_error = true;
}

// ---- Accessors
// ---------------------------------------------------------------

const Buffer& Editor::buffer() const {
    return m_buffer;
}

Buffer& Editor::buffer() {
    return m_buffer;
}

Position Editor::cursor() const {
    return m_cursor;
}

Mode Editor::mode() const {
    return m_mode;
}

std::optional<Editor::Selection> Editor::selection() const {
    if (!is_visual(m_mode)) {
        return std::nullopt;
    }
    return Selection{std::min(m_visual_anchor, m_cursor),
                     std::max(m_visual_anchor, m_cursor),
                     m_mode == Mode::VisualLine};
}

const std::string& Editor::filepath() const {
    return m_filepath;
}

const Options& Editor::options() const {
    return m_options;
}

Options& Editor::options() {
    return m_options;
}

bool Editor::should_quit() const {
    return m_should_quit;
}

const std::string& Editor::message() const {
    return m_message;
}

bool Editor::message_is_error() const {
    return m_message_error;
}

std::string Editor::command_line() const {
    if (m_mode != Mode::CommandLine) {
        return {};
    }
    return m_cmdline_type + m_cmdline;
}

std::string Editor::pending_keys() const {
    std::string out;
    for (const int k : m_pending) {
        out += key::to_notation(k);
    }
    return out;
}

void Editor::set_view_size(const std::size_t rows, const std::size_t cols) {
    m_view_rows = std::max<std::size_t>(rows, 1);
    m_view_cols = std::max<std::size_t>(cols, 1);
    scroll_to_cursor();
}

std::size_t Editor::view_top() const {
    return m_view_top;
}

std::size_t Editor::view_left() const {
    return m_view_left;
}

// ---- Key dispatch
// ------------------------------------------------------------

void Editor::handle_key(int input) {
    if (input == key::ctrl('c')) {
        input = key::Esc; // like vim, Ctrl-C cancels
    }
    if (m_mode != Mode::CommandLine) {
        m_message.clear();
    }
    switch (m_mode) {
    case Mode::Normal:
        handle_normal_key(input);
        break;
    case Mode::Insert:
        handle_insert_key(input);
        break;
    case Mode::Visual:
    case Mode::VisualLine:
        handle_visual_key(input);
        break;
    case Mode::CommandLine:
        handle_command_line_key(input);
        break;
    }
    clamp_cursor();
    scroll_to_cursor();
}

void Editor::handle_normal_key(const int input) {
    if (input == key::Esc) {
        m_pending.clear();
        return;
    }
    m_pending.push_back(input);

    // Every normal command is one undo step. Commands that enter insert mode
    // leave the group open until <Esc>, so the typed text joins the step.
    m_buffer.begin_undo_group(m_cursor);
    const ParseResult result = run_normal_command(m_pending);
    if (m_mode != Mode::Insert) {
        m_buffer.end_undo_group();
    }
    if (result != ParseResult::Incomplete) {
        m_pending.clear();
    }
}

void Editor::handle_visual_key(const int input) {
    if (input == key::Esc) {
        m_pending.clear();
        m_mode = Mode::Normal;
        return;
    }
    m_pending.push_back(input);
    m_buffer.begin_undo_group(m_cursor);
    const ParseResult result = run_visual_command(m_pending);
    if (m_mode != Mode::Insert) {
        m_buffer.end_undo_group();
    }
    if (result != ParseResult::Incomplete) {
        m_pending.clear();
    }
}

void Editor::handle_insert_key(const int input) {
    const std::string& line = m_buffer.line(m_cursor.row);
    switch (input) {
    case key::Esc:
        m_mode = Mode::Normal;
        m_buffer.end_undo_group();
        if (m_cursor.col > 0) {
            m_cursor.col = utf8::prev(line, m_cursor.col);
        }
        break;
    case key::Enter: {
        const std::string indent =
            m_options.autoindent ? indent_of(m_cursor.row) : "";
        m_cursor = m_buffer.insert(m_cursor, "\n" + indent);
        break;
    }
    case key::Backspace:
    case key::ctrl('h'):
        if (m_cursor.col > 0) {
            const std::size_t prev = utf8::prev(line, m_cursor.col);
            m_buffer.erase({m_cursor.row, prev}, m_cursor);
            m_cursor.col = prev;
        } else if (m_cursor.row > 0) {
            // Join with the previous line.
            const Position join{m_cursor.row - 1,
                                m_buffer.line_length(m_cursor.row - 1)};
            m_buffer.erase(join, m_cursor);
            m_cursor = join;
        }
        break;
    case key::Delete:
        if (m_cursor.col < line.size()) {
            m_buffer.erase(m_cursor,
                           {m_cursor.row, utf8::next(line, m_cursor.col)});
        } else if (m_cursor.row < m_buffer.last_row()) {
            m_buffer.erase(m_cursor, {m_cursor.row + 1, 0});
        }
        break;
    case key::Tab:
        if (m_options.expandtab) {
            const std::size_t col =
                utf8::display_col(line, m_cursor.col, m_options.tabstop);
            const std::size_t n = m_options.tabstop - col % m_options.tabstop;
            m_cursor = m_buffer.insert(m_cursor, std::string(n, ' '));
        } else {
            m_cursor = m_buffer.insert(m_cursor, "\t");
        }
        break;
    case key::Left:
        m_cursor.col = utf8::prev(line, m_cursor.col);
        break;
    case key::Right:
        m_cursor.col = utf8::next(line, m_cursor.col);
        break;
    case key::Up:
    case key::Down:
        if (input == key::Up && m_cursor.row > 0) {
            --m_cursor.row;
        } else if (input == key::Down && m_cursor.row < m_buffer.last_row()) {
            ++m_cursor.row;
        }
        m_cursor.col = col_for_want(m_cursor.row);
        return; // keep the desired column
    case key::Home:
        m_cursor.col = 0;
        break;
    case key::End:
        m_cursor.col = line.size();
        break;
    default:
        if (is_text_key(input)) {
            m_cursor = m_buffer.insert(
                m_cursor, utf8::encode(static_cast<char32_t>(input)));
        }
        break;
    }
    update_want_col();
}

void Editor::handle_command_line_key(const int input) {
    switch (input) {
    case key::Esc:
        m_mode = Mode::Normal;
        break;
    case key::Enter:
        m_mode = Mode::Normal;
        execute_command_line();
        break;
    case key::Backspace:
    case key::ctrl('h'):
        if (m_cmdline.empty()) {
            m_mode = Mode::Normal;
        } else {
            m_cmdline.erase(utf8::prev(m_cmdline, m_cmdline.size()));
        }
        break;
    case key::ctrl('u'):
        m_cmdline.clear();
        break;
    default:
        if (is_text_key(input)) {
            m_cmdline += utf8::encode(static_cast<char32_t>(input));
        }
        break;
    }
}

// ---- Normal-mode grammar
// -----------------------------------------------------
//
//   ["x] [count] command
//   ["x] [count] operator [count] motion
//   ["x] [count] operator operator          (dd, cc, yy: whole lines)
//
// The pending keys are re-parsed from the start on every key press. That keeps
// the parser stateless: it either finds a complete command, needs more keys,
// or rejects the sequence.

Editor::ParseResult Editor::parse_motion(const std::vector<int>& keys,
                                         std::size_t& i, MotionKeys& out) {
    if (i >= keys.size()) {
        return ParseResult::Incomplete;
    }
    out = {keys[i++], 0};
    switch (out.key) {
    case 'g':
        if (i >= keys.size()) {
            return ParseResult::Incomplete;
        }
        out.arg = keys[i++];
        return out.arg == 'g' || out.arg == 'e' || out.arg == 'E'
                   ? ParseResult::Done
                   : ParseResult::Invalid;
    case 'f':
    case 'F':
    case 't':
    case 'T':
        if (i >= keys.size()) {
            return ParseResult::Incomplete;
        }
        out.arg = keys[i++];
        return is_text_key(out.arg) ? ParseResult::Done : ParseResult::Invalid;
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case 'w':
    case 'W':
    case 'b':
    case 'B':
    case 'e':
    case 'E':
    case '0':
    case '^':
    case '$':
    case 'G':
    case 'n':
    case 'N':
    case ';':
    case ',':
    case '+':
    case '-':
    case ' ':
    case key::Enter:
    case key::Backspace:
    case key::Left:
    case key::Right:
    case key::Up:
    case key::Down:
    case key::Home:
    case key::End:
        return ParseResult::Done;
    default:
        return ParseResult::Invalid;
    }
}

Editor::ParseResult Editor::run_normal_command(const std::vector<int>& keys) {
    std::size_t i = 0;
    char reg = '"';
    if (keys[i] == '"') {
        if (keys.size() < 2) {
            return ParseResult::Incomplete;
        }
        if (keys[1] > 0x7F ||
            !Registers::is_valid(static_cast<char>(keys[1]))) {
            return ParseResult::Invalid;
        }
        reg = static_cast<char>(keys[1]);
        i = 2;
    }
    const int count1 = read_count(keys, i);
    if (i >= keys.size()) {
        return ParseResult::Incomplete;
    }
    const int count = std::max(count1, 1);
    const int k = keys[i++];

    // Shorthands that are defined in terms of an operator + motion.
    std::string_view alias;
    switch (k) {
    case 'x':
    case key::Delete:
        alias = "dl";
        break;
    case 'X':
        alias = "dh";
        break;
    case 'D':
        alias = "d$";
        break;
    case 'C':
        alias = "c$";
        break;
    case 's':
        alias = "cl";
        break;
    case 'S':
        alias = "cc";
        break;
    case 'Y':
        alias = "yy";
        break;
    default:
        break;
    }
    if (!alias.empty()) {
        std::vector<int> expanded(
            keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(i - 1));
        expanded.insert(expanded.end(), alias.begin(), alias.end());
        return run_normal_command(expanded);
    }

    // Operators: d c y
    if (k == 'd' || k == 'c' || k == 'y') {
        const int count2 = read_count(keys, i);
        if (i >= keys.size()) {
            return ParseResult::Incomplete;
        }
        const int total = count * std::max(count2, 1);
        const bool has_count = count1 > 0 || count2 > 0;
        if (keys[i] == k) {
            // dd / cc / yy: `total` whole lines starting at the cursor.
            const std::size_t last =
                std::min(m_cursor.row + static_cast<std::size_t>(total) - 1,
                         m_buffer.last_row());
            apply_operator(k, m_cursor, {{last, 0}, MotionType::Linewise}, reg);
            return ParseResult::Done;
        }
        MotionKeys spec;
        if (const ParseResult r = parse_motion(keys, i, spec);
            r != ParseResult::Done) {
            return r;
        }
        std::optional<MotionTarget> target;
        if (k == 'c' && (spec.key == 'w' || spec.key == 'W')) {
            // Special case: "cw" changes to the end of the word, like "ce".
            target = change_word_target(total, spec.key == 'W');
        } else {
            target = eval_motion(spec, total, has_count, true);
        }
        if (!target) {
            return ParseResult::Invalid;
        }
        apply_operator(k, m_cursor, *target, reg);
        return ParseResult::Done;
    }

    switch (k) {
    case 'i':
        enter_insert(m_cursor);
        return ParseResult::Done;
    case 'a': {
        const auto& line = m_buffer.line(m_cursor.row);
        enter_insert({m_cursor.row, utf8::next(line, m_cursor.col)});
        return ParseResult::Done;
    }
    case 'I':
        enter_insert({m_cursor.row, m_buffer.first_non_blank(m_cursor.row)});
        return ParseResult::Done;
    case 'A':
        enter_insert({m_cursor.row, m_buffer.line_length(m_cursor.row)});
        return ParseResult::Done;
    case 'o':
    case 'O':
        open_line(k == 'o');
        return ParseResult::Done;
    case 'p':
    case 'P':
        paste(reg, k == 'P', count);
        return ParseResult::Done;
    case 'J':
        join_lines(count);
        return ParseResult::Done;
    case 'r':
        if (i >= keys.size()) {
            return ParseResult::Incomplete;
        }
        return replace_chars(count, keys[i]) ? ParseResult::Done
                                             : ParseResult::Invalid;
    case 'u':
        for (int n = 0; n < count; ++n) {
            undo();
        }
        return ParseResult::Done;
    case key::ctrl('r'):
        for (int n = 0; n < count; ++n) {
            redo();
        }
        return ParseResult::Done;
    case 'v':
    case 'V':
        m_mode = k == 'v' ? Mode::Visual : Mode::VisualLine;
        m_visual_anchor = m_cursor;
        return ParseResult::Done;
    case ':':
    case '/':
    case '?':
        begin_command_line(static_cast<char>(k));
        return ParseResult::Done;
    case key::ctrl('d'):
    case key::ctrl('u'):
        scroll_half_page(k == key::ctrl('d'), count1);
        return ParseResult::Done;
    default:
        break;
    }

    // Anything else must be a motion.
    MotionKeys spec;
    --i;
    if (const ParseResult r = parse_motion(keys, i, spec);
        r != ParseResult::Done) {
        return r;
    }
    const auto target = eval_motion(spec, count, count1 > 0, false);
    if (!target) {
        return ParseResult::Invalid;
    }
    move_to(*target);
    return ParseResult::Done;
}

Editor::ParseResult Editor::run_visual_command(const std::vector<int>& keys) {
    std::size_t i = 0;
    char reg = '"';
    if (keys[i] == '"') {
        if (keys.size() < 2) {
            return ParseResult::Incomplete;
        }
        if (keys[1] > 0x7F ||
            !Registers::is_valid(static_cast<char>(keys[1]))) {
            return ParseResult::Invalid;
        }
        reg = static_cast<char>(keys[1]);
        i = 2;
    }
    const int count1 = read_count(keys, i);
    if (i >= keys.size()) {
        return ParseResult::Incomplete;
    }
    const int k = keys[i];

    const Selection sel = *selection();
    auto apply_to_selection = [&](const int op) {
        m_mode = Mode::Normal;
        const MotionType type =
            sel.linewise ? MotionType::Linewise : MotionType::Inclusive;
        apply_operator(op, sel.start, {sel.end, type}, reg);
    };

    switch (k) {
    case 'd':
    case 'x':
    case key::Delete:
        apply_to_selection('d');
        return ParseResult::Done;
    case 'y':
        apply_to_selection('y');
        return ParseResult::Done;
    case 'c':
    case 's':
        apply_to_selection('c');
        return ParseResult::Done;
    case 'o':
        std::swap(m_cursor, m_visual_anchor);
        update_want_col();
        return ParseResult::Done;
    case 'v':
    case 'V': {
        const Mode wanted = k == 'v' ? Mode::Visual : Mode::VisualLine;
        m_mode = m_mode == wanted ? Mode::Normal : wanted;
        return ParseResult::Done;
    }
    case ':':
        m_mode = Mode::Normal;
        begin_command_line(':');
        return ParseResult::Done;
    default:
        break;
    }

    MotionKeys spec;
    if (const ParseResult r = parse_motion(keys, i, spec);
        r != ParseResult::Done) {
        return r;
    }
    const auto target =
        eval_motion(spec, std::max(count1, 1), count1 > 0, false);
    if (!target) {
        return ParseResult::Invalid;
    }
    move_to(*target);
    return ParseResult::Done;
}

// ---- Motions
// -----------------------------------------------------------------

std::optional<Editor::MotionTarget>
Editor::eval_motion(const MotionKeys& spec, const int count,
                    const bool has_count, const bool for_operator) {
    const Position cur = m_cursor;
    const std::string& line = m_buffer.line(cur.row);
    const auto n = static_cast<std::size_t>(count);
    MotionTarget t{cur, MotionType::Exclusive};

    auto first_non_blank_of = [&](const std::size_t row) {
        return Position{row, m_buffer.first_non_blank(row)};
    };

    switch (spec.key) {
    case 'h':
    case key::Left:
    case key::Backspace:
        for (int k = 0; k < count && t.pos.col > 0; ++k) {
            t.pos.col = utf8::prev(line, t.pos.col);
        }
        return t;
    case 'l':
    case key::Right:
    case ' ':
        for (int k = 0; k < count && t.pos.col < line.size(); ++k) {
            t.pos.col = utf8::next(line, t.pos.col);
        }
        // Outside of operators, l stops on the last character.
        if (!for_operator && t.pos.col >= line.size()) {
            t.pos.col = line.empty() ? 0 : utf8::prev(line, line.size());
        }
        return t;
    case 'j':
    case key::Down:
    case 'k':
    case key::Up: {
        const bool down = spec.key == 'j' || spec.key == key::Down;
        if (down ? cur.row == m_buffer.last_row() : cur.row == 0) {
            return std::nullopt;
        }
        t.pos.row = down ? std::min(cur.row + n, m_buffer.last_row())
                         : cur.row - std::min(cur.row, n);
        t.pos.col = col_for_want(t.pos.row);
        t.type = MotionType::Linewise;
        t.vertical = true;
        return t;
    }
    case '+':
    case key::Enter:
        t.pos = first_non_blank_of(std::min(cur.row + n, m_buffer.last_row()));
        t.type = MotionType::Linewise;
        return t;
    case '-':
        t.pos = first_non_blank_of(cur.row - std::min(cur.row, n));
        t.type = MotionType::Linewise;
        return t;
    case 'w':
    case 'W':
        for (int k = 0; k < count; ++k) {
            t.pos = motion::word_forward(m_buffer, t.pos, spec.key == 'W');
        }
        // "dw" on the last word of a line stops at the end of that line
        // instead of eating the line break.
        if (for_operator && t.pos.row > cur.row &&
            t.pos.col <= m_buffer.first_non_blank(t.pos.row)) {
            const Position eol{t.pos.row - 1,
                               m_buffer.line_length(t.pos.row - 1)};
            if (eol > cur) {
                t.pos = eol;
            }
        }
        return t;
    case 'b':
    case 'B':
        for (int k = 0; k < count; ++k) {
            t.pos = motion::word_backward(m_buffer, t.pos, spec.key == 'B');
        }
        return t;
    case 'e':
    case 'E':
        for (int k = 0; k < count; ++k) {
            t.pos = motion::word_end(m_buffer, t.pos, spec.key == 'E');
        }
        t.type = MotionType::Inclusive;
        return t;
    case 'g':
        if (spec.arg == 'g') {
            t.pos = first_non_blank_of(
                std::min(has_count ? n - 1 : 0, m_buffer.last_row()));
            t.type = MotionType::Linewise;
        } else { // ge / gE
            for (int k = 0; k < count; ++k) {
                t.pos =
                    motion::word_end_backward(m_buffer, t.pos, spec.arg == 'E');
            }
            t.type = MotionType::Inclusive;
        }
        return t;
    case 'G':
        t.pos =
            first_non_blank_of(has_count ? std::min(n - 1, m_buffer.last_row())
                                         : m_buffer.last_row());
        t.type = MotionType::Linewise;
        return t;
    case '0':
    case key::Home:
        t.pos.col = 0;
        return t;
    case '^':
        t.pos = first_non_blank_of(cur.row);
        return t;
    case '$':
    case key::End: {
        t.pos.row = std::min(cur.row + n - 1, m_buffer.last_row());
        const std::string& target_line = m_buffer.line(t.pos.row);
        t.pos.col = target_line.empty()
                        ? 0
                        : utf8::prev(target_line, target_line.size());
        t.type = MotionType::Inclusive;
        t.to_eol = true;
        return t;
    }
    case 'f':
    case 'F':
    case 't':
    case 'T':
    case ';':
    case ',': {
        MotionKeys find = spec;
        const bool repeat = spec.key == ';' || spec.key == ',';
        if (repeat) {
            if (m_last_find.key == 0) {
                return std::nullopt;
            }
            find = m_last_find;
            if (spec.key == ',') {
                // Same search, opposite direction.
                find.key = std::islower(find.key) ? std::toupper(find.key)
                                                  : std::tolower(find.key);
            }
        } else {
            m_last_find = spec;
        }
        const bool forward = find.key == 'f' || find.key == 't';
        const bool till = find.key == 't' || find.key == 'T';
        const auto col = motion::find_in_line(
            line, cur.col, utf8::encode(static_cast<char32_t>(find.arg)),
            forward, till, count, repeat);
        if (!col) {
            return std::nullopt;
        }
        t.pos.col = *col;
        t.type = forward ? MotionType::Inclusive : MotionType::Exclusive;
        return t;
    }
    case 'n':
    case 'N': {
        if (m_last_search.empty()) {
            show_error("E35: No previous regular expression");
            return std::nullopt;
        }
        const bool forward = (spec.key == 'n') == m_last_search_forward;
        bool wrapped = false;
        Position pos = cur;
        for (int k = 0; k < count; ++k) {
            const auto match = find_match(m_last_search, forward, pos, wrapped);
            if (!match) {
                return std::nullopt;
            }
            pos = *match;
        }
        if (wrapped) {
            show_message(forward ? "search hit BOTTOM, continuing at TOP"
                                 : "search hit TOP, continuing at BOTTOM");
        }
        t.pos = pos;
        return t;
    }
    default:
        return std::nullopt;
    }
}

void Editor::move_to(const MotionTarget& target) {
    m_cursor = target.pos;
    clamp_cursor();
    if (target.to_eol) {
        m_want_col = want_eol; // j/k keep sticking to the end of the line
    } else if (!target.vertical) {
        update_want_col();
    }
}

Editor::MotionTarget Editor::change_word_target(const int count,
                                                const bool bigword) const {
    const auto& line = m_buffer.line(m_cursor.row);
    if (m_cursor.col >= line.size() ||
        motion::char_class(utf8::decode(line, m_cursor.col), bigword) ==
            motion::CharClass::Blank) {
        // On whitespace "cw" behaves like "dw" (changes just the blanks).
        MotionTarget t{m_cursor, MotionType::Exclusive};
        for (int k = 0; k < count; ++k) {
            t.pos = motion::word_forward(m_buffer, t.pos, bigword);
        }
        if (t.pos.row > m_cursor.row) {
            t.pos = {m_cursor.row, line.size()};
        }
        return t;
    }
    Position pos = m_cursor;
    for (int k = 0; k < count; ++k) {
        // If already on the last character of a word, the first step stays.
        const std::size_t next = utf8::next(line, pos.col);
        const bool at_word_end =
            next >= line.size() ||
            motion::char_class(utf8::decode(line, next), bigword) !=
                motion::char_class(utf8::decode(line, pos.col), bigword);
        if (k == 0 && at_word_end) {
            continue;
        }
        pos = motion::word_end(m_buffer, pos, bigword);
    }
    return {pos, MotionType::Inclusive};
}

// ---- Operators
// ---------------------------------------------------------------

void Editor::apply_operator(const int op, const Position from,
                            MotionTarget target, const char reg) {
    if (target.type == MotionType::Linewise) {
        const std::size_t first = std::min(from.row, target.pos.row);
        const std::size_t last = std::max(from.row, target.pos.row);
        const Register text{
            m_buffer.get_text({first, 0}, {last, m_buffer.line_length(last)}),
            true};
        switch (op) {
        case 'y':
            m_registers.yank(reg, text);
            m_cursor.row = first;
            break;
        case 'd':
            m_registers.del(reg, text);
            m_buffer.delete_lines(first, last);
            m_cursor.row = std::min(first, m_buffer.last_row());
            m_cursor.col = m_buffer.first_non_blank(m_cursor.row);
            break;
        case 'c': {
            m_registers.del(reg, text);
            const std::string indent =
                m_options.autoindent ? indent_of(first) : "";
            m_buffer.replace({first, 0}, {last, m_buffer.line_length(last)},
                             indent);
            enter_insert({first, indent.size()});
            break;
        }
        default:
            break;
        }
        clamp_cursor();
        update_want_col();
        return;
    }

    Position start = std::min(from, target.pos);
    Position end = std::max(from, target.pos);
    if (target.type == MotionType::Inclusive) {
        end.col = utf8::next(m_buffer.line(end.row), end.col);
    } else if (end.col == 0 && end.row > start.row) {
        // An exclusive motion ending at column 0 does not include the line
        // break before it (e.g. "d}" or "dw" at the end of a paragraph).
        end = {end.row - 1, m_buffer.line_length(end.row - 1)};
    }
    const Register text{m_buffer.get_text(start, end), false};

    switch (op) {
    case 'y':
        m_registers.yank(reg, text);
        m_cursor = start;
        break;
    case 'd':
        m_registers.del(reg, text);
        m_buffer.erase(start, end);
        m_cursor = start;
        break;
    case 'c':
        m_registers.del(reg, text);
        m_buffer.erase(start, end);
        enter_insert(start);
        break;
    default:
        break;
    }
    clamp_cursor();
    update_want_col();
}

// ---- Commands
// ----------------------------------------------------------------

void Editor::enter_insert(const Position pos) {
    m_cursor = pos;
    m_mode = Mode::Insert;
    update_want_col();
}

void Editor::open_line(const bool below) {
    const std::string indent =
        m_options.autoindent ? indent_of(m_cursor.row) : "";
    if (below) {
        m_buffer.insert({m_cursor.row, m_buffer.line_length(m_cursor.row)},
                        "\n" + indent);
        enter_insert({m_cursor.row + 1, indent.size()});
    } else {
        m_buffer.insert({m_cursor.row, 0}, indent + "\n");
        enter_insert({m_cursor.row, indent.size()});
    }
}

void Editor::paste(const char reg, const bool before, const int count) {
    const auto contents = m_registers.get(reg);
    if (!contents) {
        show_error(std::string("E353: Nothing in register ") + reg);
        return;
    }
    std::string text;
    for (int k = 0; k < count; ++k) {
        if (contents->linewise && k > 0) {
            text += '\n';
        }
        text += contents->text;
    }

    if (contents->linewise) {
        const std::size_t row = before ? m_cursor.row : m_cursor.row + 1;
        m_buffer.insert_lines(row, text);
        m_cursor = {row, m_buffer.first_non_blank(row)};
    } else {
        const auto& line = m_buffer.line(m_cursor.row);
        const Position at{m_cursor.row, before || line.empty()
                                            ? m_cursor.col
                                            : utf8::next(line, m_cursor.col)};
        const Position end = m_buffer.insert(at, text);
        // Cursor on the last pasted character (or its start if multi-line).
        m_cursor =
            text.find('\n') == std::string::npos
                ? Position{end.row, utf8::prev(m_buffer.line(end.row), end.col)}
                : at;
    }
    clamp_cursor();
    update_want_col();
}

void Editor::join_lines(const int count) {
    const int joins = std::max(count, 2) - 1;
    for (int k = 0; k < joins && m_cursor.row < m_buffer.last_row(); ++k) {
        const std::size_t row = m_cursor.row;
        const std::size_t len = m_buffer.line_length(row);
        m_buffer.erase({row, len},
                       {row + 1, m_buffer.first_non_blank(row + 1)});
        // Separate with one space unless the line ends in whitespace or the
        // joined text is empty or starts with ')'.
        const std::string& line = m_buffer.line(row);
        const bool needs_space = len > 0 && line[len - 1] != ' ' &&
                                 line[len - 1] != '\t' && len < line.size() &&
                                 line[len] != ')';
        if (needs_space) {
            m_buffer.insert({row, len}, " ");
        }
        m_cursor.col = len;
    }
    update_want_col();
}

bool Editor::replace_chars(const int count, const int ch) {
    if (!is_text_key(ch)) {
        return false; // e.g. r<Esc> cancels
    }
    const auto& line = m_buffer.line(m_cursor.row);
    std::size_t end = m_cursor.col;
    for (int k = 0; k < count; ++k) {
        if (end >= line.size()) {
            return false; // not enough characters
        }
        end = utf8::next(line, end);
    }
    std::string text;
    for (int k = 0; k < count; ++k) {
        text += utf8::encode(static_cast<char32_t>(ch));
    }
    m_buffer.replace(m_cursor, {m_cursor.row, end}, text);
    // Cursor on the last replaced character.
    m_cursor.col =
        utf8::prev(m_buffer.line(m_cursor.row), m_cursor.col + text.size());
    update_want_col();
    return true;
}

void Editor::undo() {
    if (const auto pos = m_buffer.undo()) {
        m_cursor = *pos;
    } else {
        show_message("Already at oldest change");
    }
    clamp_cursor();
    update_want_col();
}

void Editor::redo() {
    if (const auto pos = m_buffer.redo()) {
        m_cursor = *pos;
    } else {
        show_message("Already at newest change");
    }
    clamp_cursor();
    update_want_col();
}

void Editor::scroll_half_page(const bool down, const int count) {
    const std::size_t amount = count > 0
                                   ? static_cast<std::size_t>(count)
                                   : std::max<std::size_t>(m_view_rows / 2, 1);
    const std::size_t last = m_buffer.last_row();
    if (down) {
        m_view_top = std::min(m_view_top + amount, last);
        m_cursor.row = std::min(m_cursor.row + amount, last);
    } else {
        m_view_top -= std::min(m_view_top, amount);
        m_cursor.row -= std::min(m_cursor.row, amount);
    }
    m_cursor.col = col_for_want(m_cursor.row);
}

// ---- Command line and search
// -------------------------------------------------

void Editor::begin_command_line(const char type) {
    m_mode = Mode::CommandLine;
    m_cmdline_type = type;
    m_cmdline.clear();
}

void Editor::execute_command_line() {
    if (m_cmdline_type == ':') {
        m_buffer.begin_undo_group(m_cursor);
        ex::execute(*this, m_cmdline);
        m_buffer.end_undo_group();
        return;
    }
    // '/' or '?': an empty pattern reuses the last one.
    if (!m_cmdline.empty()) {
        m_last_search = m_cmdline;
    }
    m_last_search_forward = m_cmdline_type == '/';
    const auto target = eval_motion({'n', 0}, 1, false, false);
    if (target) {
        move_to(*target);
    }
}

std::optional<Position> Editor::find_match(const std::string& pattern,
                                           const bool forward,
                                           const Position from, bool& wrapped) {
    std::regex re;
    try {
        re = std::regex(pattern);
    } catch (const std::regex_error&) {
        show_error("E383: Invalid search string: " + pattern);
        return std::nullopt;
    }

    // All match start columns in a line (non-overlapping, left to right).
    auto matches_in = [&](const std::string& line) {
        std::vector<std::size_t> cols;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), re);
             it != std::sregex_iterator(); ++it) {
            cols.push_back(static_cast<std::size_t>(it->position()));
        }
        return cols;
    };

    const std::size_t lines = m_buffer.line_count();
    // Visit every line once starting at the cursor line, then the cursor line
    // again (the part on the other side of the cursor).
    for (std::size_t step = 0; step <= lines; ++step) {
        const std::size_t row = forward
                                    ? (from.row + step) % lines
                                    : (from.row + lines - step % lines) % lines;
        const auto cols = matches_in(m_buffer.line(row));
        std::optional<std::size_t> hit;
        if (forward) {
            for (const std::size_t c : cols) {
                if (step > 0 || c > from.col) {
                    hit = c;
                    break;
                }
            }
        } else {
            for (auto it = cols.rbegin(); it != cols.rend(); ++it) {
                if (step > 0 || *it < from.col) {
                    hit = *it;
                    break;
                }
            }
        }
        if (hit) {
            wrapped = wrapped ||
                      (forward ? from.row + step >= lines : step > from.row);
            return Position{row, *hit};
        }
    }
    show_error("E486: Pattern not found: " + pattern);
    return std::nullopt;
}

// ---- Cursor helpers
// ----------------------------------------------------------

void Editor::clamp_cursor() {
    m_cursor.row = std::min(m_cursor.row, m_buffer.last_row());
    const std::string& line = m_buffer.line(m_cursor.row);
    // In insert mode the cursor may sit after the last character.
    if (m_mode == Mode::Insert || m_mode == Mode::CommandLine) {
        m_cursor.col = std::min(m_cursor.col, line.size());
    } else if (line.empty()) {
        m_cursor.col = 0;
    } else if (m_cursor.col >= line.size()) {
        m_cursor.col = utf8::prev(line, line.size());
    }
    if (m_cursor.col > 0 && m_cursor.col < line.size()) {
        // Never point into the middle of a UTF-8 sequence.
        m_cursor.col = utf8::prev(line, utf8::next(line, m_cursor.col));
    }
}

void Editor::update_want_col() {
    m_want_col = utf8::display_col(m_buffer.line(m_cursor.row), m_cursor.col,
                                   m_options.tabstop);
}

std::size_t Editor::col_for_want(const std::size_t row) const {
    return utf8::byte_col_at(m_buffer.line(row), m_want_col, m_options.tabstop);
}

std::string Editor::indent_of(const std::size_t row) const {
    return m_buffer.line(row).substr(0, m_buffer.first_non_blank(row));
}

void Editor::scroll_to_cursor() {
    // Keep `scrolloff` lines of context above and below the cursor.
    const std::size_t so = std::min(m_options.scrolloff, (m_view_rows - 1) / 2);
    if (m_cursor.row < m_view_top + so) {
        m_view_top = m_cursor.row - std::min(m_cursor.row, so);
    } else if (m_cursor.row + so >= m_view_top + m_view_rows) {
        m_view_top = m_cursor.row + so + 1 - m_view_rows;
    }
    m_view_top = std::min(m_view_top, m_buffer.last_row());

    const std::size_t col = utf8::display_col(m_buffer.line(m_cursor.row),
                                              m_cursor.col, m_options.tabstop);
    if (col < m_view_left) {
        m_view_left = col;
    } else if (col >= m_view_left + m_view_cols) {
        m_view_left = col + 1 - m_view_cols;
    }
}
