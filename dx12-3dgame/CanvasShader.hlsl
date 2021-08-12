Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

struct VS_OUT {
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

VS_OUT MainVS(float4 pos : POSITION, float2 uv : TEXCOORD) {
	VS_OUT output;
	output.svpos = pos;
	output.uv = uv;
	return output;
}

float4 MainPS(VS_OUT input) : SV_TARGET
{
	float4 color = tex.Sample(smp, input.uv);
	// gray scale
//	float Y = dot(color.rgb, float3(0.299, 0.587, 0.114));
//	return float4(Y, Y, Y, 1);

	// posterization
//	return float4(color.rgb - fmod(color.rgb, 0.25f), color.a);
	return color;
}