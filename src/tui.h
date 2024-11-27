#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

struct Position {
    std::size_t row;
    std::size_t col;
};

class TermManager {
public:
    TermManager() {
        enableRawMode();
    }

    ~TermManager() {
        disableRawMode();
    }

private:
    struct termios origTermios;

    void enableRawMode() {
        tcgetattr(STDIN_FILENO, &origTermios); // Get current terminal attributes
        struct termios raw = origTermios;

        // Disable canonical mode, echo, and signal generation
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        // Disable Ctrl-S/Ctrl-Q flow control
        raw.c_iflag &= ~(IXON);
        // Disable automatic carriage returns
        raw.c_oflag &= ~(OPOST);

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Apply new terminal attributes
    }

    void disableRawMode() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios); // Restore original attributes
    }

public:
    Position get_terminal_size() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return {w.ws_row, w.ws_col};
    }
};

enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

class Cursey {
private:
    Position cursor;
    TermManager tm;

    const Position boundary = tm.get_terminal_size();
    const std::size_t max_row = boundary.row - 1;
    const std::size_t max_col = boundary.col - 1;

    std::vector<std::string> file_contents;
    std::size_t view_offset = 0; // Tracks the top line displayed on the screen

public:
    Cursey(const Position pos = {1, 1}) : cursor(pos) {}

    void clear_screen() {
        write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
        write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
    }

    void move_cursor(const Position pos) {
        std::string seq = "\x1b[" + std::to_string(pos.row) + ";" +
                          std::to_string(pos.col) + "H";
        write(STDOUT_FILENO, seq.c_str(), seq.size());
    }

    void move(Direction direction) {
        switch (direction) {
        case Direction::Up:
            if (cursor.row > 1)
                --cursor.row;
            else if (view_offset > 0)
                --view_offset; // Scroll up if at the top
            break;

        case Direction::Down:
            if (cursor.row < max_row)
                ++cursor.row;
            else if (view_offset + max_row < file_contents.size())
                ++view_offset; // Scroll down if at the bottom
            break;

        case Direction::Left:
            if (cursor.col > 1)
                --cursor.col;
            break;

        case Direction::Right:
            if (cursor.col < max_col)
                ++cursor.col;
            break;
        }

        refresh_screen();
    }

    void load_file(const std::string &filename) {
        std::ifstream file(filename);
        if (!file) {
            throw std::runtime_error("Could not open file: " + filename);
        }

        file_contents.clear();
        std::string line;
        while (std::getline(file, line)) {
            file_contents.push_back(line);
        }
    }

    void render_file() {
        clear_screen();

        for (std::size_t i = 0; i < max_row && i + view_offset < file_contents.size(); ++i) {
            const std::string &line = file_contents[i + view_offset];
            write(STDOUT_FILENO, line.c_str(), std::min(line.size(), max_col));
            write(STDOUT_FILENO, "\r\n", 2); // Move to the next line
        }
    }

    void refresh_screen() {
        render_file();
        move_cursor(cursor);
        clear_screen();
    }
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    TermManager tm; // Automatically manages terminal state
    Cursey cursey;

    try {
        cursey.load_file(argv[1]); // Load the specified file
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

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
