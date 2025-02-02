#pragma once

#include "editor.h"
#include "cursor.h"

class DeleteLine : public Command {
private:
    const CursorManager cm_;

public:
    DeleteLine(const CursorManager& cm) : cm_(cm) {}
    void execute(Editor& editor) override {
        auto& tb = editor.getBuffer();
        tb.deleteLine(cm_);
        editor.updateView();
    }
};
