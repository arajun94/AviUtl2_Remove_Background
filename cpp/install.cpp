#include <Windows.h>
#include <Shlwapi.h>
#include <cstdlib>
#include <shobjidl.h>


#define PROJECT_NAME L"Aviutl2 Remove Background Installer"
#define ID_BUTTON_START 1001
#define ID_BUTTON_SET_INSTALL_PATH 1002

#define CLIENT_WIDTH 640
#define CLIENT_HEIGHT 360

#ifdef USE_CUDA
#define START_LABEL L"AviUtl2 Remove Background(CUDA)\nをインストールします"
#else
#define START_LABEL L"AviUtl2 Remove Background\nをインストールします"
#endif

#define DEFAULT_INSTALL_PATH L"C:\\ProgramData\\aviutl2"


HWND hwnd_install_window;
HWND hwnd_label;
HWND hwnd_label2;
HWND hwnd_button_start;
HWND hwnd_edit_install_path;
HWND hwnd_button_set_install_path;

WCHAR install_path[MAX_PATH] = DEFAULT_INSTALL_PATH;

bool SelectFolder(HWND hwnd, PWSTR outPath)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return false;

    IFileDialog* pDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog));
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    DWORD options;
    pDialog->GetOptions(&options);
    pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    hr = pDialog->Show(hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = nullptr;
        if (SUCCEEDED(pDialog->GetResult(&pItem))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                StrCpyW(outPath, path);
                CoTaskMemFree(path);
                pItem->Release();
                pDialog->Release();
                CoUninitialize();
                return true;
            }
            pItem->Release();
        }
    }

    pDialog->Release();
    CoUninitialize();
    return false;
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

    //エラー
    if(!GetExitCodeProcess(pi.hProcess, &exitCode) || exitCode != 0){
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(command);
        return 0;
    }


    //正常
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(command);
    return 1;
}

BOOL install(){
    SetWindowText(hwnd_label, L"ファイルをコピー中");

    WCHAR python_dir[MAX_PATH];
    wsprintf(python_dir, L"%s\\Plugin\\ARB\\Python", install_path);

    WCHAR plugin_copy_cmd[MAX_PATH+10];
    wsprintf(plugin_copy_cmd, L"xcopy /e /y .\\Plugin %s\\Plugin", install_path);

    WCHAR script_copy_cmd[MAX_PATH+10];
    wsprintf(script_copy_cmd, L"xcopy /e /y .\\Script %s\\Script", install_path);

    PWSTR plugin_dir = plugin_copy_cmd+21;
    PWSTR script_dir = script_copy_cmd+21;

    
    if(!PathFileExists(plugin_dir) || !PathFileExists(script_dir)){
        MessageBox(hwnd_install_window, L"Pluginフォルダ、Scriptフォルダを含むフォルダを指定して下さい。", script_dir, MB_OKCANCEL);
        return 0;
    }



    if(!cmd(plugin_copy_cmd)){
        MessageBox(hwnd_install_window, L"ファイルコピーに失敗\nAviutl2を開いている場合は閉じて下さい", L"エラー", MB_OK);
        return 0;
    }

    if(!cmd(script_copy_cmd)){
        MessageBox(hwnd_install_window, L"ファイルコピーに失敗\nAviutl2を開いている場合は閉じて下さい", L"エラー", MB_OK);
        return 0;
    }

    SetWindowText(hwnd_label, L"Python依存関係をインストール中");

    SetCurrentDirectory(python_dir);

    if(PathFileExists(L".\\venv")){
        #ifndef DEBUG
        
        SetWindowText(hwnd_label_text, L"既に存在するvenvフォルダを削除");
        if(!cmd(L"cmd.exe /c rmdir /s /q .\\venv")){
            MessageBox(hwnd_install_window, L"venvフォルダの削除に失敗", L"エラー", MB_OK);
            return 0;
        }
        
        #else 

        SetWindowText(hwnd_label, L"完了");
        return 1;

        #endif
    }
    SetWindowText(hwnd_label, L"venvを作成中");

    if(!cmd(L"python.exe -m venv venv")){
        MessageBox(hwnd_install_window, L"venvの作成に失敗", L"エラー", MB_OK);
        return 0;
    }

    SetWindowText(hwnd_label, L"ライブラリをインストール中（数分かかります）");

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
    return 1;
}





LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    static BOOL started = 0;
    switch (message) {
        case WM_COMMAND:{
			switch (LOWORD(wparam)) {
				case ID_BUTTON_SET_INSTALL_PATH: {

                    if(SelectFolder(hwnd, install_path)){

                        if(!PathFileExists(install_path)){
                            MessageBox(hwnd_install_window, L"存在しないパス", L"エラー", MB_OK);
                            return 0;
                        }

                        SetWindowText(hwnd_edit_install_path, install_path);
                    }

					return 0;
					break;
				}




				case ID_BUTTON_START: {
                    if(!started){
                        WCHAR install_path[MAX_PATH];
                        GetWindowText(hwnd_edit_install_path, install_path, MAX_PATH);
                        if(!PathFileExists(install_path)){
                            MessageBox(hwnd_install_window, L"存在しないパス", L"エラー", MB_OK);
                            return 0;
                        }
                        started = 1;
                        EnableWindow(hwnd_button_start, FALSE);
                        EnableWindow(hwnd_button_set_install_path, FALSE); 
                        if(install()){
                            SetWindowText(hwnd_label, L"インストール完了");
                        }else{
                            SetWindowText(hwnd_label, L"インストール失敗");
                        }
                        SetWindowText(hwnd_button_start, L"終了");
                        EnableWindow(hwnd_button_start, TRUE);
                        return 0;
                    }else{
                        DestroyWindow(hwnd_install_window);
                        return 0;
                    }
                    break;
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){

    RECT window_space {0, 0, CLIENT_WIDTH, CLIENT_HEIGHT};

    AdjustWindowRectEx(&window_space, WS_OVERLAPPEDWINDOW, FALSE, 0);

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
		CW_USEDEFAULT, CW_USEDEFAULT, window_space.right - window_space.left, window_space.bottom-window_space.top,
		nullptr,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    ShowWindow(hwnd_install_window, nCmdShow);

    hwnd_label = CreateWindowEx(
		0,
		L"STATIC",
		START_LABEL,
		WS_VISIBLE | WS_CHILD | ES_CENTER,
		10, 40, CLIENT_WIDTH-20, 100,
		hwnd_install_window,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    hwnd_label2 = CreateWindowEx(
		0,
		L"STATIC",
		L"インストール先（PluginフォルダやScriptフォルダを含むフォルダ）",
		WS_VISIBLE | WS_CHILD | ES_CENTER,
		10, 160, CLIENT_WIDTH-20, 30,
		hwnd_install_window,
		nullptr,
		GetModuleHandle(0),
		nullptr);

    hwnd_button_set_install_path = CreateWindowEx(
		0,
		L"BUTTON",
		L"変更",
		WS_VISIBLE | WS_CHILD,
		CLIENT_WIDTH/2 - 300, 200, 120, 40,
		hwnd_install_window,
		(HMENU)ID_BUTTON_SET_INSTALL_PATH,
		GetModuleHandle(0),
		nullptr
    );

    hwnd_edit_install_path = CreateWindowEx(
		0,
		L"EDIT",
		DEFAULT_INSTALL_PATH,
		WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
		CLIENT_WIDTH/2 - 170, 200, 470, 40,
		hwnd_install_window,
        nullptr,
		GetModuleHandle(0),
		nullptr
	);

    hwnd_button_start = CreateWindowEx(
		0,
		L"BUTTON",
		L"インストール",
		WS_VISIBLE | WS_CHILD,
		CLIENT_WIDTH-10-160, CLIENT_HEIGHT-10-40, 160, 40,
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