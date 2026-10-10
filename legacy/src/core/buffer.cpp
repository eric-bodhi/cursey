#include "buffer.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

// Position just after `text` if it were inserted at `pos`.
Position end_of_inserted(const Position pos, const std::string_view text) {
    const std::size_t newlines = std::ranges::count(text, '\n');
    if (newlines == 0) {
        return {pos.row, pos.col + text.size()};
    }
    return {pos.row + newlines, text.size() - text.rfind('\n') - 1};
}

} // namespace

Buffer::Buffer() : m_lines(1) {
}

void Buffer::set_text(const std::string_view text) {
    m_lines.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            m_lines.emplace_back(text.substr(start));
            break;
        }
        m_lines.emplace_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    m_undo_stack.clear();
    m_redo_stack.clear();
    m_open_group.reset();
    m_group_depth = 0;
    m_saved_state = 0;
    ++m_revision;
}

std::string Buffer::text() const {
    std::string out;
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        out += m_lines[i];
    }
    return out;
}

bool Buffer::load(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        error = "\"" + path + "\" " + std::strerror(errno);
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    // A trailing newline terminates the last line; it does not start a new
    // (empty) one.
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    m_crlf = content.find("\r\n") != std::string::npos;
    if (m_crlf) {
        std::erase(content, '\r');
    }
    set_text(content);
    return true;
}

bool Buffer::save(const std::string& path, std::string& error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        error = "\"" + path +
                "\" E212: Can't open file for writing: " + std::strerror(errno);
        return false;
    }
    const char* eol = m_crlf ? "\r\n" : "\n";
    // An empty buffer is written as an empty file.
    if (!(m_lines.size() == 1 && m_lines[0].empty())) {
        for (const auto& line : m_lines) {
            file << line << eol;
        }
    }
    file.close();
    if (!file) {
        error = "\"" + path + "\" E514: Write error";
        return false;
    }
    mark_saved();
    return true;
}

std::size_t Buffer::line_count() const {
    return m_lines.size();
}

const std::string& Buffer::line(const std::size_t row) const {
    return m_lines.at(row);
}

std::size_t Buffer::line_length(const std::size_t row) const {
    return m_lines.at(row).size();
}

std::size_t Buffer::last_row() const {
    return m_lines.size() - 1;
}

Position Buffer::end_position() const {
    return {last_row(), m_lines.back().size()};
}

std::size_t Buffer::first_non_blank(const std::size_t row) const {
    const auto& l = m_lines.at(row);
    const std::size_t i = l.find_first_not_of(" \t");
    return i == std::string::npos ? l.size() : i;
}

Position Buffer::clamp(Position pos) const {
    if (pos.row >= m_lines.size()) {
        return end_position();
    }
    pos.col = std::min(pos.col, m_lines[pos.row].size());
    return pos;
}

std::string Buffer::get_text(Position start, Position end) const {
    start = clamp(start);
    end = clamp(end);
    if (end <= start) {
        return {};
    }
    if (start.row == end.row) {
        return m_lines[start.row].substr(start.col, end.col - start.col);
    }
    std::string out = m_lines[start.row].substr(start.col);
    for (std::size_t r = start.row + 1; r < end.row; ++r) {
        out += '\n';
        out += m_lines[r];
    }
    out += '\n';
    out += m_lines[end.row].substr(0, end.col);
    return out;
}

Position Buffer::raw_insert(Position pos, const std::string_view text) {
    pos = clamp(pos);
    ++m_revision;
    std::string& line = m_lines[pos.row];
    const std::size_t nl = text.find('\n');
    if (nl == std::string_view::npos) {
        line.insert(pos.col, text);
        return {pos.row, pos.col + text.size()};
    }

    // Split the line: [head][first piece] ... [last piece][tail]
    std::string tail = line.substr(pos.col);
    line.erase(pos.col);
    line.append(text.substr(0, nl));

    std::vector<std::string> new_lines;
    std::size_t start = nl + 1;
    while (true) {
        const std::size_t next_nl = text.find('\n', start);
        if (next_nl == std::string_view::npos) {
            new_lines.emplace_back(text.substr(start));
            break;
        }
        new_lines.emplace_back(text.substr(start, next_nl - start));
        start = next_nl + 1;
    }
    const Position end{pos.row + new_lines.size(), new_lines.back().size()};
    new_lines.back() += tail;
    m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(pos.row + 1),
                   new_lines.begin(), new_lines.end());
    return end;
}

std::string Buffer::raw_erase(Position start, Position end) {
    start = clamp(start);
    end = clamp(end);
    if (end <= start) {
        return {};
    }
    ++m_revision;
    std::string removed = get_text(start, end);
    if (start.row == end.row) {
        m_lines[start.row].erase(start.col, end.col - start.col);
        return removed;
    }
    m_lines[start.row].erase(start.col);
    m_lines[start.row] += m_lines[end.row].substr(end.col);
    m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(start.row + 1),
                  m_lines.begin() + static_cast<std::ptrdiff_t>(end.row + 1));
    return removed;
}

