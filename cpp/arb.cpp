#include <windows.h>
#include <commctrl.h>
#include <cerrno>
#include <cstdlib>
#include <Shlwapi.h>
#include <clocale>
#include <string>

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

WCHAR plugin_dir[MAX_PATH];

PCWSTR strItem[] = {
    L"sam2.1_hiera_tiny",
    L"sam2.1_hiera_small",
    L"sam2.1_hiera_base_plus",
    L"sam2.1_hiera_large"
};

OBJECT_HANDLE* selected_object;

typedef struct{
	CHAR playback_position[32];
	CHAR path[MAX_PATH];
	CHAR alias_data[1024];
} VideoData;

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

//プラグインフォルダを取得するための嘘の関数
void dummy(){}


//---------------------------------------------------------------------
//	ウィンドウプロシージャ
//---------------------------------------------------------------------
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
		case WM_COMMAND:
			switch (LOWORD(wparam)) {
				case ID_BUTTON_FILE_OPEN: {
					char** file_path = (char**)malloc(sizeof(char*));
					edit_handle->call_edit_section_param(file_path, [](void * param, EDIT_SECTION* edit) {
						*selected_object = edit->get_focus_object();

						const char** file_path = (const char**)param;

						*file_path = edit->get_object_item_value(*selected_object, L"動画ファイル", L"ファイル");
					});

					if(*selected_object == nullptr){
						MessageBoxEx(hwnd, L"動画オブジェクト取得失敗", L"エラー", 0, 0);
						return 0;
						break;
					}
					wchar_t* file_path_w = util::str2wstr(*file_path);
					if(!PathFileExists(file_path_w)){
						MessageBoxEx(hwnd, L"動画パスが存在しません", L"エラー", 0, 0);
						return 0;
						break;
					}
					SetWindowText(
						hwnd_edit_file_path, file_path_w
					);

					return 0;
					break;
				}

				case ID_BUTTON_EXEC: {
					std::string plugin_dir_a = util::wstr2str(plugin_dir);
					std::string venv = plugin_dir_a + "\\ARB\\Python\\venv\\Scripts\\python.exe";
					std::string py = plugin_dir_a + "\\ARB\\Python\\sam2_video.py";

					if(*selected_object == nullptr){
						MessageBoxEx(hwnd, L"動画オブジェクトを選択して下さい", L"エラー", 0, 0);
						return 0;
						break;
					}

					//ビデオデータ構造体を初期化
					VideoData* video_data = (VideoData*)malloc(sizeof(VideoData));
					memset(video_data, 0, sizeof(VideoData));
					video_data->playback_position[0]='\0';

					edit_handle->call_edit_section_param(video_data, [](void* param, EDIT_SECTION* edit) {
						typedef struct{
							CHAR playback_position[32];
							CHAR path[MAX_PATH];
							CHAR alias_data[1024];
						} VideoData;
						VideoData* video_data = (VideoData*)param;
						PCSTR item_value = edit->get_object_item_value(*selected_object, L"動画ファイル", L"再生位置");
						PCSTR file_path = edit->get_object_item_value(*selected_object, L"動画ファイル", L"ファイル");
						PCSTR alias = edit->get_object_alias(*selected_object);
						if(item_value!=nullptr && file_path !=nullptr && alias != nullptr){
							strncpy(video_data->playback_position, item_value, 32);
							strncpy(video_data->path, file_path, MAX_PATH);
							strncpy(video_data->alias_data, alias, 1024);
						}
					});

					if(video_data->playback_position[0]=='\0'){
						MessageBoxEx(hwnd, L"オブジェクトが存在しません", L"エラー", 0, 0);
						return 0;
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
						return 0;
						break;
					}

					//エイリアスデータからstart,endを取得
					PSTR* split;
					WORD len;
					std::tie(split, len) = util::splitStr(video_data->playback_position, ',');
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
					PSTR model_name = util::wstr2str(strItem[SendMessage(hwnd_combo_model_name , CB_GETCURSEL , 0 , 0)]);

					char* tmp = util::combineStr("_mask_", split[0], "_", split[1]);
					PSTR mask = util::decorPath(video_data->path, tmp);
					free(tmp);

					WORD i=0;
					PSTR decor;
					wchar_t* tmp_w = util::str2wstr(mask);
					while(PathFileExists(tmp_w)){
						decor = (PSTR)malloc(sprintf(nullptr, "_mask_%s_%s_%d",split[0], split[1], i)*sizeof(CHAR));
						sprintf(decor, "_%d", i);
						tmp = mask;
						free(mask);
						mask = util::decorPath(tmp, decor);
						free(tmp);
						free(decor);
						free(tmp_w);
						tmp_w = util::str2wstr(mask);
					}
					free(tmp_w);

					//Python呼び出し
					CHAR command[1024];
					sprintf(command, "%s %s \"%s\" %f %.3f %.3f %s", venv.c_str(), py.c_str(), video_data->path, scale, start, end, model_name);
					if(system(command)!=0){
						break;
					}

					//エイリアス作成
					tmp = util::combineStr(video_data->alias_data, 
u8R"(
[Object.2]
effect.name=動画マスク
動画ファイル=)", mask, u8R"(
オフセット=)", split[0]);
					strncpy(video_data->alias_data, tmp, 1024);
					free(tmp);
					//strncpy(video_data->path, mask, MAX_PATH);
					edit_handle->call_edit_section_param(video_data, [](void* param, EDIT_SECTION* edit) {
						typedef struct{
							CHAR playback_position[32];
							CHAR path[MAX_PATH];
							CHAR alias_data[1024];
						} VideoData;
						VideoData* video_data = (VideoData*)param;
						// 古いオブジェクトを削除
						edit->delete_object(*selected_object);
						// エイリアスデータからオブジェクトを作成
						OBJECT_HANDLE new_object = edit->create_object_from_alias(video_data->alias_data, edit->info->layer, edit->info->frame, 10);
						if (new_object) {
							logger->log(logger, L"create alias object");
							edit->set_focus_object(new_object);
						} else {
							logger->warn(logger, L"create alias failed");
						}
					});

					SetWindowText(
						hwnd_edit_file_path, L"（トラックバーから動画オブジェクトを選択し、選択ボタンを押して下さい）"
					);
					
					free(mask);
					free(model_name);
					for(WORD i=0;i<len;i++){
						free(split[i]);
					}
					free(split);
					video_data->playback_position[0]='\0';
					return 0;
					break;
				}
			}
			break;
	}
	return DefWindowProc(hwnd, message, wparam, lparam);
}

static inline void init_window(HOST_APP_TABLE* host){
// プラグインの情報を設定
	host->set_plugin_information(L"AviUtl2 Remove Background");

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
}

//---------------------------------------------------------------------
//	プラグイン登録関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
	init_window(host);
	// 編集ハンドルを作成
	edit_handle = host->create_edit_handle();

	//プラグインフォルダ取得
	HMODULE hModule;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		(LPCTSTR)&dummy,
		&hModule
	);
	GetModuleFileName(hModule, plugin_dir, MAX_PATH);
	PathRemoveFileSpec(plugin_dir);

	selected_object = (OBJECT_HANDLE*)malloc(sizeof(OBJECT_HANDLE));
	*selected_object = nullptr;
}

