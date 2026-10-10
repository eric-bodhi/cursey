#pragma once

#include "core/editor.h"
#include "core/keys.h"
#include "core/types.h"
#include <ostream>
#include <string>
#include <string_view>

inline std::ostream& operator<<(std::ostream& os, const Position& p) {
    return os << "(" << p.row << "," << p.col << ")";
}

// An editor with some text in it, driven by vim key notation.
struct Session {
    Editor editor;

    explicit Session(const std::string_view text) {
        editor.buffer().set_text(text);
    }

    Session& keys(const std::string_view notation) {
        for (const int k : key::parse(notation)) {
            editor.handle_key(k);
        }
        return *this;
    }

    std::string text() const {
        return editor.buffer().text();
    }

    Position cursor() const {
        return editor.cursor();
    }
};

// Feed `keys` to an editor containing `text` and return the resulting text.
inline std::string run(const std::string_view text,
                       const std::string_view keys) {
    Session s(text);
    s.keys(keys);
    return s.text();
}
