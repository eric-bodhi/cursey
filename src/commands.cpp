#include "commands.h"
#include "editor.h"
#include <functional>

namespace Command {

std::unordered_map<std::string_view, std::function<void(Editor&)>> ftable = {
    {
        "w",
        [](Editor& editor) {
            editor.writeFile();
        },
    },
};

} // namespace Command
