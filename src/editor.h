#include "tui.h"

class Editor {
private:
    enum class Mode {
        Normal,
        Insert,
        Visual,
    };

    Mode currMode = Mode::Normal;
    Cursey cursey;

public:
    Editor(const std::string& filepath);

    // TODO: Each respective mode will handle what happens in that given mode
    // normal mode will interpret input char as switching to new mode etc
    void normalMode(char input);
    void insertMode(char input);
    void command_mode(char input);
};
