#pragma once
#include "../Common.h"
// bufferŒn‚Íinterface‚ð“¯‚¶‚É‚µ‚½‚¢
class TDX12ConstantBuffer {
public:
	TDX12ConstantBuffer() = default;
	TDX12ConstantBuffer(size_t size, ID3D12Device* device);
	void Initialize(size_t size, ID3D12Device* device);
	bool IsValid() { return m_constantBuffer != nullptr; }
	void Map(void** toMap);
	ID3D12Resource* m_constantBuffer = nullptr;
};