#include "src/core/editor.h"
#include "src/core/tui.h"
#include "src/utils/log.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    /*
    TermManager tm; // Automatically manages terminal state
    Cursey cursey(argv[1]);

    char c;
    while (true) {
        cursey.refresh_screen();   // Redraw the screen each time we go through
                                   // the loop
        read(STDIN_FILENO, &c, 1); // Read a single character from the terminal

        // Handle keypress events
        if (c == 'q') {
            break; // Quit the program on 'q'
        } else if (c == 'h') {
            cursey.move(Direction::Left);
        } else if (c == 'j') {
            cursey.move(Direction::Down);
        } else if (c == 'k') {
            cursey.move(Direction::Up);
        } else if (c == 'l') {
            cursey.move(Direction::Right);
        }
    }
    */

    /*
    TextBuffer tb(argv[1]);
    tb.insertAt({0, 0}, 'X');
    tb.insertAt({0, 0}, 'Y');
    for (std::size_t i = 0; i < tb.lineCount(); i++) {
        std::cout << tb.getLine(i) << "\n";
    }
    */

    Editor editor(argv[1]);
    editor.run();

    return 0;
}
