#include <windows.h>
#include <commctrl.h>
#include <cerrno>
#include <cstdlib>
#include <Shlwapi.h>

#include "plugin2.h"
#include "logger2.h"

#include "util.hpp"

#define PROJECT_NAME L"AviUtl2 Remove Background"
#define ID_BUTTON_FILE_OPEN 1001
#define ID_EDIT_SCALE 1002
#define ID_EDIT_FRAME_NUM 1003
#define ID_EDIT_FILE_PATH 1004
#define ID_BUTTON_EXEC 1005
EDIT_HANDLE* edit_handle;
LOG_HANDLE* logger;
HWND hwnd_edit_file_path;
HWND hwnd_edit_scale;
HWND hwnd_edit_frame_num;
HWND hwnd_combo_model_name;

PCWSTR strItem[] = {
    L"sam2.1_hiera_tiny",
    L"sam2.1_hiera_small",
    L"sam2.1_hiera_base_plus",
    L"sam2.1_hiera_large"
};

//---------------------------------------------------------------------
//	ログ出力機能初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* handle) {
	logger = handle;
}

//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL解放関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}


//---------------------------------------------------------------------
//	ウィンドウプロシージャ
//---------------------------------------------------------------------
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
		case WM_COMMAND:
			switch (LOWORD(wparam)) {
				case ID_BUTTON_FILE_OPEN: {
					SetFocus(hwnd);
					//ファイルオープン
					WCHAR file_path[MAX_PATH] = {};
					OPENFILENAME ofn = {};
					ofn.lStructSize = sizeof(OPENFILENAME);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFilter = L"すべてのファイル\0*.*\0動画ファイル\0*.mp4;*.avi;*.wmv;*.mov;*.mkv\0";
					ofn.lpstrFile = file_path;
					ofn.nMaxFile = MAX_PATH;
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
					ofn.lpstrTitle = L"ファイルを開く";
					if (GetOpenFileName(&ofn)) {
						logger->info(logger, L"Selected file:");
						logger->info(logger, file_path);
					} else {
						logger->warn(logger, L"File open canceled or failed");
					}
					
					SetWindowText(
						hwnd_edit_file_path, file_path
					);

					return 0;
					break;
				}
				case ID_BUTTON_EXEC: {
					PSTR venv = "C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\venv\\Scripts\\python.exe";
					PSTR py = "C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\sam2_video.py";
					WCHAR video[MAX_PATH] = {};
					GetWindowText(hwnd_edit_file_path, video, MAX_PATH);

					if(!PathFileExists(video)){
						MessageBoxEx(hwnd, L"動画のパスが不正です", L"エラー", 0, 0);
						break;
					}

					//スケール取得
					WCHAR s[16];
					GetWindowText(hwnd_edit_scale, s, 16);
					wchar_t* end;
					errno = 0;
					FLOAT scale = wcstod(s, &end);
					if (errno != 0 || *end != L'\0') {
						MessageBoxEx(hwnd, L"スケールの値が不正です", L"エラー", 0, 0);
						break;
					}

					//分割フレーム数取得
					WCHAR f[16];
					GetWindowText(hwnd_edit_frame_num, f, 16);
					errno = 0;
					WORD frame_num = wcstol(f, &end, 10);
					if (errno != 0 || *end != L'\0') {
						MessageBoxEx(hwnd, L"分割フレーム数の値が不正です", L"エラー", 0, 0);
						break;
					}

					//モデルネーム取得
					PCSTR model_name = util::wstr2str(strItem[SendMessage(hwnd_combo_model_name , CB_GETCURSEL , 0 , 0)]);

					//Python呼び出し
					CHAR command[1024];
					sprintf(command, "%s %s \"%s\" %f %d %s", venv, py, util::wstr2str(video), scale, frame_num, model_name);
					if(system(command)!=0){
						break;
					}

					//エイリアス作成
					PSTR mask = util::decorPath(util::wstr2str(video), "_mask");
					CHAR alias[1024];
					sprintf(alias, u8R"([Object]
frame=0,100
[Object.0]
effect.name=動画ファイル
再生位置=0.000,3.000,再生範囲,0
再生速度=100.00
ファイル=%s
トラック=0
ループ再生=0
音声付き=1
YUV=
[Object.1]
effect.name=映像再生
X=0.00
Y=0.00
Z=0.00
Group=1
中心X=0.00
中心Y=0.00
中心Z=0.00
X軸回転=0.00
Y軸回転=0.00
Z軸回転=0.00
拡大率=100.000
縦横比=0.000
透明度=0.00
合成モード=通常
音量=100.00
左右=0.00
[Object.2]
effect.name=mask
動画ファイル=%s)", util::wstr2str(video), mask);

					edit_handle->call_edit_section_param(alias, [](void* alias, EDIT_SECTION* edit) {
						// エイリアスデータからオブジェクトを作成
						if (edit->create_object_from_alias((CHAR*)alias, edit->info->layer, edit->info->frame, 10)) {
							logger->log(logger, L"create alias object");
						} else {
							logger->warn(logger, L"create alias failed");
						}
					});
					break;
				}
			}
			break;
	}
	return DefWindowProc(hwnd, message, wparam, lparam);
}

