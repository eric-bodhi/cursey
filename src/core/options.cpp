#include "options.h"

#include <array>
#include <charconv>
#include <variant>

namespace {

struct OptionDef {
    std::string_view name;
    std::string_view abbrev;
    std::variant<bool Options::*, std::size_t Options::*> field;
};

const std::array<OptionDef, 6> option_defs = {{
    {"tabstop", "ts", &Options::tabstop},
    {"shiftwidth", "sw", &Options::shiftwidth},
    {"expandtab", "et", &Options::expandtab},
    {"autoindent", "ai", &Options::autoindent},
    {"number", "nu", &Options::number},
    {"scrolloff", "so", &Options::scrolloff},
}};

const OptionDef* find_option(const std::string_view name) {
    for (const auto& def : option_defs) {
        if (def.name == name || def.abbrev == name) {
            return &def;
        }
    }
    return nullptr;
}

} // namespace

std::optional<std::string> Options::set(std::string_view arg,
                                        std::string& shown) {
    const std::string unknown = "E518: Unknown option: " + std::string(arg);

    const std::size_t eq = arg.find('=');
    if (eq != std::string_view::npos) {
        const OptionDef* def = find_option(arg.substr(0, eq));
        if (!def) {
            return unknown;
        }
        const auto* field = std::get_if<std::size_t Options::*>(&def->field);
        const std::string_view value = arg.substr(eq + 1);
        std::size_t n = 0;
        const auto [ptr, ec] =
            std::from_chars(value.data(), value.data() + value.size(), n);
        if (!field || ec != std::errc() || ptr != value.data() + value.size()) {
            return "E521: Number required after =: " + std::string(arg);
        }
        if (n == 0 &&
            (*field == &Options::tabstop || *field == &Options::shiftwidth)) {
            return "E487: Argument must be positive: " + std::string(arg);
        }
        this->*(*field) = n;
        return std::nullopt;
    }

    bool query = false;
    if (arg.ends_with('?')) {
        query = true;
        arg.remove_suffix(1);
    }
    enum class Action { On, Off, Toggle } action = Action::On;
    const OptionDef* def = find_option(arg);
    if (!def && arg.starts_with("no")) {
        def = find_option(arg.substr(2));
        action = Action::Off;
    } else if (!def && arg.starts_with("inv")) {
        def = find_option(arg.substr(3));
        action = Action::Toggle;
    }
    if (!def) {
        return unknown;
    }

    if (const auto* num = std::get_if<std::size_t Options::*>(&def->field)) {
        // `:set ts` on a number option shows its value, like vim.
        shown = std::string(def->name) + "=" + std::to_string(this->*(*num));
        return std::nullopt;
    }
    bool& flag = this->*std::get<bool Options::*>(def->field);
    if (query) {
        shown = (flag ? "" : "no") + std::string(def->name);
        return std::nullopt;
    }
    switch (action) {
    case Action::On:
        flag = true;
        break;
    case Action::Off:
        flag = false;
        break;
    case Action::Toggle:
        flag = !flag;
        break;
    }
    return std::nullopt;
}
