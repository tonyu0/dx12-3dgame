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
		_T("DX12テスト"),//タイトルバーの文字
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

// LRESULT? HWND?
// ウィンドウが出続けてる間マイフレーム呼ばれる？
// Window生成時に渡すコールバック関数
LRESULT TWindowManager::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ウィンドウ破棄時
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理
}
