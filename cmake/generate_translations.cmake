if(NOT CSV_FILE)
    message(FATAL_ERROR "CSV_FILE not set")
endif()
if(NOT OUT_FILE)
    message(FATAL_ERROR "OUT_FILE not set")
endif()

file(STRINGS "${CSV_FILE}" csv_lines ENCODING UTF-8)

set(rows "")
set(count 0)
set(first_line TRUE)

foreach(line IN LISTS csv_lines)
    if(first_line)
        set(first_line FALSE)
        continue()
    endif()

    if(line STREQUAL "")
        continue()
    endif()

    string(REGEX MATCH "^([^;]*);([^;]*);([^;]*)$" _ "${line}")
    set(key "${CMAKE_MATCH_1}")
    set(en  "${CMAKE_MATCH_2}")
    set(ru  "${CMAKE_MATCH_3}")

    if(key STREQUAL "")
        continue()
    endif()

    foreach(var key en ru)
        string(REPLACE "\\" "\\\\" ${var} "${${var}}")
        string(REPLACE "\"" "\\\"" ${var} "${${var}}")
    endforeach()

    string(APPEND rows "    {\"${key}\", \"${en}\", \"${ru}\"},\n")
    math(EXPR count "${count} + 1")
endforeach()

set(content "#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace i18n::detail {

struct Row {
    const char* key;
    const char* en;
    const char* ru;
};

inline constexpr std::array<Row, ${count}> kUnsortedTranslations = {{
${rows}}};

template<std::size_t N>
[[nodiscard]] static constexpr std::array<Row, N> sort_translations(std::array<Row, N> arr) noexcept {
    std::ranges::sort(arr, [](const Row& a, const Row& b) {
        return std::string_view(a.key) < std::string_view(b.key);
    });
    return arr;
}

inline constexpr auto kTranslations = sort_translations(kUnsortedTranslations);
}
")

file(WRITE "${OUT_FILE}" "${content}")
message(STATUS "Generated ${count} translations -> ${OUT_FILE}")
