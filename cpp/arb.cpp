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
#define ERROR_CAPTION L"AviUtl2 Remove Background Error"

#define ID_BUTTON_FILE_OPEN 1001
#define ID_EDIT_SCALE 1002
#define ID_EDIT_FILE_PATH 1004
#define ID_BUTTON_EXEC 1005

EDIT_HANDLE* edit_handle;
LOG_HANDLE* logger;

HWND hwnd_edit_file_path;
HWND hwnd_edit_scale;
HWND hwnd_combo_model_name;
HWND hwnd_button_exec;

WCHAR plugin_dir[MAX_PATH];

PCWSTR strItem[] = {
    L"sam2.1_hiera_tiny",
    L"sam2.1_hiera_small",
    L"sam2.1_hiera_base_plus",
    L"sam2.1_hiera_large"
};

OBJECT_HANDLE* selected_object;
WORD object_type;
#define OBJECT_TYPE_VIDEO 1
#define OBJECT_TYPE_IMAGE 0


typedef struct{
	std::string playback_position;
	std::string path;
	std::string alias_data;
	OBJECT_LAYER_FRAME layer_frame;
} ObjectData;

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


//エイリアス文字列から特定の値を取得
//edit->get_object_item_valueはたまに信用ならない
std::string get_object_item_value(std::string alias_data, std::string effect_name, std::string key){
	size_t now_pos = alias_data.find("effect.name="+effect_name);
	if(now_pos == std::string::npos)return "";
	now_pos = alias_data.find("\n"+key, now_pos);
	if(now_pos == std::string::npos)return "";
	now_pos = alias_data.find("=",now_pos);
	if(now_pos == std::string::npos)return "";

	size_t start_pos = now_pos+1;

	size_t end_pos = alias_data.find("\n", start_pos)-1;
	if(end_pos == std::string::npos)return "";

	return alias_data.substr(start_pos, end_pos-start_pos);
}

