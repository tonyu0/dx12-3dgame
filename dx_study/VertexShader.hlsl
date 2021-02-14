#include "BasicShaderHeader.hlsli"

Output main(float3 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
	Output output;
	output.svpos = mul(mul(viewproj, world), float4(pos, 1.0));
	output.normal = mul(world, float4(normal, 0.0));
	output.uv = uv;
	return output;
}