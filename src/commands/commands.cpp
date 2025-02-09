#include "commands.h"
#include "../core/editor.h"
#include <functional>

namespace Command {

std::unordered_map<std::string_view, std::function<void(Editor&)>> ftable = {
    {
        "w",
        [](Editor& editor) {
            editor.writeFile();
        },
    },
    {
        "q",
        [](Editor& editor) {
            editor.setShouldExit(true);
        }
    },
    {
        "q!",
        [](Editor& editor) {
            editor.setShouldExit(true);
            editor.getBuffer().setModified(false);
        }
    },
    {
        "wq",
        [](Editor& editor) {
            ftable.at("w")(editor);
            ftable.at("q")(editor);
        }
    },
};

} // namespace Command
