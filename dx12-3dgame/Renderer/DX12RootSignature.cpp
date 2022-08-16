#include "DX12RootSignature.h"

TDX12RootSignature::TDX12RootSignature(ID3D12Device* device) {
	Initialize(device);
}

TDX12RootSignature::~TDX12RootSignature() {
}

void TDX12RootSignature::Initialize(ID3D12Device* device) {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	// shaderのregisterに登録する値を伝えるのがDescriptorTable. これの設定に使うのがDescriptorRange, RangeごとにDescriptorHeapのようにでスクリプタ数を保持
	// DescriptorTableをまとめるのがRootSignature, これの設定に使うのがRootParameter

	// ルーﾄシグネチャにルートパラメーターを設定 それがディスクリプタレンジ。
	// まとめると、rangeで起点のスロットと種別を指定したものを複数root parameterのdescriptor rangeに渡すと、まとめてシェーダーに指定できる。
	D3D12_DESCRIPTOR_RANGE descTblRange[5] = {}; // 読まれただけで結び付けられなかったテクスチャはどうなるんだろう。。。 
	// テーブルに一つもヒープっを結び付けなくてもクラッシュもしないらしい。
	//Rangeという情報は、シェーダのレジスタ番号n番からx個のレジスタに、Heapのm番からのDescriptorを割り当てます、という情報です。
		//もちろんレジスタの種類が違えばRangeも違ってくるので、現在の例ではマテリアル用のDescriptorHeapに対して1つのDescriptorTableがあり、そいつがRangeを2つ持つことになります。
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
	// 上の、実は全部まとめられるんじゃないか？


	// レンジ: ヒープ上に同じ種類のでスクリプタが連続している場合、まとめて指定できる
	// シェーダーリソースと定数バッファーを同一パラメーターとして扱う。
	// ルートパラメーターを全シェーダーから参照可能にする。
	// SRVとCBVが連続しており、レンジも連続してるため←この理由がよくわからん
	// 8章で、今までは行列だけだったが、マテリアルも読み込むため、るーとぱらめーたーを増設。

	// 以下のノリでCBVは渡せるっぽいので、一度ちゃんと起動できたら試してみる。
	//root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	//root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	//root_parameters[0].Descriptor.ShaderRegister = 0;
	//root_parameters[0].Descriptor.RegisterSpace = 0;

	// command_list->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

	//cbuffer cbTansMatrix : register(b0) { // if ShaderRegister = 1 then b1.
	//	float4x4 WVP;
	//};

	// テーブルがなんのテーブルか？これはリソースをどこに配置するかのテーブルである。
	D3D12_ROOT_PARAMETER rootparam[3] = {};
	// 行列、テクスチャ用root parameter
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// CBVだけならD3D12_ROOT_PARAMETER_TYPE_CBVで奥ちゃってもよい。
	// その場合、SetGraphicsRootConstantBufferViewで
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 2;
	//rootparam[0].Descriptor.ShaderRegister = 0; // b0
	//rootparam[0].Descriptor.RegisterSpace = 0;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // 共有メモリのアクセス権限を設定している？
	// マテリアル用root parameter
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
	// ここを一体化前と混同して1にしたら、SerializeRootSignatureが失敗して終了

	// 3D textureでは奥行にwを使う。
	D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補間しない(ニアレストネイバー)
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//補間しない(ニアレストネイバー)
	// samplerDesc[0].MipLODBias = 0.0; // what is this.
	// samplerDesc[0].MaxAnisotropy = 16; // what?
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//ミップマップ最大値
	samplerDesc[0].MinLOD = 0.0f;//ミップマップ最小値
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//オーバーサンプリングの際リサンプリングしない？
	samplerDesc[0].ShaderRegister = 0;
	samplerDesc[0].RegisterSpace = 0; // s0
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].ShaderRegister = 1; // s1
	samplerDesc[2] = samplerDesc[0];
	samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // <= ならtrue(1.0), otherwise 0.0
	samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // bilinear hokan
	samplerDesc[2].MaxAnisotropy = 1; // 深度で傾斜させる
	samplerDesc[2].ShaderRegister = 2;

	rootSignatureDesc.pStaticSamplers = samplerDesc; // StaticSamplerは特に設定しなくてもs0, s1に結びつく。
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


