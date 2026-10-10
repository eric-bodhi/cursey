#pragma once

#include <string_view>

class Editor;

// Ex (":") commands. To add one, write a handler in ex_commands.cpp and add a
// row to the command table there.
namespace ex {

// Run one command line, without the leading ':' (e.g. "wq", "set ts=8", "42").
void execute(Editor& editor, std::string_view line);

} // namespace ex
