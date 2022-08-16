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
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {}; // ����Ȋ����ŁACBV, SRV, UAV�̐ݒ�͂قړ����Ȃ̂ŁA�g���܂킵�Ă݂�B����T�C�Y�̗e�ʂ��Aheap�ɐςݏグ�Ă����B
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//�V�F�[�_���猩����悤��
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = m_numShaderResource + m_numConstantBuffer + m_numUavResource;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT result = device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_descriptorHeap.ReleaseAndGetAddressOf()));



	// ������ւ�ŌĂ� ���ꂼ��́@CreateView

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	for (int i = 0; i < m_numConstantBuffer; i++) {
		if (m_constantBuffers[i] != nullptr) {
			m_constantBuffers[i]->CreateView(cpuHandle, device);
		}
		cpuHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // TODO : �萔������

	}
	for (int i = 0; i < m_numShaderResource; i++) {
		if (m_shaderResources[i] != nullptr) {
			m_shaderResources[i]->CreateView(cpuHandle, device);
		}
		cpuHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}