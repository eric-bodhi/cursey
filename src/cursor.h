#include <cstddef>
#include "defs.h"

/*
 0 indexed (converted to 1 indexed for graphics)
 Handles all movement and position validations
*/

class CursorManager {
private:
    Cursor cursor;

public:
    CursorManager(const Cursor& argCursor = Cursor());
    const Cursor& get();
    void moveDir(Direction direction);
};
