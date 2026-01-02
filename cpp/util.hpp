#pragma once

#include <cwchar>   // wcslen, wcscpy
#include <cstddef>  // size_t
#include <cstring>
#include <tuple>

namespace util{

    template<typename... Args>
    char* combineStr(Args... args)
    {
        size_t total_len = 1; // null終端

        ((total_len += strlen(args)), ...);

        char* result = new char[total_len];
        result[0] = L'\0';

        char* cursor = result;
        ((strcpy(cursor, args), cursor += strlen(args)), ...);

        return result;
    }

    template<typename... Args>
    wchar_t* combineWStr(Args... args)
    {
        size_t total_len = 1; // null終端

        ((total_len += wcslen(args)), ...);

        wchar_t* result = new wchar_t[total_len];
        result[0] = L'\0';

        wchar_t* cursor = result;
        ((wcscpy(cursor, args), cursor += wcslen(args)), ...);

        return result;
    }

    wchar_t* str2wstr(const char* w);

    char* wstr2str(const wchar_t* w);
    
    std::tuple<char**, int> splitStr(char* s, char p);

    char* decorPath(char* p, char* d);
}