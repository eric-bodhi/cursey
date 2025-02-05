#include "keybindings.h"
#include "editor.h"
#include <functional>

namespace Keybindings {

std::unordered_map<std::string_view, std::function<void(Editor&)>> normalkeys =
    {
        {
            "h",
            [](Editor& editor) {
                editor.getCm().moveDir(Direction::Left);
                auto cursor = editor.getCm().get();
            },
        },
        {"j", [](Editor& editor) { editor.getCm().moveDir(Direction::Down); }},
        {"k", [](Editor& editor) { editor.getCm().moveDir(Direction::Up); }},
        {"l",
         [](Editor& editor) {
             auto& cm = editor.getCm();
             auto cursor = cm.get();
             if (cursor.col <
                 editor.getBuffer().getLineLength(cursor.row) - 1) {
                 cm.moveDir(Direction::Right);
             }
         }},
        {"i", [](Editor& editor) { editor.setMode(Mode::Insert); }},
        {"a",
         [](Editor& editor) {
             auto& cm = editor.getCm();
             auto cm_cursor = cm.get();
            cm.moveDir(Direction::Right);
             editor.setMode(Mode::Insert);
         }},
        {":", [](Editor& editor) { editor.setMode(Mode::Command); }},

        {"dd",
         [](Editor& editor) { editor.getBuffer().deleteLine(editor.getCm()); }},
};

} // namespace Keybindings