bool exec_do(HWND hwnd){
	std::string plugin_dir_a = util::wstr2str(plugin_dir);
	std::string venv = plugin_dir_a + "\\ARB\\Python\\venv\\Scripts\\python.exe";
	std::string video_py = plugin_dir_a + "\\ARB\\Python\\sam2_video.py";
	std::string image_py = plugin_dir_a + "\\ARB\\Python\\sam2_image.py";

	//スケール取得
	WCHAR s[16];
	GetWindowText(hwnd_edit_scale, s, 16);
	wchar_t* end_wc;
	errno = 0;
	FLOAT scale = wcstod(s, &end_wc);
	if (errno != 0 || *end_wc != L'\0') {
		MessageBoxEx(hwnd, L"スケールの値が不正です", ERROR_CAPTION, 0, 0);
		return 0;
	}

	//モデルネーム取得
	std::string model_name = util::wstr2str(strItem[SendMessage(hwnd_combo_model_name , CB_GETCURSEL , 0 , 0)]);

	if(*selected_object == nullptr){
		MessageBoxEx(hwnd, L"動画オブジェクトを選択して下さい", ERROR_CAPTION, 0, 0);
		return 0;
	}

	ObjectData object_data;
	//コールバック関数内でオブジェクトデータを取得
	edit_handle->call_edit_section_param(&object_data, [](void* param, EDIT_SECTION* edit) {
		ObjectData* object_data = (ObjectData*)param;
		PCSTR alias_data = edit->get_object_alias(*selected_object);

		if(alias_data != nullptr){
			object_data->alias_data = alias_data;
			object_data->layer_frame = edit->get_object_layer_frame(*selected_object);
		}
	});

	//動画の場合
	if(object_type == OBJECT_TYPE_VIDEO){
		object_data.playback_position = get_object_item_value(object_data.alias_data, "動画ファイル", "再生位置");
		object_data.path = get_object_item_value(object_data.alias_data, "動画ファイル", "ファイル");

		std::vector<std::string> split = util::splitStr(object_data.playback_position, ',');

		if(object_data.playback_position.empty()
		|| object_data.path.empty() 
		|| object_data.alias_data.empty()
		|| split.size() < 4 ){
			MessageBoxEx(hwnd, L"オブジェクトデータの取得に失敗", ERROR_CAPTION, 0, 0);
			return 0;
		}

		//エイリアスデータからstart,endを取得
		std::string start_s = split[0];
		std::string end_s = split[1];
		FLOAT start = 0.0;
		FLOAT end = 0.0;
		try{
			start = std::stof(start_s);
			end = std::stof(end_s);
		}catch (const std::exception& e){
			MessageBoxEx(hwnd, L"start,endの取得に失敗", ERROR_CAPTION, 0, 0);
			return 0;
		}

		//マスクファイルのパスを生成
		std::string mask = util::setExt(util::decorPath(object_data.path, "_mask_"+start_s+"_"+end_s), "mp4");
		PWSTR mask_w = util::str2wstr(mask.c_str());
		int i=0;
		while(PathFileExists(mask_w)){
			mask = util::decorPath(mask, "_"+i);
			mask_w = util::str2wstr(mask.c_str());
			i++;
		}

		//Python呼び出し
		CHAR command[1024];
		sprintf(command, "\"%s\" \"%s\" \"%s\" %f %.3f %.3f %s", venv.c_str(), video_py.c_str(), object_data.path.c_str(), scale, start, end, model_name.c_str());
		if(!util::cmd(util::str2wstr(command), true)){
			MessageBoxEx(hwnd, L"実行が中断されました", ERROR_CAPTION, 0, 0);
			return 0;
		}

		//エイリアス作成
		object_data.alias_data +=u8R"(
[Object.2]
effect.name=動画マスク
動画ファイル=)"+mask+u8R"(
オフセット=)"+start_s;

	//画像の場合
	}else if(object_type == OBJECT_TYPE_IMAGE){
		object_data.path = get_object_item_value(object_data.alias_data, "画像ファイル", "ファイル");

		if(object_data.path.empty() 
		|| object_data.alias_data.empty()){
			MessageBoxEx(hwnd, L"オブジェクトデータの取得に失敗", ERROR_CAPTION, 0, 0);
			return 0;
		}
		//マスクファイルのパスを生成
		std::string mask = util::setExt(util::decorPath(object_data.path, "_mask"), "png");
		PWSTR mask_w = util::str2wstr(mask.c_str());
		int i=0;
		while(PathFileExists(mask_w)){
			mask = util::decorPath(mask, "_"+i);
			mask_w = util::str2wstr(mask.c_str());
			i++;
		}

		//Python呼び出し
		CHAR command[1024];
		sprintf(command, "\"%s\" \"%s\" \"%s\" %s", venv.c_str(), image_py.c_str(), object_data.path.c_str(), model_name.c_str());
		PWSTR tmp;
		tmp = util::str2wstr(command);
		if(!util::cmd(tmp, true)){
			MessageBoxEx(hwnd, tmp, ERROR_CAPTION, 0, 0);
			return 0;
		}

		//エイリアス作成
		object_data.alias_data +=u8R"(
[Object.2]
effect.name=動画マスク
動画ファイル=)"+mask+u8R"(
オフセット=0)";
	}else{
		MessageBoxEx(hwnd, L"不明なエラー", ERROR_CAPTION, 0, 0);
		return 0;
	}

	edit_handle->call_edit_section_param(&object_data, [](void* param, EDIT_SECTION* edit) {
		ObjectData* object_data = (ObjectData*)param;
		// 古いオブジェクトを削除
		edit->delete_object(*selected_object);
		// エイリアスデータからオブジェクトを作成
		OBJECT_HANDLE new_object = edit->create_object_from_alias(object_data->alias_data.c_str(), object_data->layer_frame.layer, object_data->layer_frame.start, 100);
		if (new_object) {
			logger->log(logger, L"create alias object");
			edit->set_focus_object(new_object);
		} else {
			logger->warn(logger, L"create alias failed");
		}
	});

	SetWindowText(
		hwnd_edit_file_path, L"（トラックバーから画像または動画オブジェクトを選択し選択ボタンを押下）"
	);

	*selected_object = nullptr;
	return 1;
}


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
						object_type = OBJECT_TYPE_VIDEO;
						if(*file_path == nullptr){
							*file_path = edit->get_object_item_value(*selected_object, L"画像ファイル", L"ファイル");
							object_type = OBJECT_TYPE_IMAGE;
						}
					});
					wchar_t* file_path_w = util::str2wstr(*file_path);
					if(*selected_object == nullptr || !PathFileExists(file_path_w)){
						MessageBoxEx(hwnd, L"オブジェクト取得失敗", ERROR_CAPTION, 0, 0);
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
					EnableWindow(hwnd_button_exec, FALSE);
					exec_do(hwnd);
					EnableWindow(hwnd_button_exec, TRUE);
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
		L"オブジェクト",
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
		L"（トラックバーから画像または動画オブジェクトを選択し選択ボタンを押下）",
		WS_VISIBLE | WS_CHILD | ES_READONLY,
		440, 10, 800, 40,
		hwnd,
		(HMENU)ID_EDIT_FILE_PATH,
		GetModuleHandle(0),
		nullptr
	);

	hwnd_edit_scale = CreateWindowEx(
		0,
		WC_EDIT,
		L"1.0",
		WS_CHILD | WS_BORDER,
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
		10, 60, 200, 40,
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
		220 , 60 , 300 , 300 ,
		hwnd,
		nullptr,
		GetModuleHandle(0),
		nullptr
	);
	for (WORD i=0;i<4;i++){
		SendMessage(hwnd_combo_model_name, CB_ADDSTRING, 0, (LPARAM)strItem[i]);
	}
	SendMessage(hwnd_combo_model_name, CB_SETCURSEL, (WPARAM)1, (LPARAM)0);

	hwnd_button_exec = CreateWindowEx(
		0,
		WC_BUTTON,
		L"実行",
		WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
		10, 110, 200, 40,
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