//---------------------------------------------------------------------
//	プラグイン登録関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
	// プラグインの情報を設定
	host->set_plugin_information(L"Sample Window Client version 2.00 By ＫＥＮくん");

	// 自身のウィンドウを作成
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpszClassName = PROJECT_NAME;
	wcex.lpfnWndProc = wnd_proc;
	wcex.hInstance = GetModuleHandle(0);
	wcex.hbrBackground = (HBRUSH)(COLOR_MENU + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hIcon         = NULL;
	wcex.lpszMenuName  = NULL;
	if (!RegisterClassEx(&wcex)) {
		return;
	}
	auto hwnd = CreateWindowEx(
		0,
		PROJECT_NAME,
		PROJECT_NAME,
		WS_POPUP, // 親ウィンドウの指定無しでWS_CHILDが作れないので一旦WS_POPUPで作成しています
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr,
		nullptr,
		GetModuleHandle(0),
		nullptr);
	if (!hwnd) {
		return;
	}

	CreateWindowEx(
		0,
		WC_STATIC,
		L"元動画",
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		10, 10, 200, 40,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);

	// ボタンの作成
	CreateWindowEx(
		0,
		WC_BUTTON,
		L"ファイル選択",
		WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
		220, 10, 200, 40,
		hwnd,
		(HMENU)ID_BUTTON_FILE_OPEN,
		GetModuleHandle(0),
		nullptr
	);

	hwnd_edit_file_path = CreateWindowEx(
		0,
		WC_EDIT,
		L"（ファイルが選択されていません）",
		WS_VISIBLE | WS_CHILD | ES_READONLY,
		440, 10, 400, 40,
		hwnd,
		(HMENU)ID_EDIT_FILE_PATH,
		GetModuleHandle(0),
		nullptr
	);

	CreateWindowEx(
		0,
		WC_STATIC,
		L"マスクスケール",
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		10, 60, 200, 40,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);

	hwnd_edit_scale = CreateWindowEx(
		0,
		WC_EDIT,
		L"1.0",
		WS_VISIBLE | WS_CHILD | WS_BORDER,
		220, 60, 50, 40,
		hwnd,
		(HMENU)ID_EDIT_SCALE,
		GetModuleHandle(0),
		nullptr
	);

	CreateWindowEx(
		0,
		WC_STATIC,
		L"分割フレーム数",
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		10, 110, 200, 40,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);

	hwnd_edit_frame_num = CreateWindowEx(
		0,
		WC_EDIT,
		L"0",
		WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
		220, 110, 50, 40,
		hwnd,
		(HMENU)ID_EDIT_SCALE,
		GetModuleHandle(0),
		nullptr
	);

	CreateWindowEx(
		0,
		WC_STATIC,
		L"使用モデル",
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		10, 160, 200, 40,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);

	hwnd_combo_model_name = CreateWindowEx(
		0,
		L"COMBOBOX",
		nullptr,
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 
		220 , 160 , 300 , 300 ,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);
	for (WORD i=0;i<4;i++){
		SendMessage(hwnd_combo_model_name, CB_ADDSTRING, 0, (LPARAM)strItem[i]);
	}
	SendMessage(hwnd_combo_model_name, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);

	CreateWindowEx(
		0,
		WC_BUTTON,
		L"実行",
		WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
		10, 200, 200, 40,
		hwnd,
		(HMENU)ID_BUTTON_EXEC,
		GetModuleHandle(0),
		nullptr
	);

	// ウィンドウを登録
	host->register_window_client(PROJECT_NAME, hwnd);

	// 編集ハンドルを作成
	edit_handle = host->create_edit_handle();
}

