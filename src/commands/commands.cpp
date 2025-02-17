#include "commands.h"
#include "../core/editor.h"
#include <functional>

namespace Command {

std::unordered_map<std::string, std::function<void(Editor&)>> command_table = {
    {
        "w",
        [](Editor& editor) { editor.write_file(); },
    },
    {"q", [](Editor& editor) { editor.set_should_exit(true); }},
    {"q!",
     [](Editor& editor) {
         editor.set_should_exit(true);
         editor.get_buffer().set_modified(false);
     }},
    {"wq",
     [](Editor& editor) {
         command_table.at("w")(editor);
         command_table.at("q")(editor);
     }},
};

} // namespace Command
