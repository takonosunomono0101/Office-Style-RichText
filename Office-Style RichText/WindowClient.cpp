
#include <windows.h>
#include "plugin2.h"

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"Office-Style RichText",
    L"Office-Style RichText version 0.1"
};

EXTERN_C __declspec(dllexport) DWORD RequiredVersion() {
    return 2003300;
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {}

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    // 読み込み確認用。まだ何も登録しない。
}
