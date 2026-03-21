#include "DX12DescriptorHeap.h"

static constexpr int NUM_DESCRIPTORS = 128; 
// 0~63:	static locations for Descriptor Table (resources need to be continuous), the resource and its Descriptor Heap are stored in the order they are added.
// 64~127:	dynamic locations for ImGui or single use of SRV or like that. Managed by the pooling way

/**
* @brief Make sure you add the resources in the correct order
*/
void TDX12DescriptorHeap::AddConstantBuffer(ID3D12Device* pDev, ID3D12Resource* constantBuffer) {
	if (constantBuffer == nullptr) {
		std::cout << "Given Constant Buffer was nullptr!" << std::endl;
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	cpuHandle.ptr = m_heapStartCPU.ptr + m_heapHandleIncSize * numResources;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)constantBuffer->GetDesc().Width;
	pDev->CreateConstantBufferView(&cbvDesc, cpuHandle);

	++numResources;
}
/**
* @brief Make sure you add the resources in the correct order
*/
void TDX12DescriptorHeap::AddShaderResource(ID3D12Device* pDev, ID3D12Resource* shaderResource, DXGI_FORMAT shaderResourceFormat) {
	if (shaderResource == nullptr) {
		std::cout << "Given Shader Resource was nullptr!" << std::endl;
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	cpuHandle.ptr = m_heapStartCPU.ptr + m_heapHandleIncSize * numResources;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = shaderResourceFormat;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f～1.0fに正規化)
	// TODO : ↑テクスチャ読めてない場合など, FormatがUnknownとかだとエラーになりデバイスが落ちる(Resrouce(実際のメモリ上の生データ)とView(メモリ上のデータをどう解釈するか))
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 画像データのRGBSの情報がそのまま捨て宇されたフォーマットに、データ通りの順序で割り当てられているか
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	pDev->CreateShaderResourceView(shaderResource, &srvDesc, cpuHandle);

	++numResources;
}

void TDX12DescriptorHeap::AllocDynamic(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuDescHandle) {
	if (m_freeIndices.size() == 0) {
		std::cout << "The number of dynamic Descriptor Heap is full!" << std::endl;
		return;
	}
	int idx = m_freeIndices.back();
	m_freeIndices.pop_back();
	cpuDescHandle->ptr = m_heapStartCPU.ptr + idx * m_heapHandleIncSize;
	gpuDescHandle->ptr = m_heapStartGPU.ptr + idx * m_heapHandleIncSize;
}

void TDX12DescriptorHeap::FreeDynamic(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle) {
	unsigned int cpuIdx = (cpuDescHandle.ptr - m_heapStartCPU.ptr) / m_heapHandleIncSize;
	unsigned int gpuIdx = (gpuDescHandle.ptr - m_heapStartGPU.ptr) / m_heapHandleIncSize;
	if (cpuIdx != gpuIdx) {
		std::cout << "Failed to free dynamic Descriptor Heap because the given data location of CPU and GPU are different!" << std::endl;
		return;
	}
	m_freeIndices.push_back(cpuIdx);
}

TDX12DescriptorHeap::TDX12DescriptorHeap(ID3D12Device* pDev) {
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = NUM_DESCRIPTORS;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = pDev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_descriptorHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		std::cerr << "Failed to creat Descriptor Heap!" << std::endl;
	}
	
	m_heapStartCPU = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_heapStartGPU = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_heapHandleIncSize = pDev->GetDescriptorHandleIncrementSize(descHeapDesc.Type);

	for (unsigned int i = 64; i < NUM_DESCRIPTORS; ++i) {
		m_freeIndices.push_back(i);
	}
}
