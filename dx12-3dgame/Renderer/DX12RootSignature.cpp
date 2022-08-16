#include "DX12RootSignature.h"

TDX12RootSignature::TDX12RootSignature(ID3D12Device* device) {
	Initialize(device);
}

TDX12RootSignature::~TDX12RootSignature() {
}

void TDX12RootSignature::Initialize(ID3D12Device* device) {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	// shader��register�ɓo�^����l��`����̂�DescriptorTable. ����̐ݒ�Ɏg���̂�DescriptorRange, Range���Ƃ�DescriptorHeap�̂悤�ɂŃX�N���v�^����ێ�
	// DescriptorTable���܂Ƃ߂�̂�RootSignature, ����̐ݒ�Ɏg���̂�RootParameter

	// ���[ăV�O�l�`���Ƀ��[�g�p�����[�^�[��ݒ� ���ꂪ�f�B�X�N���v�^�����W�B
	// �܂Ƃ߂�ƁArange�ŋN�_�̃X���b�g�Ǝ�ʂ��w�肵�����̂𕡐�root parameter��descriptor range�ɓn���ƁA�܂Ƃ߂ăV�F�[�_�[�Ɏw��ł���B
	D3D12_DESCRIPTOR_RANGE descTblRange[5] = {}; // �ǂ܂ꂽ�����Ō��ѕt�����Ȃ������e�N�X�`���͂ǂ��Ȃ�񂾂낤�B�B�B 
	// �e�[�u���Ɉ���q�[�v�������ѕt���Ȃ��Ă��N���b�V�������Ȃ��炵���B
	//Range�Ƃ������́A�V�F�[�_�̃��W�X�^�ԍ�n�Ԃ���x�̃��W�X�^�ɁAHeap��m�Ԃ����Descriptor�����蓖�Ă܂��A�Ƃ������ł��B
		//������񃌃W�X�^�̎�ނ��Ⴆ��Range������Ă���̂ŁA���݂̗�ł̓}�e���A���p��DescriptorHeap�ɑ΂���1��DescriptorTable������A������Range��2�����ƂɂȂ�܂��B
	descTblRange[0].NumDescriptors = 2;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[0].BaseShaderRegister = 0; // b0 ~ b1
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[1].BaseShaderRegister = 0; // t0
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// Material
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[2].BaseShaderRegister = 1; // t1
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// shadow map
	descTblRange[3].NumDescriptors = 1;
	descTblRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[3].BaseShaderRegister = 2; // t2
	// ��́A���͑S���܂Ƃ߂���񂶂�Ȃ����H


	// �����W: �q�[�v��ɓ�����ނ̂ŃX�N���v�^���A�����Ă���ꍇ�A�܂Ƃ߂Ďw��ł���
	// �V�F�[�_�[���\�[�X�ƒ萔�o�b�t�@�[�𓯈�p�����[�^�[�Ƃ��Ĉ����B
	// ���[�g�p�����[�^�[��S�V�F�[�_�[����Q�Ɖ\�ɂ���B
	// SRV��CBV���A�����Ă���A�����W���A�����Ă邽�߁����̗��R���悭�킩���
	// 8�͂ŁA���܂ł͍s�񂾂����������A�}�e���A�����ǂݍ��ނ��߁A��[�Ƃς�߁[���[�𑝐݁B

	// �ȉ��̃m����CBV�͓n������ۂ��̂ŁA��x�����ƋN���ł����玎���Ă݂�B
	//root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//root_parameters[0].Descriptor.ShaderRegister = 0;
	//root_parameters[0].Descriptor.RegisterSpace = 0;

	// command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//cbuffer cbTansMatrix : register(b0) { // if ShaderRegister = 1 then b1.
	//	float4x4 WVP;
	//};

	// �e�[�u�����Ȃ�̃e�[�u�����H����̓��\�[�X���ǂ��ɔz�u���邩�̃e�[�u���ł���B
	D3D12_ROOT_PARAMETER rootparam[3] = {};
	// �s��A�e�N�X�`���proot parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// CBV�����Ȃ�D3D12_ROOT_PARAMETER_TYPE_CBV�ŉ�������Ă��悢�B
	// ���̏ꍇ�ASetGraphicsRootConstantBufferView��
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;
	//rootparam[0].Descriptor.ShaderRegister = 0; // b0
	//rootparam[0].Descriptor.RegisterSpace = 0;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // ���L�������̃A�N�Z�X������ݒ肵�Ă���H
	// �}�e���A���proot parameter
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[2];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// shadow map
	rootparam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[2].DescriptorTable.pDescriptorRanges = &descTblRange[3];
	rootparam[2].DescriptorTable.NumDescriptorRanges = 1;

	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 3;
	// ��������̉��O�ƍ�������1�ɂ�����ASerializeRootSignature�����s���ďI��

	// 3D texture�ł͉��s��w���g���B
	D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
	// samplerDesc[0].MipLODBias = 0.0; // what is this.
	// samplerDesc[0].MaxAnisotropy = 16; // what?
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
	samplerDesc[0].MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].RegisterSpace = 0; // s0
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1; // s1
	samplerDesc[2] = samplerDesc[0];
	samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // <= �Ȃ�true(1.0), otherwise 0.0
	samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // bilinear hokan
	samplerDesc[2].MaxAnisotropy = 1; // �[�x�ŌX�΂�����
	samplerDesc[2].ShaderRegister = 2;

	rootSignatureDesc.pStaticSamplers = samplerDesc; // StaticSampler�͓��ɐݒ肵�Ȃ��Ă�s0, s1�Ɍ��т��B
	rootSignatureDesc.NumStaticSamplers = 3;

	ID3DBlob* rootSigBlob = nullptr;
	// Selialize Root Signature?
	ID3DBlob* errorBlob = nullptr;
	D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	//std::cout << rootSigBlob->GetBufferSize() << std::endl;
	device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	rootSigBlob->Release();

	// create root signature
	//D3D12_DESCRIPTOR_RANGE range = {};
	//range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // texture, register using texture like this (such as t0)
	//range.BaseShaderRegister = 0; // t0
	//range.NumDescriptors = 1;
	//D3D12_ROOT_PARAMETER rootParameter = {};
	//rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//rootParameter.DescriptorTable.NumDescriptorRanges = 1;
	//rootParameter.DescriptorTable.pDescriptorRanges = &range;
	//D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0); // s0
	//rsDesc.NumParameters = 1;
	//rsDesc.pParameters = &rootParameter;
	//rsDesc.NumStaticSamplers = 1;
	//rsDesc.pStaticSamplers = &sampler;
}


