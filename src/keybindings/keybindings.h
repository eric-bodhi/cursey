#pragma once

#include "../core/editor.h"
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Keybindings {
    //...
    extern std::unordered_map<std::string_view, std::function<void(Editor&)>> normalkeys;
    extern std::unordered_map<std::string_view, std::function<void(Editor&)>> visualkeys;
}
