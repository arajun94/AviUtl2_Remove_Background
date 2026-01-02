#include <Windows.h>
#include <Shlwapi.h>
#include <cstdlib>

#define PROJECT_NAME L"Aviutl2 Remove Background Installer"

HWND hwnd_install_window;

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProc(hwnd, message, wparam, lparam);
}

BOOL cmd(PCWSTR c){
    PWSTR command = (PWSTR)malloc(sizeof(c)*(wcslen(c) + 1));
    StrCpyW(command, c);
    STARTUPINFO si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    if(CreateProcess(
        nullptr,
        command,
        nullptr, nullptr,
        FALSE,
        0,
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

    DWORD exitCode = 0;
    if(!GetExitCodeProcess(pi.hProcess, &exitCode)){
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(command);
        return 0;
    }

    if (exitCode != 0) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(command);
        return 0;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(command);
    return 1;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
    WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpszClassName = PROJECT_NAME;
	wcex.lpfnWndProc = wnd_proc;
	wcex.hInstance = hInstance;
	wcex.hbrBackground = (HBRUSH)(COLOR_MENU + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hIcon         = NULL;
	wcex.lpszMenuName  = NULL;
	if (!RegisterClassEx(&wcex)) {
		return 0;
	}

    hwnd_install_window = CreateWindowEx(
		0,
		PROJECT_NAME,
		PROJECT_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
		nullptr,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    HWND hwnd_label_text = CreateWindowEx(
		0,
		L"STATIC",
		L"",
		WS_VISIBLE | WS_CHILD,
		10,10,280,100,
		hwnd_install_window,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    ShowWindow(hwnd_install_window, nCmdShow);

    SetWindowText(hwnd_label_text, L"ファイルをコピー中");

    if(!cmd(L"xcopy /e /y .\\Plugin C:\\ProgramData\\aviutl2\\Plugin")){
        MessageBox(hwnd_install_window, L"ファイルコピーに失敗\nAviutl2を開いている場合は閉じて下さい", L"エラー", MB_OK);
        return 0;
    }

    if(!cmd(L"xcopy /e /y .\\Script C:\\ProgramData\\aviutl2\\Script")){
        MessageBox(hwnd_install_window, L"ファイルコピーに失敗\nAviutl2を開いている場合は閉じて下さい", L"エラー", MB_OK);
        return 0;
    }

    SetWindowText(hwnd_label_text, L"Python依存関係をインストール中");

    SetCurrentDirectory(L"C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python");

    if(PathFileExists(L"C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\venv")){
        /*SetWindowText(hwnd_label_text, L"既に存在するvenvフォルダを削除");
        if(!cmd(L"cmd.exe /c rmdir /s /q C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\venv")){
            MessageBox(hwnd_install_window, L"venvフォルダの削除に失敗", L"エラー", MB_OK);
            return 0;
        }*/
    }else{
        SetWindowText(hwnd_label_text, L"venvを作成中");

        if(!cmd(L"python.exe -m venv venv")){
            MessageBox(hwnd_install_window, L"venvの作成に失敗", L"エラー", MB_OK);
            return 0;
        }

        SetWindowText(hwnd_label_text, L"ライブラリをインストール中（数分かかります）");

        if(!cmd(L".\\venv\\Scripts\\pip.exe install torch==2.9.1+cu130 torchvision==0.24.1+cu130 --index-url https://download.pytorch.org/whl/cu130")){
            MessageBox(hwnd_install_window, L"Pytorchのインストールに失敗", L"エラー", MB_OK);
            return 0;
        }

        if(!cmd(L".\\venv\\Scripts\\pip.exe install -r .\\requirements.txt")){
            MessageBox(hwnd_install_window, L"requirements.txtのインストールに失敗", L"エラー", MB_OK);
            return 0;
        }
    }
    SetWindowText(hwnd_label_text, L"完了");

    MessageBox(hwnd_install_window, L"インストール完了", PROJECT_NAME, MB_OK);
    return 0;
}