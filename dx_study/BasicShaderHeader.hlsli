struct Output {
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

Texture2D<float4> tex : register(t0); // 0番スロットに設定されたテクスチャ
Texture2D<float4> materialTex : register(t1);
SamplerState smp : register(s0); // 0番スロットに設定されたサンプラー
SamplerState materialSampler : register(s1);
// サンプラーを複数設定しないといけない？


cbuffer cbuff0 : register(b0) {
	matrix world;
	matrix viewproj;
};


// 定数バッファーマテリアル用
cbuffer Material : register(b1) {
	float4 ambient;
	float4 diffuse;
	float4 specular;
	float alpha;
};
