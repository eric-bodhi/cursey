#include "keybindings.h"
#include "editor.h"
#include <functional>

namespace Keybindings {

std::unordered_map<std::string_view, std::function<void(Editor&)>> normalkeys =
    {
        {
            "h",
            [](Editor& editor) { editor.getCm().moveDir(Direction::Left); },
        },
        {"j", [](Editor& editor) { editor.getCm().moveDir(Direction::Down); }},
        {"k", [](Editor& editor) { editor.getCm().moveDir(Direction::Up); }},
        {"l", [](Editor& editor) { editor.getCm().moveDir(Direction::Right); }},
        {"i", [](Editor& editor) { editor.setMode(Mode::Insert); }},
        {":", [](Editor& editor) { editor.setMode(Mode::Command); }},

        {"dd", [](Editor& editor) { editor.getBuffer().deleteLine(editor.getCm()); }},
};

} // namespace Keybindings
