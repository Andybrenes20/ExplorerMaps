#pragma once

enum class InterfaceLanguage {
    Spanish,
    English
};

namespace Localization {
    inline InterfaceLanguage language = InterfaceLanguage::Spanish;
    inline bool shadowsEnabled = true;

    inline const char* Text(const char* spanish, const char* english) {
        return language == InterfaceLanguage::English ? english : spanish;
    }
}
