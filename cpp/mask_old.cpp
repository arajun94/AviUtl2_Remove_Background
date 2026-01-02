#include <Windows.h>
#include "../lua/include/lua.hpp"

DWORD* mask_func(lua_State *L)
{
    // 画像データ、幅、高さを取得
    DWORD *base = (DWORD*)lua_touserdata(L, 1);
    DWORD *mask = (DWORD*)lua_touserdata(L, 2);
    DWORD w = (DWORD)lua_tonumber(L, 3);
    DWORD h = (DWORD)lua_tonumber(L, 4);

    lua_settop(L, 1); // スタックトップ(戻り値)の値を data にする

    // 画像データを 1 ピクセルずつ処理
    for(DWORD i = 0; i < w*h; i++) {
        // 画像データを RGBA に分解
        BYTE r =  base[i]        & 0xFF;
        BYTE g = (base[i] >>  8) & 0xFF;
        BYTE b = (base[i] >> 16) & 0xFF;
        BYTE a = (base[i] >> 24) & 0xFF;

        a = r;

        // 画像データに書き込む
        base[i] = r | (g << 8) | (b << 16) | (a << 24);
    }

    return base; // Lua 側での戻り値の個数を返す(data だけを返すので 1)
}