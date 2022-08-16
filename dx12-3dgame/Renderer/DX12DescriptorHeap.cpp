#include "DX12DescriptorHeap.h"
#include "DX12ConstantBuffer.h"
#include "DX12ShaderResource.h"

void TDX12DescriptorHeap::RegistConstantBuffer(int registerIndex, TDX12ConstantBuffer* constantBuffer) {
	++m_numConstantBuffer;
	m_constantBuffers.push_back(constantBuffer);
}
void TDX12DescriptorHeap::RegistShaderResource(int registerIndex, TDX12ShaderResource* shaderResource) {
	++m_numShaderResource;
	m_shaderResources.push_back(shaderResource);
}

void TDX12DescriptorHeap::Commit(ID3D12Device* device) {
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // こんな感じで、CBV, SRV, UAVの設定はほぼ同じなので、使いまわしてみる。同一サイズの容量を、heapに積み上げていく。
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = m_numShaderResource + m_numConstantBuffer + m_numUavResource;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_descriptorHeap.ReleaseAndGetAddressOf()));



	// ここらへんで呼ぶ それぞれの　CreateView

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	for (int i = 0; i < m_numConstantBuffer; i++) {
		if (m_constantBuffers[i] != nullptr) {
			m_constantBuffers[i]->CreateView(cpuHandle, device);
		}
		cpuHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // TODO : 定数化する

	}
	for (int i = 0; i < m_numShaderResource; i++) {
		if (m_shaderResources[i] != nullptr) {
			m_shaderResources[i]->CreateView(cpuHandle, device);
		}
		cpuHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}