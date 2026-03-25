#pragma once

namespace ModelViewer {
	// NOTICE: 
	// - HLSL has an implicit rule that data should not be placed across 16-byte boundaries. 
	// - Without padding, for example, the first data will be interpreted by HLSL as pos.xyz & normal.x, and following data will be shifted.
	// Vertex: 64bytes
	struct Vertex {
		DirectX::XMFLOAT3 pos;			float pad0;
		DirectX::XMFLOAT3 normal;		float pad1;
		DirectX::XMFLOAT2 uv;
		uint32_t boneid[2];
		float weight[2];

		float pad2[2];
	};

	struct alignas(256) TransformMatrices {
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX bones[256];
	};

	struct SceneMatrices {
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		DirectX::XMMATRIX lightViewProj;
		DirectX::XMMATRIX shadow;
		DirectX::XMFLOAT3 eye;
	};

	struct CanvasVertex {
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};

	struct MeshDrawInfo {
		D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle;
		D3D12_GPU_VIRTUAL_ADDRESS cbvGpuHandle; // TODO
		UINT vertexCount;
		ID3D12Resource* pOutputVertexBuffer;
	};

	// 52 bytes
	struct Material {
		void SetAmbient(float r, float g, float b, float factor) {
			Ambient[0] = r;
			Ambient[1] = g;
			Ambient[2] = b;
			Ambient[3] = factor;
		}
		void SetDiffuse(float r, float g, float b, float factor) {
			Diffuse[0] = r;
			Diffuse[1] = g;
			Diffuse[2] = b;
			Diffuse[3] = factor;
		}
		void SetSpecular(float r, float g, float b, float factor) {
			Specular[0] = r;
			Specular[1] = g;
			Specular[2] = b;
			Specular[3] = factor;
		}
		float Ambient[4];
		float Diffuse[4];
		float Specular[4];
		float Alpha;
	};

	struct AnimState {
		int		sceneAnimCount = 0;
		int		currentAnimIdx = 0;
		float	currentAnimDuration = 0.;
		float	playingTime = 0.;
		float	playingSpeed = 1.;
		bool	isPlaying = true;
		bool	isLooping = true;
		std::vector<std::string> animationNames;
	};
};