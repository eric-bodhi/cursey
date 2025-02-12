#pragma once

#include "../core/editor.h"
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Command {
    //...
    extern std::unordered_map<std::string, std::function<void(Editor&)>>
    command_table;
}
