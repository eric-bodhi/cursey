#include <cstddef>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
        tcgetattr(STDIN_FILENO,
                  &origTermios); // Get the current terminal attributes
        struct termios raw = origTermios;

        // Disable canonical mode and echo
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        // Disable Ctrl-S/Ctrl-Q flow control
        raw.c_iflag &= ~(IXON);
        // Disable automatic carriage returns
        raw.c_oflag &= ~(OPOST);

        tcsetattr(STDIN_FILENO, TCSAFLUSH,
                  &raw); // Apply the new terminal attributes
    }

    void disableRawMode() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH,
                  &origTermios); // Restore the original attributes
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

public:
    Cursey(const Position pos = {1, 1}) : cursor(pos) {
    }

    void clear_screen() {
        write(STDOUT_FILENO, "\x1b[2J", 4); // Clear entire screen
    }

    void move_cursor(const Position pos) {
        if (pos.row > max_row)
            cursor.row = max_row;
        if (pos.row < 1)
            cursor.row = 1;
        if (pos.col > max_col)
            cursor.col = max_col;
        if (pos.col < 1)
            cursor.col = 1;

        // ANSI escape code to move cursor to pos
        std::string seq = "\x1b[" + std::to_string(pos.row) + ";" +
                          std::to_string(pos.col) + "H";
        write(STDOUT_FILENO, seq.c_str(), seq.size());
    }

    void move(Direction direction) {
        switch (direction) {
        case Direction::Up:
            if (cursor.row > 1)
                --cursor.row;
            break;

        case Direction::Down:
            if (cursor.row < max_row)
                ++cursor.row;
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

        move_cursor(cursor);
    }

    void refresh_screen() {
        clear_screen();
        move_cursor(cursor);
    }
};

int main() {
    TermManager tm; // Automatically manages terminal state
    Cursey cursey;

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
}
