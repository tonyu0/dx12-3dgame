#pragma once
#include "../Common.h"
#include "DX12ConstantBuffer.h"
#include "DX12ShaderResource.h"

// bufferånÇÕinterfaceÇìØÇ∂Ç…ÇµÇΩÇ¢
class TDX12DescriptorHeap {
public:
	TDX12DescriptorHeap() = delete;
	explicit TDX12DescriptorHeap(ID3D12Device* pDev);
	D3D12_GPU_DESCRIPTOR_HANDLE AddCBV(ID3D12Device* pDev, ID3D12Resource* pBuffer);
	D3D12_GPU_DESCRIPTOR_HANDLE AddSRV(ID3D12Device* pDev, ID3D12Resource* pBuffer, DXGI_FORMAT shaderResourceFormat);
	void AllocDynamic(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuDescHandle);
	void FreeDynamic(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle);

	ID3D12DescriptorHeap* Get() {
		return m_descriptorHeap.Get();
	}

	ID3D12DescriptorHeap* const* GetAddressOf() {
		return m_descriptorHeap.GetAddressOf(); // ñ≥óùÇ‚ÇËÇ¬Ç»Ç∞ÇÈÇΩÇﬂÅAçÌèúó\íË
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart() {
		return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(); // ìØè„
	}
	size_t numResources = 0;

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGPU;
	std::vector<size_t> m_freeIndices;
	size_t m_heapHandleIncSize = 0;
};