struct Output {
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

Texture2D<float4> tex : register(t0); // 0�ԃX���b�g�ɐݒ肳�ꂽ�e�N�X�`��
Texture2D<float4> materialTex : register(t1);
SamplerState smp : register(s0); // 0�ԃX���b�g�ɐݒ肳�ꂽ�T���v���[
SamplerState materialSampler : register(s1);
// �T���v���[�𕡐��ݒ肵�Ȃ��Ƃ����Ȃ��H


cbuffer cbuff0 : register(b0) {
	matrix world;
	matrix viewproj;
};


// �萔�o�b�t�@�[�}�e���A���p
cbuffer Material : register(b1) {
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float alpha;
};
