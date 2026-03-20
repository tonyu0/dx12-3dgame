#pragma once

// STL
#include <chrono>
#include <vector>
#include <map>
//#ifdef _DEBUG
#include <iostream>
//#endif

// Renderer
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <wrl.h>

// ImGui
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "DirectXTex.lib")
// pragma comment: オブジェクトファイルにコメントを残す。これはリンカーにより読まれる
