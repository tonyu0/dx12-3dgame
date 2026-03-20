#include "WindowManager.h"
#include <tchar.h>

TWindowManager::TWindowManager(unsigned int windowWidth, unsigned int windowHeight) : windowWidth(windowWidth), windowHeight(windowHeight)
{
	Initialize();
}
TWindowManager::~TWindowManager() {
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

void TWindowManager::Initialize() {
	windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;//コールバック関数の指定
	windowClass.lpszClassName = _T("DirectXTest");//アプリケーションクラス名
	windowClass.hInstance = GetModuleHandle(0);//ハンドルの取得
	RegisterClassEx(&windowClass);//アプリケーションクラス(こういうの作るからよろしくってOSに予告する)
	RECT windowRect = { 0,0, static_cast<LONG>(windowWidth), static_cast<LONG>(windowHeight) };

	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);//ウィンドウのサイズはちょっと面倒なので関数を使って補正する
//ウィンドウオブジェクトの生成
	windowHandle = CreateWindow(windowClass.lpszClassName,//クラス名指定
		_T("Model Viewer DX12"),//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,//タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,//表示X座標はOSにお任せします
		CW_USEDEFAULT,//表示Y座標はOSにお任せします
		windowRect.right - windowRect.left,//ウィンドウ幅
		windowRect.bottom - windowRect.top,//ウィンドウ高
		nullptr,//親ウィンドウハンドル
		nullptr,//メニューハンドル
		windowClass.hInstance,//呼び出しアプリケーションハンドル
		nullptr);//追加パラメータ
}

// Code from: https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp
// Forward declare message handler from imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT TWindowManager::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) { // HWND: Handle to Window, LRESULT: Long Result
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) // Calling this function enables actions like a clicking event on ImGui
		return true;

	// ウィンドウ破棄時
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理
}
