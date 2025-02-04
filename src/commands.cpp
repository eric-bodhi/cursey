#include "commands.h"
#include "editor.h"
#include <functional>

namespace Command {

void deleteLine(Editor& editor) {
    auto& tb = editor.getBuffer();
    auto& cm = editor.getCm();
    tb.deleteLine(cm);
    if (cm.get().row == tb.lineCount()) {
        cm.moveDir(Direction::Up);
    }
    editor.updateView();
}

std::unordered_map<std::string_view, std::function<void(Editor&)>> ftable = {
    {
        "dd",
        deleteLine,
    },
};

} // namespace Command
