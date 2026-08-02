#pragma once

#include <string>
#include <string_view>
#include <stdexcept>

#include "Lattice/Engine/physics/Atom/AtomData.h"

namespace {
std::string trim(std::string_view value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

std::string uppercase(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string normalizeElementSymbol(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return {};
    }

    std::string letters;
    letters.reserve(value.size());
    for (char ch : value) {
        if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
            letters.push_back(ch);
        }
    }

    if (letters.empty()) {
        return letters;
    }

    letters = uppercase(letters);
    if (letters.size() == 1) {
        return letters;
    }

    letters.resize(2);
    letters[1] = static_cast<char>(std::tolower(static_cast<unsigned char>(letters[1])));
    return letters;
}

AtomData::Type parseAtomTypeFromSymbol(const std::string& rawSymbol) {
    const std::string symbol = normalizeElementSymbol(rawSymbol);
    for (size_t i = 0; i < static_cast<size_t>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        if (AtomData::symbol(type) == symbol) {
            return type;
        }
    }

    throw std::runtime_error("MoleculeParser: unknown atom type '" + rawSymbol + "'");
}

int parseIntField(const std::string& line, size_t offset, size_t width) {
    return std::stoi(trim(std::string_view(line).substr(offset, width)));
}

float parseFloatField(const std::string& line, size_t offset, size_t width) {
    return std::stof(trim(std::string_view(line).substr(offset, width)));
}
}