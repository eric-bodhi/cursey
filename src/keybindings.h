#pragma once

#include "editor.h"
#include <unordered_map>
#include <functional>
#include <string_view>

namespace Keybindings {
    //...
    extern std::unordered_map<std::string_view, std::function<void(Editor&)>> normalkeys;
}
