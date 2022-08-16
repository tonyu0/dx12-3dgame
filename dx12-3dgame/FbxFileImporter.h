#pragma once
#include <fbxsdk.h>
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
	void CreateFbxManager();
private:
	void LoadMesh(FbxMesh* mesh);
	void LoadMaterial(FbxSurfaceMaterial* material);
	void LoadBone(FbxMesh* mesh);
	void DisplayIndex(FbxMesh* mesh);
	bool SaveScene(FbxManager* pManager, FbxDocument* pScene, const char* pFilename, int pFileFormat, bool pEmbedMedia);
	std::string GetExtension(const std::string& path) {
		auto idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	std::wstring GetExtension(const std::wstring& path) {
		auto idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	std::wstring GetWideStringFromString(const std::string& str);
	FbxScene* scene;
	std::map<std::string, Material> raw_materials;

	// Animation Parameters
	FbxTime frameTime, timeCount, animStartTime, animStopTime;

public:
	std::map<std::string, std::vector<Vertex>> mesh_vertices; // 使うマテリアルごとに分類されたメッシュ。
	std::map<std::string, std::vector<unsigned short>> mesh_indices;
	// めっちゃ遠回りなんだけど、mesh_name -> material_name -> texture実体
	// 複数のmeshで同じテクスチャを使うことがある場合、flyweight patternを使うと良い。これはmapにfindしてあったら返す、なかったら作るみたいにすればいい.
	std::map<std::string, std::string> mesh_material_name;
	std::map<std::string, std::string> m_materialNameToTextureName;

	struct BoneInfo {
		FbxNode* meshNode;
		FbxCluster* cluster;
	};
	// bone information
	BoneInfo bones[256];
	DirectX::XMMATRIX boneMatrices[256];
	// Animation
	DirectX::XMMATRIX ConvertFbxMatrix(FbxAMatrix& src);
	void UpdateBoneMatrices();
};