Position Buffer::insert(Position pos, const std::string_view text) {
    pos = clamp(pos);
    if (text.empty()) {
        return pos;
    }
    record({Edit::Kind::Insert, pos, std::string(text)});
    return raw_insert(pos, text);
}

std::string Buffer::erase(Position start, Position end) {
    start = clamp(start);
    std::string removed = raw_erase(start, end);
    if (!removed.empty()) {
        record({Edit::Kind::Erase, start, removed});
    }
    return removed;
}

void Buffer::replace(const Position start, const Position end,
                     const std::string_view text) {
    erase(start, end);
    insert(start, text);
}

void Buffer::replace_line(const std::size_t row, const std::string_view text) {
    if (m_lines.at(row) != text) {
        replace({row, 0}, {row, m_lines[row].size()}, text);
    }
}

std::string Buffer::delete_lines(std::size_t first, std::size_t last) {
    last = std::min(last, last_row());
    if (first > last) {
        return {};
    }
    std::string removed = get_text({first, 0}, {last, m_lines[last].size()});
    if (last < last_row()) {
        // Remove the lines together with their trailing line breaks.
        erase({first, 0}, {last + 1, 0});
    } else if (first > 0) {
        // Deleting through the last line: eat the line break before `first`.
        erase({first - 1, m_lines[first - 1].size()},
              {last, m_lines[last].size()});
    } else {
        // Deleting everything: leave a single empty line.
        erase({0, 0}, end_position());
    }
    return removed;
}

void Buffer::insert_lines(const std::size_t row, const std::string_view text) {
    if (row >= m_lines.size()) {
        insert(end_position(), "\n" + std::string(text));
    } else {
        insert({row, 0}, std::string(text) + "\n");
    }
}

// ---- Undo -------------------------------------------------------------------

void Buffer::record(Edit edit) {
    m_redo_stack.clear();
    if (m_open_group) {
        m_open_group->edits.push_back(std::move(edit));
        return;
    }
    // An edit outside of any group becomes its own undo step.
    UndoGroup group;
    group.cursor_before = edit.pos;
    group.id = m_next_group_id++;
    group.edits.push_back(std::move(edit));
    m_undo_stack.push_back(std::move(group));
}

void Buffer::begin_undo_group(const Position cursor) {
    if (m_group_depth++ == 0) {
        m_open_group = UndoGroup{};
        m_open_group->cursor_before = cursor;
    }
}

void Buffer::end_undo_group() {
    if (m_group_depth == 0) {
        return;
    }
    if (--m_group_depth == 0) {
        if (m_open_group && !m_open_group->edits.empty()) {
            m_open_group->id = m_next_group_id++;
            m_undo_stack.push_back(std::move(*m_open_group));
        }
        m_open_group.reset();
    }
}

std::optional<Position> Buffer::undo() {
    if (m_undo_stack.empty()) {
        return std::nullopt;
    }
    UndoGroup group = std::move(m_undo_stack.back());
    m_undo_stack.pop_back();
    for (auto it = group.edits.rbegin(); it != group.edits.rend(); ++it) {
        if (it->kind == Edit::Kind::Insert) {
            raw_erase(it->pos, end_of_inserted(it->pos, it->text));
        } else {
            raw_insert(it->pos, it->text);
        }
    }
    const Position cursor = group.cursor_before;
    m_redo_stack.push_back(std::move(group));
    return clamp(cursor);
}

std::optional<Position> Buffer::redo() {
    if (m_redo_stack.empty()) {
        return std::nullopt;
    }
    UndoGroup group = std::move(m_redo_stack.back());
    m_redo_stack.pop_back();
    for (const auto& edit : group.edits) {
        if (edit.kind == Edit::Kind::Insert) {
            raw_insert(edit.pos, edit.text);
        } else {
            raw_erase(edit.pos, end_of_inserted(edit.pos, edit.text));
        }
    }
    const Position cursor = group.edits.front().pos;
    m_undo_stack.push_back(std::move(group));
    return clamp(cursor);
}

bool Buffer::can_undo() const {
    return !m_undo_stack.empty();
}

bool Buffer::can_redo() const {
    return !m_redo_stack.empty();
}

std::uint64_t Buffer::state_id() const {
    return m_undo_stack.empty() ? 0 : m_undo_stack.back().id;
}

bool Buffer::is_modified() const {
    const bool pending = m_open_group && !m_open_group->edits.empty();
    return pending || state_id() != m_saved_state;
}

void Buffer::mark_saved() {
    m_saved_state = state_id();
}

std::uint64_t Buffer::revision() const {
    return m_revision;
}
