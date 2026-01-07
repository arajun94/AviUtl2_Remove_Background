//----------------------------------------------------------------------------------
//	サンプルスクリプトモジュールプラグイン for AviUtl ExEdit2
//----------------------------------------------------------------------------------
#include <windows.h>
#include <algorithm>

#include "module2.h"
#include "filter2.h" // PIXEL_RGBA定義用

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

void mask(SCRIPT_MODULE_PARAM* param) {
	// 引数を取得
	auto n = param->get_param_num();
	if (n != 5) {
		param->set_error(u8"引数の数が正しくありません");
		return;
	}
	auto base = (PIXEL_RGBA*)param->get_param_data(0);
    auto mask = (PIXEL_RGBA*)param->get_param_data(1);
	auto w = param->get_param_int(2);
	auto h = param->get_param_int(3);
	auto invert = param->get_param_boolean(4);
	if (!base || !mask || w <= 0 || h <= 0) {
		param->set_error(u8"引数の値が正しくありません");
		return;
	}

    //マスク
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			base->a = invert? ~mask->r : mask->r;
			base++;
            mask++;
		}
	}
}

//---------------------------------------------------------------------
//	スクリプトモジュール関数リスト定義
//---------------------------------------------------------------------
SCRIPT_MODULE_FUNCTION functions[] = {
    { L"mask", mask },
	{ nullptr }
};

//---------------------------------------------------------------------
//	スクリプトモジュール構造体定義
//---------------------------------------------------------------------
SCRIPT_MODULE_TABLE script_module_table = {
	L"動画マスク",	// モジュールの情報
	functions
};

//---------------------------------------------------------------------
//	スクリプトモジュール構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) SCRIPT_MODULE_TABLE* GetScriptModuleTable(void) {
	return &script_module_table;
}
