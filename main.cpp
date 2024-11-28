#include "src/tui.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

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

    return 0;
}
