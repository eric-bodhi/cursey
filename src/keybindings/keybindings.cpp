#include "keybindings.h"
#include "../core/editor.h"
#include <functional>

namespace Keybindings {

std::unordered_map<std::string, std::function<void(Editor&)>> normal_keys = {
    {
        "h",
        [](Editor& editor) { editor.get_cm().move_dir(Direction::Left); },
    },
    {"j", [](Editor& editor) { editor.get_cm().move_dir(Direction::Down); }},
    {"k", [](Editor& editor) { editor.get_cm().move_dir(Direction::Up); }},
    {"l",
     [](Editor& editor) {
         auto& cm = editor.get_cm();
         if (const auto cursor = cm.get(); cursor.col < editor.get_buffer().get_line_length(cursor.row) - 1) {
             cm.move_dir(Direction::Right);
         }
     }},
    {"i", [](Editor& editor) { editor.set_mode(Mode::Insert); }},
    {"v", [](Editor& editor) { editor.set_mode(Mode::Visual); }},
    {"a",
     [](Editor& editor) {
         auto& cm = editor.get_cm();
         cm.move_dir(Direction::Right);
         editor.set_mode(Mode::Insert);
     }},
    {":", [](Editor& editor) { editor.set_mode(Mode::Command); }},

    {"dd",
     [](Editor& editor) {
         const auto& buffer = editor.get_buffer();
         auto& cm = editor.get_cm();
         editor.get_buffer().delete_line(editor.get_cm());
         if (cm.row() == buffer.line_count()) {
             cm.move_dir(Direction::Up);
         }
     }},

    {"G",
     [](Editor& editor) {
         const auto& tb = editor.get_buffer();
         editor.get_cm().move_abs({tb.line_count() - 1, 0});
     }},
    {"gg", [](Editor& editor) { editor.get_cm().move_abs({0, 0}); }},
};

std::unordered_map<std::string, std::function<void(Editor&)>> visual_keys = {
    {"h",
     [](Editor& editor) {
         editor.get_cm().move_dir(Direction::Left);
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"j",
     [](Editor& editor) {
         editor.get_cm().move_dir(Direction::Down);
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"k",
     [](Editor& editor) {
         editor.get_cm().move_dir(Direction::Up);
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"l",
     [](Editor& editor) {
         editor.get_cm().move_dir(Direction::Right);
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"G",
     [](Editor& editor) {
         const auto& tb = editor.get_buffer();
         editor.get_cm().move_abs({tb.line_count() - 1, 0});
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"gg",
     [](Editor& editor) {
         editor.get_cm().move_abs({0, 0});
         editor.set_visual_end(editor.get_cm().get());
     }},
    {"v", [](Editor& editor) { editor.set_mode(Mode::Normal); }},
    {"d",
     [](Editor& editor) {
         auto [visual_start, visual_end] = editor.get_visual_range();
         const Cursor start_cursor = visual_start.value();
         const Cursor end_cursor = visual_end.value();
         editor.get_buffer().delete_range(start_cursor, end_cursor,
                                          editor.get_logger());
         editor.get_cm().move_abs(start_cursor);
         editor.set_mode(Mode::Normal);
     }},
    {"\x1b",
     [](Editor& editor) { editor.set_mode(Mode::Normal); }}, // Escape key
};
} // namespace Keybindings
