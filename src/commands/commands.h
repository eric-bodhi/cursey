#pragma once

#include "../core/editor.h"
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Command {
    //...
    extern std::unordered_map<std::string_view, std::function<void(Editor&)>> ftable;
}
