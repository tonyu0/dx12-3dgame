#include "DX12ConstantBuffer.h"

// TODO : device global

TDX12ConstantBuffer::TDX12ConstantBuffer(size_t size, ID3D12Device* device) {
	Initialize(size, device);
}
void TDX12ConstantBuffer::Initialize(size_t size, ID3D12Device* device) {
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer((size + 0xff) & ~0xff);
	// ����buffer�͂����Ŏ������}���邪�AMap����GPU��̉��z�A�h���X��DescriptorHeap�ɃR�s�[����̂ő��v�B
	device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		// 256�̔{���ɂ���Ƃ������Z�B size + (256 - size % 256) 
		// 0xff�𑫂���~0xff��&���ƁA�@�}�X�N�������Ƃŋ����I��256�̔{���ɁB0xff�𑫂����ƂŃr�b�g�̌J��オ��͍��X1
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBuffer)
	);
}

void TDX12ConstantBuffer::Map(void** toMap) {
	HRESULT result = m_constantBuffer->Map(0, nullptr, toMap);
}

// Called from Descriptor Heap
void TDX12ConstantBuffer::CreateView(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle, ID3D12Device* device) {
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (UINT)m_constantBuffer->GetDesc().Width;
	device->CreateConstantBufferView(&cbvDesc, cpuDescriptorHandle);
}