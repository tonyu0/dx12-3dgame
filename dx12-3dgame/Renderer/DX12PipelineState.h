#pragma once
#include "DX12RendererCommon.h"
#include "DX12RootSignature.h"
#include "Shader.h"

class TDX12PipelineState {
public:
	TDX12PipelineState() = default;
	TDX12PipelineState(TDX12RootSignature rootSignature, TShader vertexShader, TShader pixelShader);
	~TDX12PipelineState();
	void Initialize(TDX12RootSignature rootSignature, TShader vertexShader, TShader pixelShader);
private:
};