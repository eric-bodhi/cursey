#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

// Editor settings, changed with `:set`. To add an option: add a field here and
// an entry in the table in options.cpp.
struct Options {
    std::size_t tabstop = 4;
    std::size_t shiftwidth = 4;
    bool expandtab = true;
    bool autoindent = true;
    bool number = true;
    std::size_t scrolloff = 5;

    // Apply one `:set` argument ("ts=8", "noet", "number", "invnu", "nu?").
    // Returns an error message on failure. `shown` receives text to display
    // for queries like "ts?".
    std::optional<std::string> set(std::string_view arg, std::string& shown);
};
