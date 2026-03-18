#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>
#include <string>
#include <vector>
#include "Application.h" // This is bad.

struct Vertex;
struct Material;

class FbxFileImporter {
public:
	FbxFileImporter() {
		for (int i = 0; i < 256; ++i) {
			boneMatrices[i] = DirectX::XMMatrixIdentity();
		}
	}
	bool CreateFbxManager(const std::string& inFbxFileName);
private:
	void LoadMesh(aiMesh* mesh);
	std::string GetExtension(const std::string& path) {
		auto idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	std::wstring GetExtension(const std::wstring& path) {
		auto idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	void UpdateBoneMatrices_internal(aiNode* pNode, const aiMatrix4x4& parentTransform);
	static aiMatrix4x4 InterpolateTransform(const aiNodeAnim* pNodeAnim, double animationTime);

	Assimp::Importer importer;
	const aiScene* scene;
	std::map<std::string, Material> raw_materials;

	// Animation Parameters
	double mAnimCurrentTicks = 0., mAnimDurationTicks = 0., mAnimTicksPerSecond = 0.;

public:
	std::map<std::string, std::vector<Vertex>> mesh_vertices; // 使うマテリアルごとに分類されたメッシュ。
	std::map<std::string, std::vector<unsigned short>> mesh_indices;
	// めっちゃ遠回りなんだけど、mesh_name -> material_name -> texture実体
	// 複数のmeshで同じテクスチャを使うことがある場合、flyweight patternを使うと良い。これはmapにfindしてあったら返す、なかったら作るみたいにすればいい.
	std::map<std::string, std::string> mesh_material_name;
	std::map<std::string, std::string> mesh_texture_name;
	std::map<std::string, unsigned int> node_bone_map;
	std::map<std::string, aiNodeAnim*> node_anim_map;

	struct BoneInfo {
		aiNode* meshNode; // aiNode: シーンの階層構造を管理する基本単位
		aiBone* cluster; // aiBone: 頂点インデックスとウェイトのペアを管理
	};
	// bone information
	BoneInfo bones[256] = {};
	aiMatrix4x4 boneOffsets[256] = {};
	DirectX::XMMATRIX boneMatrices[256] = {};
	aiMatrix4x4 globalInverseTransform;
	// Animation
	DirectX::XMMATRIX ConvertFbxMatrix(const aiMatrix4x4& src);
	void UpdateBoneMatrices(double deltaTime);
};
