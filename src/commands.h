#pragma once

#include "cursor.h"
#include "editor.h"

class DeleteLine : public Command {
private:
    CursorManager& cm_;
    Logger& lg_;
public:
    DeleteLine(CursorManager& cm, Logger& lg) : cm_(cm), lg_(lg) {
    }
    void execute(Editor& editor) override {
        auto& tb = editor.getBuffer();
        tb.deleteLine(cm_);
        lg_.log(std::to_string(cm_.get().row));
        lg_.log(std::to_string(tb.lineCount()));
        if (cm_.get().row == tb.lineCount()) {
            lg_.log("Moved up");
            cm_.moveDir(Direction::Up);
        }
        editor.updateView();
        lg_.log("Updated View");
    }
};
