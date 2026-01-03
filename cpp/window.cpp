#include <windows.h>
#include <commctrl.h>
#include <cerrno>
#include <cstdlib>
#include <Shlwapi.h>
#include <clocale>

#include "plugin2.h"
#include "logger2.h"

#include "util.hpp"

#define PROJECT_NAME L"AviUtl2 Remove Background"
#define ID_BUTTON_FILE_OPEN 1001
#define ID_EDIT_SCALE 1002
#define ID_EDIT_FILE_PATH 1004
#define ID_BUTTON_EXEC 1005
EDIT_HANDLE* edit_handle;
LOG_HANDLE* logger;
HWND hwnd_edit_file_path;
HWND hwnd_edit_scale;
HWND hwnd_combo_model_name;

PCWSTR strItem[] = {
    L"sam2.1_hiera_tiny",
    L"sam2.1_hiera_small",
    L"sam2.1_hiera_base_plus",
    L"sam2.1_hiera_large"
};

typedef struct{
	CHAR playback_position[32];
	CHAR path[MAX_PATH];
	OBJECT_HANDLE* object;
	CHAR alias_data[1024];
} VideoData;
VideoData* video_data;

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
					//エイリアスデータ取得
					video_data = (VideoData*)malloc(sizeof(VideoData));
					memset(video_data, 0, sizeof(VideoData));
					video_data->object = (OBJECT_HANDLE*)malloc(sizeof(OBJECT_HANDLE));
					edit_handle->call_edit_section_param(video_data, [](void* param, EDIT_SECTION* edit) {
						typedef struct{
							CHAR playback_position[32];
							CHAR path[MAX_PATH];
							OBJECT_HANDLE* object;
							CHAR alias_data[1024];
						} VideoData;
						VideoData* video_data = (VideoData*)param;
						OBJECT_HANDLE object_handle = edit->get_focus_object();
						PCSTR item_value = edit->get_object_item_value(object_handle, L"動画ファイル", L"再生位置");
						PCSTR file_path = edit->get_object_item_value(object_handle, L"動画ファイル", L"ファイル");
						if(item_value!=nullptr && file_path !=nullptr){
							strncpy(video_data->playback_position, item_value, 32);
							strncpy(video_data->path, file_path, MAX_PATH);
							*video_data->object = object_handle;
							strncpy(video_data->alias_data, edit->get_object_alias(object_handle), 1024);
						}
					});
					if(video_data->playback_position[0]=='\0'){
						MessageBoxEx(hwnd, L"動画オブジェクト取得失敗", L"エラー", 0, 0);
						break;
					}
					if(!PathFileExists(util::str2wstr(video_data->path))){
						MessageBoxEx(hwnd, L"動画のパスが不正です", L"エラー", 0, 0);
						break;
					}

					SetWindowText(
						hwnd_edit_file_path, util::str2wstr(video_data->path)
					);

					return 0;
					break;
				}
				case ID_BUTTON_EXEC: {
					PSTR venv = "C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\venv\\Scripts\\python.exe";
					PSTR py = "C:\\ProgramData\\aviutl2\\Plugin\\ARB\\Python\\sam2_video.py";
					PWSTR video = util::str2wstr(video_data->path);

					if(!PathFileExists(video)){
						MessageBoxEx(hwnd, L"動画のパスが不正です", L"エラー", 0, 0);
						break;
					}

					//スケール取得
					WCHAR s[16];
					GetWindowText(hwnd_edit_scale, s, 16);
					wchar_t* end_wc;
					errno = 0;
					FLOAT scale = wcstod(s, &end_wc);
					if (errno != 0 || *end_wc != L'\0') {
						MessageBoxEx(hwnd, L"スケールの値が不正です", L"エラー", 0, 0);
						break;
					}

					//エイリアスデータからstart,endを取得
					PSTR* split;
					WORD _;
					std::tie(split, _) = util::splitStr(video_data->playback_position, ',');
					PSTR end_c;
					FLOAT start = strtod(split[0], &end_c);
					if (errno != 0 || *end_c != '\0') {
						break;
					}
					FLOAT end = strtod(split[1], &end_c);
					if (errno != 0 || *end_c != '\0') {
						break;
					}

					//モデルネーム取得
					PCSTR model_name = util::wstr2str(strItem[SendMessage(hwnd_combo_model_name , CB_GETCURSEL , 0 , 0)]);

					PSTR mask = util::decorPath(util::wstr2str(video), util::combineStr("_mask_", split[0], "_", split[1]));

					WORD i=0;
					PSTR decor;
					while(PathFileExists(util::str2wstr(mask))){
						decor = (PSTR)malloc(sprintf(nullptr, "_mask_%s_%s_%d",split[0], split[1], i)*sizeof(CHAR));
						sprintf(decor, "_%d", i);
						mask = util::decorPath(mask, decor);
						free(decor);
					}

					//Python呼び出し
					CHAR command[1024];
					sprintf(command, "%s %s \"%s\" %f %.3f %.3f %s", venv, py, util::wstr2str(video), scale, start, end, model_name);
					if(system(command)!=0){
						break;
					}

					//コールバック関数にvideo_dataを渡し、コールバック関数内で動画オブジェクトのaliasを取得し、sprintfで書き換え、動画オブジェクトに書き込む
					
					//エイリアス作成
					strncpy(video_data->alias_data, util::combineStr(video_data->alias_data, 
u8R"(
[Object.2]
effect.name=動画マスク
動画ファイル=)", mask, u8R"(
オフセット=)", split[0]), 1024);
					//strncpy(video_data->path, mask, MAX_PATH);
					edit_handle->call_edit_section_param(video_data, [](void* param, EDIT_SECTION* edit) {
						typedef struct{
							CHAR playback_position[32];
							CHAR path[MAX_PATH];
							OBJECT_HANDLE* object;
							CHAR alias_data[1024];
						} VideoData;
						VideoData* video_data = (VideoData*)param;
						// 古いオブジェクトを削除
						edit->delete_object(*video_data->object);
						logger->log(logger, L"delete object");
						// エイリアスデータからオブジェクトを作成
						OBJECT_HANDLE new_object = edit->create_object_from_alias(video_data->alias_data, edit->info->layer, edit->info->frame, 10);
						if (new_object) {
							logger->log(logger, L"create alias object");
							edit->set_focus_object(new_object);
						} else {
							logger->warn(logger, L"create alias failed");
						}
					});
					return 0;
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
		L"動画オブジェクト",
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
		L"選択",
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
		L"（トラックバーから動画オブジェクトを選択し、選択ボタンを押して下さい）",
		WS_VISIBLE | WS_CHILD | ES_READONLY,
		440, 10, 800, 40,
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

