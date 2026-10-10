#include "src/core/editor.h"
#include "src/ui/tui.h"

#include <exception>
#include <iostream>

int main(const int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [filename]\n";
        return 1;
    }

    Editor editor;
    if (argc == 2) {
        editor.open(argv[1]);
    }

    try {
        Tui tui;
        while (!editor.should_quit() && !tui.closed()) {
            tui.render(editor);
            if (const auto key = tui.read_key()) {
                editor.handle_key(*key);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "cursey: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
