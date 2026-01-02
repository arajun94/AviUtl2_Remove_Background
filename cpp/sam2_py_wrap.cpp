#include <windows.h>
#include <Python.h>
#include <iostream>

/*

sam2のPythonスクリプトのラッパー
引数
動画ファイル名 フレームレート 
出力フォルダにマスク構造体を保存

*/

int main(int argc, char** argv){
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if(argc < 4){
        throw std::runtime_error("引数が足りません");
    }

    std::cout << "start" << std::endl;

    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    PyConfig_SetString(&config, &config.program_name,
        L"C:\\ProgramData\\aviutl2\\Plugin\\python\\venv\\Scripts\\python.exe");
    PyConfig_SetString(&config, &config.home,
        L"C:\\Users\\arajun\\AppData\\Local\\Programs\\Python\\Python313");
    status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        Py_ExitStatusException(status);
    }
    
    wchar_t *py_argv[3];
    py_argv[0] = Py_DecodeLocale("sam2_video.py", NULL);
    py_argv[1] = Py_DecodeLocale(argv[1], NULL);
    py_argv[2] = Py_DecodeLocale(argv[2], NULL);
    py_argv[3] = Py_DecodeLocale(argv[3], NULL);

    PySys_SetArgvEx(4, py_argv, 0);

    FILE* fp = _wfopen(L"C:\\ProgramData\\aviutl2\\Plugin\\python\\sam2_video.py", L"r");
    if (PyRun_SimpleFileEx(
        fp,
        "sam2_video.py",
        1
    ) != 0) {
        PyErr_Print();
    }
}