#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <tuple>
#include <Shlwapi.h>
#include <string>
#include <vector>

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

    std::vector<std::string> splitStr(std::string str, char split){
        std::vector<std::string> out;
        str+=split;

        int start = 0;
        for(int i=0;i<str.length();i++){
            if(str[i] == split){
                if(start!=i){
                    out.push_back(str.substr(start, i-start));
                }
                start = i+1;
            }
        }

        return out;
    }

    std::string decorPath(const std::string& p, const std::string& d){
        std::vector<std::string> split = splitStr(p,'.');
        std::string out = split[0];

        for(size_t i=1;i+1<split.size();i++){
            out.push_back('.');
            out += split[i];
        }
        out += d;
        out.push_back('.');
        out += split.back();

        return out;
    }

    std::string setExt(const std::string& p, const std::string& ext){
        std::vector<std::string> split = splitStr(p,'.');
        std::string out = "";

        for(size_t i=0;i+1<split.size();i++){
            out += split[i];
            out.push_back('.');
        }
        out += ext;

        return out;
    }

    BOOL cmd(PCWSTR c, BOOL show_window){
        PWSTR command = (PWSTR)malloc(sizeof(wchar_t) * (wcslen(c) + 1));
        StrCpyW(command, c);
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        if(CreateProcess(
            nullptr,
            command,
            nullptr, nullptr,
            FALSE,
            show_window ? 0 : CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi
        )==0){
            return 0;
        }
        
        while (WaitForSingleObject(pi.hProcess, 100)) {
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        free(command);


        DWORD exitCode = 0;

        //エラー
        if(!GetExitCodeProcess(pi.hProcess, &exitCode) || exitCode != 0){
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return 0;
        }


        //正常
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }

    std::string cmd_get_out(PCWSTR c){
            SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hRead = NULL;
        HANDLE hWrite = NULL;

        // パイプ作成
        CreatePipe(&hRead, &hWrite, &sa, 0);

        // 親側で読み取りハンドルを継承させない
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);



        PWSTR command = (PWSTR)malloc(sizeof(wchar_t) * (wcslen(c) + 1));
        StrCpyW(command, c);
        STARTUPINFO si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError  = hWrite;
        si.hStdInput  = NULL;
        if(CreateProcess(
            nullptr,
            command,
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi
        )==0){
            return 0;
        }
        
        while (WaitForSingleObject(pi.hProcess, 100)) {
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        free(command);


        CloseHandle(hWrite);
        char buffer[4096];
        DWORD readSize;
        std::string output;
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &readSize, NULL) && readSize > 0) {
            buffer[readSize] = '\0';
            output += buffer;
        }
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return output;
    }
}