#include <cstdlib>
#include <clocale>
#include <cstring>
#include <iostream>
#include <tuple>

#include "util.hpp"


namespace util{

    char* wstr2str(const wchar_t* w){
        size_t len = wcstombs(nullptr, w, 0);
        char* s = (char*)malloc(len + 1);

        wcstombs(s, w, len + 1);

        return s;
    }
    
    wchar_t* str2wstr(const char* w){
        size_t len = mbstowcs(nullptr, w, 0);
        wchar_t* s = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));

        mbstowcs(s, w, len + 1);

        return s;
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
        
        char* result = "";
        int i;
        for(i=0;i<len-1;i++){
            result = combineStr(result, split[i]);
        }
        std::cout << len << std::endl;
        result = combineStr(result, d, ".", split[i]);
        return result;
    }
}