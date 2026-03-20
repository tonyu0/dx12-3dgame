#pragma once

namespace ModelViewer {
	struct Vertex {
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		unsigned short boneid[2] = { 255, 255 };
		float weight[2];
	};

	struct CanvasVertex {
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
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