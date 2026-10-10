#include "ex_commands.h"
#include "../core/editor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace ex {

namespace {

struct ExArgs {
    bool bang = false; // "q!" / "e!"
    std::string arg;   // everything after the command name, trimmed
};

struct ExCommand {
    std::string_view name; // full name, e.g. "write"
    std::size_t min_len;   // shortest accepted abbreviation, e.g. 1 for "w"
    void (*run)(Editor&, const ExArgs&);
};

void cmd_write(Editor& editor, const ExArgs& args) {
    editor.write(args.arg);
}

void cmd_quit(Editor& editor, const ExArgs& args) {
    editor.quit(args.bang);
}

void cmd_write_quit(Editor& editor, const ExArgs& args) {
    if (editor.write(args.arg)) {
        editor.quit(args.bang);
    }
}

// :x - like :wq, but only writes when there are changes.
void cmd_exit(Editor& editor, const ExArgs& args) {
    if (editor.buffer().is_modified() && !editor.write(args.arg)) {
        return;
    }
    editor.quit(args.bang);
}

void cmd_edit(Editor& editor, const ExArgs& args) {
    editor.edit(args.arg, args.bang);
}

void cmd_set(Editor& editor, const ExArgs& args) {
    if (args.arg.empty()) {
        editor.show_error("E471: Argument required");
        return;
    }
    std::string shown;
    std::size_t start = 0;
    // Several options may be given at once: ":set ts=8 noet".
    while (start < args.arg.size()) {
        std::size_t end = args.arg.find(' ', start);
        if (end == std::string::npos) {
            end = args.arg.size();
        }
        if (end > start) {
            const auto error = editor.options().set(
                std::string_view(args.arg).substr(start, end - start), shown);
            if (error) {
                editor.show_error(*error);
                return;
            }
        }
        start = end + 1;
    }
    if (!shown.empty()) {
        editor.show_message(shown);
    }
}

constexpr std::array<ExCommand, 6> commands = {{
    {"write", 1, cmd_write},
    {"quit", 1, cmd_quit},
    {"wq", 2, cmd_write_quit},
    {"xit", 1, cmd_exit},
    {"edit", 1, cmd_edit},
    {"set", 2, cmd_set},
}};

const ExCommand* find_command(const std::string_view name) {
    for (const auto& cmd : commands) {
        if (name.size() >= cmd.min_len && cmd.name.starts_with(name)) {
            return &cmd;
        }
    }
    return nullptr;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

void execute(Editor& editor, std::string_view line) {
    line = trim(line);
    while (line.starts_with(':')) {
        line = trim(line.substr(1));
    }
    if (line.empty()) {
        return;
    }

    // ":42" jumps to line 42, ":$" to the last line.
    if (line == "$") {
        editor.goto_line(editor.buffer().last_row());
        return;
    }
    if (std::isdigit(static_cast<unsigned char>(line.front()))) {
        std::size_t n = 0;
        for (const char c : line) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                editor.show_error("E492: Not an editor command: " +
                                  std::string(line));
                return;
            }
            n = std::min<std::size_t>(n * 10 + (c - '0'), 1'000'000'000);
        }
        editor.goto_line(n == 0 ? 0 : n - 1);
        return;
    }

    std::size_t name_end = 0;
    while (name_end < line.size() &&
           std::isalpha(static_cast<unsigned char>(line[name_end]))) {
        ++name_end;
    }
    const std::string_view name = line.substr(0, name_end);
    ExArgs args;
    std::string_view rest = line.substr(name_end);
    if (rest.starts_with('!')) {
        args.bang = true;
        rest.remove_prefix(1);
    }
    args.arg = std::string(trim(rest));

    const ExCommand* cmd = find_command(name);
    if (!cmd) {
        editor.show_error("E492: Not an editor command: " + std::string(line));
        return;
    }
    cmd->run(editor, args);
}

} // namespace ex
