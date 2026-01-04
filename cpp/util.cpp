#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <tuple>

#include "util.hpp"


namespace util{

    /*
    char* wstr2str(const wchar_t* w){
        size_t len = wcstombs(nullptr, w, 0);
        char* s = (char*)malloc(len + 1);

        wcstombs(s, w, len + 1);

        return s;
    }*/

    char* wstr2str(const wchar_t* w){
        int len = WideCharToMultiByte(
            CP_UTF8, 0,
            w, -1,
            nullptr, 0,
            nullptr, nullptr
        );

        char* s = (char*)malloc(len);

        WideCharToMultiByte(
            CP_UTF8, 0,
            w, -1,
            s, len,
            nullptr, nullptr
        );
        return s;
    }
    
    /*wchar_t* str2wstr(const char* w){
        size_t len = mbstowcs(nullptr, w, 0);
        wchar_t* s = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));

        mbstowcs(s, w, len + 1);

        return s;
    }*/

    wchar_t* str2wstr(const char* s){
        int len = MultiByteToWideChar(
            CP_UTF8, 0,
            s, -1,
            nullptr, 0
        );

        wchar_t* w = (wchar_t*)malloc(len * sizeof(wchar_t));

        MultiByteToWideChar(
            CP_UTF8, 0,
            s, -1,
            w, len
        );
        return w;
    }

    std::tuple<char**, int> splitStr(char* s, char p){
        int count = 1;
        int start = 0;
        for(int i=0;i<strlen(s);i++){
            //連続、最初
            if(i==start && s[i] == p)continue;
            //最後
            if(start>=strlen(s))continue;
            if(i == strlen(s)-1 || s[i+1] == p){
                count++;
            }
        }
        char** result = (char**)malloc(sizeof(char*)*count);
        count = 0;
        start = 0;
        for(int i=0;i<strlen(s);i++){
            //連続、最初
            if(i==start && s[i] == p)continue;
            //最後
            if(start>=strlen(s))continue;
            if(i == strlen(s)-1 || s[i+1] == p){
                char* tmp = (char*)malloc(sizeof(char)*(i-start+2));
                memcpy(tmp,s+start,i-start+1);
                tmp[i-start+1] = '\0';
                result[count] = tmp;
                count++;
                start = i+2;
                i++;
            }
        }

        return std::forward_as_tuple(result, count);
    }

    char* decorPath(char* p, char* d){
        char** split;
        int len;
        std::tie(split, len) = splitStr(p, '.');
        
        if(len==1)return p;
        
        char* result = (char*)malloc(sizeof(char*));
        result[0] = '\0';
        char* tmp;
        int i;
        for(i=0;i<len-1;i++){
            tmp = combineStr(result, split[i]);
            free(result);
            result = tmp;
        }
        std::cout << len << std::endl;
        tmp = combineStr(result, d, ".", split[i]);
        free(result);
        return tmp;
    }
}