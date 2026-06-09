#pragma once

#include <algorithm>
#include <string_view>

#include "generated/translate/translate.h"

namespace i18n {
    std::string operator""_tr (const char16_t*, size_t);

    enum class Lang { En, Ru };

    inline Lang currentLanguage = Lang::Ru;

    inline Lang current() noexcept { return currentLanguage; }
    inline void setCurrent(Lang lang) noexcept { currentLanguage = lang; }
    inline void toggle() noexcept { currentLanguage = currentLanguage == Lang::Ru ? Lang::En : Lang::Ru; }

    class Table {
    public:
        [[nodiscard]] static constexpr std::string_view get(std::string_view key, Lang lang) {
            auto it = std::lower_bound(detail::kTranslations.begin(), detail::kTranslations.end(), key,
                                       [](const detail::Row& row, std::string_view k) { return std::string_view(row.key) < k; });

            if (it != detail::kTranslations.end() && it->key == key) {
                return lang == Lang::Ru ? it->ru : it->en;
            }

            return key;
        }
    };

    inline std::string_view tr(std::string_view key) noexcept { return Table::get(key, currentLanguage); }
    constexpr std::string_view tr(std::string_view key, Lang lang) noexcept { return Table::get(key, lang); }

    inline std::string_view operator""_tr(const char* key, size_t) noexcept { return i18n::tr(key); }
}
