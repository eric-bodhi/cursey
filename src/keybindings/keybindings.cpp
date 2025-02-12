#include "keybindings.h"
#include "../core/editor.h"
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
                 editor.getBuffer().get_line_length(cursor.row) - 1) {
                 cm.moveDir(Direction::Right);
             }
         }},
        {"i", [](Editor& editor) { editor.setMode(Mode::Insert); }},
        {"v", [](Editor& editor) { editor.setMode(Mode::Visual); }},
        {"a",
         [](Editor& editor) {
             auto& cm = editor.getCm();
             auto cm_cursor = cm.get();
             cm.moveDir(Direction::Right);
             editor.setMode(Mode::Insert);
         }},
        {":", [](Editor& editor) { editor.setMode(Mode::Command); }},

        {"dd",
         [](Editor& editor) {
             auto& buffer = editor.getBuffer();
             auto& cm = editor.getCm();
             editor.getBuffer().delete_line(editor.getCm());
             if (cm.get().row == buffer.line_count()) {
                 cm.moveDir(Direction::Up);
             }
         }},

        {"G",
         [](Editor& editor) {
             auto& tb = editor.getBuffer();
             // length is 1 idx, position is 0 idx
             editor.getCm().moveAbs({tb.line_count() - 1, 0});
         }},
        {"gg", [](Editor& editor) { editor.getCm().moveAbs({0, 0}); }},
};

std::unordered_map<std::string_view, std::function<void(Editor&)>> visualkeys =
    {
        {"h",
         [](Editor& editor) {
             editor.getCm().moveDir(Direction::Left);
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"j",
         [](Editor& editor) {
             editor.getCm().moveDir(Direction::Down);
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"k",
         [](Editor& editor) {
             editor.getCm().moveDir(Direction::Up);
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"l",
         [](Editor& editor) {
             editor.getCm().moveDir(Direction::Right);
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"G",
         [](Editor& editor) {
             auto& tb = editor.getBuffer();
             editor.getCm().moveAbs({tb.line_count() - 1, 0});
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"gg",
         [](Editor& editor) {
             editor.getCm().moveAbs({0, 0});
             editor.setVisualEnd(editor.getCm().get());
         }},
        {"v", [](Editor& editor) { editor.setMode(Mode::Normal); }},
        {"d",
         [](Editor& editor) {
             VisualRange vr = editor.getVisualRange();
             Cursor start_cursor = vr.visual_start.value();
             Cursor end_cursor = vr.visual_end.value();
             editor.getBuffer().delete_range(start_cursor, end_cursor,
                                             editor.getLogger());
             editor.getCm().moveAbs(start_cursor);
             editor.setMode(Mode::Normal);
         }},
        {"\x1b",
         [](Editor& editor) { editor.setMode(Mode::Normal); }}, // Escape key
};
} // namespace Keybindings
