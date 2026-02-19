#pragma once

#include <cwchar>   // wcslen, wcscpy
#include <cstddef>  // size_t
#include <cstring>
#include <string>
#include <vector>
#include <tuple>
#include <Windows.h>

namespace util{

    wchar_t* str2wstr(const char* w);

    char* wstr2str(const wchar_t* w);
    
    std::vector<std::string> splitStr(std::string, const char);

    std::string decorPath(const std::string&, const std::string&);

    std::string setExt(const std::string&, const std::string&);

    BOOL cmd(PCWSTR c, BOOL show_window);

    std::string cmd_get_out(PCWSTR c);

    std::string path_duplicate_numbering(const std::string& file_path);
}