// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_URI_H
#define BIBBLELSP_LSP_URI_H

#include <uriparser/Uri.h>

#include <filesystem>
#include <string>

namespace bibblec::lsp {
    inline std::string PathToFileUri(const std::filesystem::path& path) {
#ifdef _WIN32
        const auto native = path.wstring();

        std::string result(7 + 12 * native.size() + 1, '\0');

        const int error = uriWindowsFilenameToUriStringW(native.c_str(), result.data());
#else
        const auto native = path.native();

        std::string result(7 + 3 * native.size() + 1, '\0');

        const int error = uriUnixFilenameToUriStringA(native.c_str(),result.data());
#endif

        if (error != URI_SUCCESS) throw std::runtime_error("failed to convert path to uri"); // temporary error solution

        result.resize(std::strlen(result.c_str()));
        return result;
    }

    inline std::filesystem::path UriToPath(const std::string& uri) {
#ifdef _WIN32
        std::wstring result(uri.size() + 1, L'\0');

        const int error = uriUriStringToWindowsFilenameW(
            uri.c_str(),
            result.data()
        );
#else
        std::string result(uri.size() + 1, '\0');

        const int error = uriUriStringToUnixFilenameA(
            uri.c_str(),
            result.data()
        );
#endif

#ifdef _WIN32
        using CharType = wchar_t;
#else
        using CharType = char;
#endif

        if (error != URI_SUCCESS) throw std::runtime_error("failed to convert uri to path"); // temporary

        result.resize(std::char_traits<CharType>::length(result.c_str()));
        return std::filesystem::path(result);
    }
}

#endif //BIBBLELSP_LSP_URI_H
