#include "BasicShaderHeader.hlsli"
float4 main(Output input) : SV_TARGET
{
	// return float4(tex.Sample(smp, input.uv));
	float3 L = normalize(float3(-1, 1, -1));
	float3 N = normalize(input.normal);
	float NdotL = dot(N, L);


	return float4(NdotL, NdotL, NdotL, 1) * materialTex.Sample(materialSampler, input.uv);
	// return float4(input.normal.xyz, 1.0);
}