struct VS_OUT {
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	float3 view : VIEW;
	uint instanceID : SV_InstanceID;
	float4 tpos : TPOS; // �擾�����e�N�X�`���ƁA���C�g���猩���[�x���r���邽�߁A���C�g�r���[�ō��W�ϊ��������
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> materialTex : register(t1);
Texture2D<float4> depthTex : register(t2);
SamplerState smp : register(s0);
SamplerState materialSampler : register(s1);
SamplerComparisonState shadowSmp : register(s2);

cbuffer cbuff0 : register(b0) {
	matrix world;
	matrix bones[256];
};
cbuffer cbuff1 : register(b1) {
	matrix view;
	matrix proj;
	matrix lightViewProj;
	matrix shadow;
	float3 eye; // eye position: view matrix�����ꂻ���B
};

cbuffer Material : register(b1) {
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float alpha;
};

float4 ShadowVS(in float4 pos: POSITION, in float4 normal : NORMAL, in float2 uv : TEXCOORD, in min16uint2 boneid : BONEID, in float2 weight : WEIGHT) : SV_POSITION{
	if (boneid[0] != 255) {
		matrix bonemat = (bones[boneid[0]] * weight[0] + bones[boneid[1]] * weight[1]) / (weight[0] + weight[1]);
		pos = mul(bonemat, pos);
	}
pos = mul(world, pos);
return mul(lightViewProj, pos);
}

VS_OUT MainVS(in float4 pos : POSITION, in float3 normal : NORMAL, in float2 uv : TEXCOORD, in min16uint2 boneid : BONEID, in float2 weight : WEIGHT, in uint instanceID : SV_InstanceID)
{
	VS_OUT output;
	if (boneid[0] != 255) {
		matrix bonemat = (bones[boneid[0]] * weight[0] + bones[boneid[1]] * weight[1]) / (weight[0] + weight[1]);
		pos = mul(bonemat, pos);
	}
	pos = mul(world, pos);
	if (instanceID == 1) {
		pos = mul(shadow, pos);
	}
	output.svpos = mul(proj, mul(view, pos));
	output.normal = mul(world, float4(normal, 0.0));
	output.uv = uv;
	output.view = normalize(pos.xyz - eye);
	output.instanceID = instanceID;
	output.tpos = mul(lightViewProj, pos); // ���̂��ƁAPS�Ŕ�r���s��
	return output;
}

float4 MainPS(in VS_OUT input) : SV_TARGET
{
	if (input.instanceID == 1) {
		return float4(0, 0, 0, 1.0);
	}
float3 L = normalize(float3(-1, 1, -1));
float3 N = normalize(input.normal).xyz;
float3 R = normalize(reflect(L, input.normal.xyz));
float Specular = pow(saturate(dot(R, -input.view)), 20);
float NdotL = dot(N, L);
float4 texColor = materialTex.Sample(materialSampler, input.uv);


// tposxyz / tpos.w
float3 posFromLightVP = input.tpos.xyz / input.tpos.w; // ���e�s��́Aw�Ŋ��邱�Ƃɂ��[-1,1],[-1,1],[0,1]�ɐ��K�������d�l, SV_POSITION�̏ꍇ�̓��X�^���C�U����������
float2 shadowUV = (posFromLightVP + float2(1, -1)) * float2(0.5, -0.5);
//float depthFromLight = depthTex.Sample(smp, shadowUV);
//float shadowWeight = 1.0f;
//if (depthFromLight < posFromLightVP.z - 0.001f) { // bias �l���������ƂŁA�v�Z�덷�͈̔͊O�A�V���h�E���v����h��
//	shadowWeight = 0.5f;
//}
float depthFromLight = depthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);
float shadowWeight = lerp(0.5f, 1.0f, depthFromLight);

// 	return float4(NdotL, NdotL, NdotL, 1) * texColor + specular * Specular + ambient * texColor; 
// �����Ȃ�͂������A���󃁃b�V�����Ƃ̃}�e���A�����o�C���h���ĂȂ� ->
	// D3D12 ERROR: ID3D12Device::CreateGraphicsPipelineState: Root Signature doesn't match Pixel Shader: Shader CBV descriptor range (BaseShaderRegister=1, NumDescriptors=1, RegisterSpace=0) is not fully bound in root signature
return (float4(NdotL, NdotL, NdotL, 1) * texColor + Specular) * shadowWeight;
}