#pragma once

#include "../core/editor.h"
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Keybindings {
//...
extern std::unordered_map<std::string, std::function<void(Editor&)>>
    normal_keys;
extern std::unordered_map<std::string, std::function<void(Editor&)>>
    visual_keys;
} // namespace Keybindings
