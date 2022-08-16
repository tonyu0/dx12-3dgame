#pragma once
#include "DX12RendererCommon.h"

class TDX12RootSignature {
public:
	TDX12RootSignature() = default;
	TDX12RootSignature(ID3D12Device* device);
	~TDX12RootSignature();
	ID3D12RootSignature* GetRootSignaturePointer() { return m_rootSignature.Get(); }
	void Initialize(ID3D12Device* device);
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_rootSignature;
};