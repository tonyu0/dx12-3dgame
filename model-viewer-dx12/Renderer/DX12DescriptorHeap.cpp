#include "DX12DescriptorHeap.h"

static constexpr int NUM_DESCRIPTORS = 128;

void TDX12DescriptorHeap::AddConstantBuffer(ID3D12Device* pDev, TDX12ConstantBuffer* constantBuffer) {
	if (constantBuffer == nullptr) {
		std::cout << "Given Constant Buffer was nullptr!" << std::endl;
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	cpuHandle.ptr = mHeapStartCPU.ptr + mHeapHandleIncSize * mNumResources;
	constantBuffer->CreateView(cpuHandle, pDev);

	++mNumResources;
}
void TDX12DescriptorHeap::AddShaderResource(ID3D12Device* pDev, TDX12ShaderResource* shaderResource) {
	if (shaderResource == nullptr) {
		std::cout << "Given Shader Resource was nullptr!" << std::endl;
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	cpuHandle.ptr = mHeapStartCPU.ptr + mHeapHandleIncSize * mNumResources;
	shaderResource->CreateView(cpuHandle, pDev);

	++mNumResources;
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
	
	mHeapStartCPU = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	mHeapStartGPU = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	mHeapHandleIncSize = pDev->GetDescriptorHandleIncrementSize(descHeapDesc.Type);
}
