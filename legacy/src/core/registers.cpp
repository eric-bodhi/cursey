#include "registers.h"

#include <cctype>
#include <utility>

bool Registers::is_valid(const char name) {
    return name == '"' || name == '-' || name == '_' ||
           std::isalnum(static_cast<unsigned char>(name));
}

Register* Registers::slot(const char name) {
    return const_cast<Register*>(std::as_const(*this).slot(name));
}

const Register* Registers::slot(const char name) const {
    if (name == '"') {
        return &m_unnamed;
    }
    if (name == '-') {
        return &m_small_delete;
    }
    if (name >= '0' && name <= '9') {
        return &m_numbered[name - '0'];
    }
    const char lower = static_cast<char>(std::tolower(name));
    if (lower >= 'a' && lower <= 'z') {
        return &m_named[lower - 'a'];
    }
    return nullptr;
}

void Registers::write_named(const char name, const Register& reg) {
    if (std::isupper(static_cast<unsigned char>(name))) {
        const std::size_t idx = std::tolower(name) - 'a';
        Register& dst = m_named[idx];
        if (!m_set[10 + idx]) {
            dst = reg;
        } else if (dst.linewise || reg.linewise) {
            dst.text += "\n" + reg.text;
            dst.linewise = true;
        } else {
            dst.text += reg.text;
        }
        m_set[10 + idx] = true;
        return;
    }
    set(name, reg);
}

void Registers::set(const char name, const Register& reg) {
    if (Register* r = slot(name)) {
        *r = reg;
        if (name >= '0' && name <= '9') {
            m_set[name - '0'] = true;
        } else if (std::isalpha(static_cast<unsigned char>(name))) {
            m_set[10 + (std::tolower(name) - 'a')] = true;
        }
    }
}

void Registers::yank(const char name, const Register& reg) {
    if (name == '_') {
        return;
    }
    if (name == '"') {
        set('0', reg);
    } else {
        write_named(name, reg);
    }
    m_unnamed = name == '"' ? reg : *slot(name);
}

void Registers::del(const char name, const Register& reg) {
    if (name == '_') {
        return;
    }
    if (name != '"') {
        write_named(name, reg);
        m_unnamed = *slot(name);
        return;
    }
    if (reg.linewise || reg.text.find('\n') != std::string::npos) {
        // Shift "1 -> "2 -> ... -> "9.
        for (int i = 9; i > 1; --i) {
            m_numbered[i] = m_numbered[i - 1];
            m_set[i] = m_set[i - 1];
        }
        set('1', reg);
    } else {
        m_small_delete = reg;
    }
    m_unnamed = reg;
}

std::optional<Register> Registers::get(const char name) const {
    if (name == '"' || name == '-') {
        const Register* r = slot(name);
        if (r->text.empty() && !r->linewise) {
            return std::nullopt;
        }
        return *r;
    }
    if (name >= '0' && name <= '9') {
        if (!m_set[name - '0']) {
            return std::nullopt;
        }
        return m_numbered[name - '0'];
    }
    if (std::isalpha(static_cast<unsigned char>(name))) {
        const std::size_t idx = std::tolower(name) - 'a';
        if (!m_set[10 + idx]) {
            return std::nullopt;
        }
        return m_named[idx];
    }
    return std::nullopt;
}
