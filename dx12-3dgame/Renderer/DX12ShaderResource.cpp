#include "DX12ShaderResource.h"

TDX12ShaderResource::TDX12ShaderResource(const std::string& textureFileName, ID3D12Device* device) {
	Initialize(textureFileName, device);
}

// �V�F�[�_�[���\�[�X�r���[�ƃ}�e���A���̃r���[�͓����f�B�X�N���v�^�q�[�v�ɒu���B
// �}�e���A���ƃe�N�����Ⴛ�ꂼ��̃��[�g�p�����[�^�[���܂Ƃ߂�B�f�B�X�N���v�^�����W�͕�����B
// 1. �e�N�X�`�����[�h�A�@2. ���\�[�X�̍쐬�A 3. �f�[�^�R�s�[
void TDX12ShaderResource::Initialize(const std::string& textureFileName, ID3D12Device* device) {
	// TODO : �ǂݍ��ݍς݃��\�[�X�̃��[�h���X�L�b�v���鏈��(���O���[�o���Ȉʒu��map�ŊǗ�?)

	DirectX::ScratchImage scratchImg = {};
	std::wstring wtexpath = GetWideStringFromString(textureFileName);//�e�N�X�`���̃t�@�C���p�X
	std::string ext = GetExtension(textureFileName);//�g���q���擾
	HRESULT result;
	if (ext == "tga") {
		result = LoadFromTGAFile(wtexpath.c_str(), &m_textureMetadata, scratchImg);
	}
	else {
		result = LoadFromWICFile(wtexpath.c_str(), DirectX::WIC_FLAGS_NONE, &m_textureMetadata, scratchImg);
	}

	if (FAILED(result)) {
		return; // TODO : �Ȃɂ��G���[����
	}
	auto img = scratchImg.GetImage(0, 0, 0);//���f�[�^���o

	//WriteToSubresource�œ]������p�̃q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//����Ȑݒ�Ȃ̂�default�ł�upload�ł��Ȃ�
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//���C�g�o�b�N��
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//�]����L0�܂�CPU�����璼��
	texHeapProp.CreationNodeMask = 0;//�P��A�_�v�^�̂���0
	texHeapProp.VisibleNodeMask = 0;//�P��A�_�v�^�̂���0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = m_textureMetadata.format;
	resDesc.Width = static_cast<UINT>(m_textureMetadata.width);//��
	resDesc.Height = static_cast<UINT>(m_textureMetadata.height);//����
	resDesc.DepthOrArraySize = static_cast<UINT16>(m_textureMetadata.arraySize);
	resDesc.SampleDesc.Count = 1;//�ʏ�e�N�X�`���Ȃ̂ŃA���`�F�����Ȃ�
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<UINT16>(m_textureMetadata.mipLevels);//�~�b�v�}�b�v���Ȃ��̂Ń~�b�v���͂P��
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_textureMetadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//���C�A�E�g�ɂ��Ă͌��肵�Ȃ�
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//�Ƃ��Ƀt���O�Ȃ�

	result = device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//���Ɏw��Ȃ�
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_shaderResource)
	);

	if (FAILED(result)) {
		return; // TODO : �����G���[����
	}
	result = m_shaderResource->WriteToSubresource(0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		static_cast<UINT>(img->rowPitch),//1���C���T�C�Y
		static_cast<UINT>(img->slicePitch)//�S�T�C�Y
	);
	if (FAILED(result)) {
		return; // TODO : �����G���[����
	}

}

void TDX12ShaderResource::CreateView(D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle, ID3D12Device* device) {
	//�ʏ�e�N�X�`���r���[�쐬
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	//srvDesc.Format = m_textureMetadata.format;//DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//RGBA(0.0f�`1.0f�ɐ��K��)
	// TODO : ���e�N�X�`���ǂ߂ĂȂ��ꍇ�Ȃ�, Format��Unknown�Ƃ����ƃG���[�ɂȂ�̂ŋ����I�ɂ���A��O�����ȂǍs��
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// �摜�f�[�^��RGBS�̏�񂪂��̂܂܎̂ĉF���ꂽ�t�H�[�}�b�g�ɁA�f�[�^�ʂ�̏����Ŋ��蓖�Ă��Ă��邩
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1
	device->CreateShaderResourceView(m_shaderResource, &srvDesc, descriptorHandle);
}