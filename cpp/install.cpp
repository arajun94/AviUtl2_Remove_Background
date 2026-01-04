#include <Windows.h>
#include <Shlwapi.h>
#include <cstdlib>

#define PROJECT_NAME L"Aviutl2 Remove Background Installer"
#define ID_BUTTON_START 1001
#define ID_BUTTON_END 1002

#ifdef USE_CUDA

#define START_LABEL L"AviUtl2 Remove Background(CUDA)\nをインストールします"

#else

#define START_LABEL L"AviUtl2 Remove Background\nをインストールします"

#endif


HWND hwnd_install_window;
HWND hwnd_label_text;
HWND hwnd_button;

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

BOOL install(){
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
        #ifndef DEBUG
        
        SetWindowText(hwnd_label_text, L"既に存在するvenvフォルダを削除");
        if(!cmd(L"cmd.exe /c rmdir /s /q C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\venv")){
            MessageBox(hwnd_install_window, L"venvフォルダの削除に失敗", L"エラー", MB_OK);
            return 0;
        }
        
        #else 

        SetWindowText(hwnd_label_text, L"完了");
        return 1;
        
        #endif
    }
    SetWindowText(hwnd_label_text, L"venvを作成中");

    if(!cmd(L"python.exe -m venv venv")){
        MessageBox(hwnd_install_window, L"venvの作成に失敗", L"エラー", MB_OK);
        return 0;
    }

    SetWindowText(hwnd_label_text, L"ライブラリをインストール中（数分かかります）");

    #ifdef USE_CUDA
        if(!cmd(L".\\venv\\Scripts\\pip.exe install torch==2.9.1+cu130 torchvision==0.24.1+cu130 --index-url https://download.pytorch.org/whl/cu130")){
            MessageBox(hwnd_install_window, L"Pytorchのインストールに失敗", L"エラー", MB_OK);
            return 0;
        }
        if(!cmd(L".\\venv\\Scripts\\pip.exe install -r .\\requirements.txt")){
            MessageBox(hwnd_install_window, L"requirements.txtのインストールに失敗", L"エラー", MB_OK);
            return 0;
        }
    #else
        if(!cmd(L".\\venv\\Scripts\\pip.exe install -r .\\requirements_cpu.txt")){
            MessageBox(hwnd_install_window, L"requirements_cpu.txtのインストールに失敗", L"エラー", MB_OK);
            return 0;
        }
    #endif
    SetWindowText(hwnd_label_text, L"完了");
    return 1;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    static BOOL started = 0;
    switch (message) {
        case WM_COMMAND:{
			switch (LOWORD(wparam)) {
				case ID_BUTTON_START: {
                    if(!started){
                        started = 1;
                        EnableWindow(hwnd_button, FALSE); 
                        if(install()){
                        }else{
                            SetWindowText(hwnd_label_text, L"インストール失敗");
                        }
                        SetWindowText(hwnd_button, L"終了");
                        EnableWindow(hwnd_button, TRUE);
                        return 0;
                    }else{
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    break;
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);   // ← ここで終了通知
            return 0;
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
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

    ShowWindow(hwnd_install_window, nCmdShow);

    hwnd_label_text = CreateWindowEx(
		0,
		L"STATIC",
		START_LABEL,
		WS_VISIBLE | WS_CHILD,
		10,10,280,100,
		hwnd_install_window,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    hwnd_button = CreateWindowEx(
		0,
		L"BUTTON",
		L"インストール",
		WS_VISIBLE | WS_CHILD,
		90,100,100,40,
		hwnd_install_window,
		(HMENU)ID_BUTTON_START,
		GetModuleHandle(0),
		nullptr
    );

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}