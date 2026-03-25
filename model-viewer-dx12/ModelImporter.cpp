#include "ModelImporter.h"

// ControlPoints = 頂点バッファ、 PolugonVertexCount = 頂点座標？
void ModelImporter::LoadMesh(aiMesh* mesh) {
	std::string mesh_name = mesh->mName.C_Str();
	std::cout << "Load " << mesh_name << " Data" << std::endl;
	{ // 1. 頂点インデックスの読み込み
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			aiFace& face = mesh->mFaces[f];
			// fbxは右手系なのでdxではポリゴン生成を逆に
			mesh_indices[mesh_name].push_back(face.mIndices[2]);
			mesh_indices[mesh_name].push_back(face.mIndices[1]);
			mesh_indices[mesh_name].push_back(face.mIndices[0]);
		}
	}

	{ // 2. 頂点情報の読み込み: pos, normal, uv
		std::vector<ModelViewer::Vertex> vertices;
		for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
			ModelViewer::Vertex vert{};
			auto p = mesh->mVertices[v];
			vert.pos = { p.x, p.y, p.z };

			auto n = mesh->mNormals[v];
			vert.normal = { n.x, n.y, n.z };

			if (mesh->HasTextureCoords(0)) {
				auto uv = mesh->mTextureCoords[0][v];
				vert.uv = { uv.x, uv.y };
			}
			vertices.push_back(vert);
		}
		mesh_vertices[mesh_name] = vertices;
	}

	{ // 3. 頂点情報の読み込み: weight, boneid
		auto AddBoneInfo = [](ModelViewer::Vertex& v, int boneid, float weight) {
			if (v.weight[0] < weight) {
				v.weight[1] = v.weight[0];
				v.boneid[1] = v.boneid[0];
				v.weight[0] = weight;
				v.boneid[0] = boneid;
			}
			else if (v.weight[1] < weight) {
				v.weight[1] = weight;
				v.boneid[1] = boneid;
			}
			};

		// boneid, weight
		for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string boneName = bone->mName.C_Str();
			node_bone_map[bone->mName.C_Str()] = boneIndex;
			boneOffsets[boneIndex] = bone->mOffsetMatrix;

			std::cout << "Mesh name: " << mesh->mName.C_Str() << ", " << boneIndex << " th Bone name: " << boneName << std::endl;
			for (unsigned int wid = 0; wid < bone->mNumWeights; ++wid) {
				aiVertexWeight weight = bone->mWeights[wid];
				int vertexId = weight.mVertexId;
				AddBoneInfo(mesh_vertices[mesh_name][vertexId], boneIndex, weight.mWeight);
			}
		}
	}

	// meshとマテリアルの結び付け。メッシュ単位で描画するので、そこからマテリアルが取れればさらにそこからテクスチャが取れて、無事UVマッピング。
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::string materialName = material->GetName().C_Str();
	aiString texPath;
	mesh_material_name[mesh_name] = materialName;
	// Check all the texture type
	for (int i = aiTextureType::aiTextureType_NONE; i < aiTextureType_GLTF_METALLIC_ROUGHNESS; ++i) {
		if (material->GetTextureCount((aiTextureType)i) > 0) {
			material->GetTexture((aiTextureType)i, 0, &texPath);
			std::cout <<"Texture type: " <<  material->GetTextureCount((aiTextureType)i) << ", Texture name: " << texPath.C_Str() << std::endl;
			mesh_texture_name[mesh_name] = texPath.C_Str();
		}
	}
}

bool ModelImporter::CreateModelImporter(const std::string& inFbxFileName) {
	// ボーン更新時等にscene->mRootNoteが必要になるのでsceneが破棄されないようにimporterをメンバにしている
	scene = importer.ReadFile(inFbxFileName, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	if (!scene) {
		std::cout << importer.GetErrorString() << std::endl;
		return false;
	}

	{// Animation Preparation
		// 上方向がYかZかなど、Assimpはモデリングソフトの差異をmRootNode->mTransformationに変換として集約するが、実際の座標計算時には、その変換を逆行列で除去する
		globalInverseTransform = scene->mRootNode->mTransformation.Inverse();
		// 
		// AnimStack（アニメーション情報）はシーンのNodeの木構造の外側に独立して存在している。
		// 各AnimStackには、「どのNode（手とか足とか）を動かすか」という情報があり、これでモデルとの関連付けを行っている。
		// pAnim->mChannels(aiNodeAnim**)にはT, R, Sすべてのキーフレーム情報が入っている。FBX内部ではそれらはばらばらだがAssimpがいい感じに集めてくれる。
		// (各aiNodeAnimはmPositionKeys, mRotationKeys, mScalingKeysがあり、ここから変換行列を作る)
		// 各aiNodeがどのaiNodeAnimに影響を受けるかをstd::map<string, aiNodeAnim*>に保管しておき、後でscene->mRootNodeから再帰的にノードを走査して、各フレームでの変換行列を構築する必要がある
		if (scene->mNumAnimations > 0) {
			SetCurrentAnimation(0);
		}
	}

	int meshCount = scene->mNumMeshes;
	for (int i = 0; i < meshCount; ++i) {
		aiMesh* mesh = scene->mMeshes[i];
		std::cout << i << " th mesh name is " << mesh->mName.C_Str() << std::endl;
		mesh_names.push_back(mesh->mName.C_Str());
		LoadMesh(mesh);
	}

	return true;
}

ModelViewer::AnimState ModelImporter::GetDefaultAnimState() {
	ModelViewer::AnimState animState;

	animState.sceneAnimCount = scene->mNumAnimations;
	if (animState.sceneAnimCount > 0) {
		animState.currentAnimIdx = 0;
		animState.currentAnimDuration = (float)scene->mAnimations[0]->mDuration / mAnimTicksPerSecond;
		for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
			animState.animationNames.push_back(scene->mAnimations[i]->mName.C_Str());
		}
	}
	animState.isLooping = true;
	animState.isPlaying = true;
	animState.playingTime = 0.f;
	animState.playingSpeed = 1.f;

	return animState;
}

void ModelImporter::SetCurrentAnimation(UINT currentAnimIdx) {
	if (currentAnimIdx >= scene->mNumAnimations) {
		std::cout << "Invalid animation index specified!" << std::endl;
		return;
	}

	node_anim_map.clear();
	for (unsigned int j = 0; j < scene->mAnimations[currentAnimIdx]->mNumChannels; ++j) {
		aiNodeAnim* node_anim = scene->mAnimations[currentAnimIdx]->mChannels[j];
		node_anim_map[node_anim->mNodeName.C_Str()] = node_anim;
	}

	mCurrentAnimIdx = currentAnimIdx;
	mAnimDurationTicks = (float)scene->mAnimations[currentAnimIdx]->mDuration;
	mAnimTicksPerSecond = scene->mAnimations[currentAnimIdx]->mTicksPerSecond != 0 ? (float)scene->mAnimations[currentAnimIdx]->mTicksPerSecond : 25.f;
	mAnimCurrentTicks = 0.;
}

void ModelImporter::UpdateBoneMatrices(float deltaTime, ModelViewer::AnimState &animState) {
	if (animState.isPlaying) {
		mAnimCurrentTicks = mAnimCurrentTicks + mAnimTicksPerSecond * deltaTime * animState.playingSpeed;
	}

	if (animState.isLooping) {
		mAnimCurrentTicks = fmod(mAnimCurrentTicks, mAnimDurationTicks);
	}
	else {
		mAnimCurrentTicks = std::fmin(mAnimCurrentTicks, mAnimDurationTicks - 0.01f); // Setting exact value leads to broken model due to decimal error, substract some small value
	}

	if (mCurrentAnimIdx != animState.currentAnimIdx) {
		SetCurrentAnimation(animState.currentAnimIdx);
	}
	animState.playingTime = mAnimCurrentTicks / mAnimTicksPerSecond;
	animState.currentAnimDuration = mAnimDurationTicks / mAnimTicksPerSecond;
	
	UpdateBoneMatrices_internal(scene->mRootNode, scene->mRootNode->mTransformation);
}

void ModelImporter::UpdateBoneMatrices_internal(aiNode* pNode, const aiMatrix4x4& parentTransform) {
	std::string nodeName = pNode->mName.C_Str();
	aiMatrix4x4 nodeTransform = pNode->mTransformation;
	// このノードに対応するアニメーションチャンネルがあれば補間行列を取得
	const aiNodeAnim* pNodeAnim = node_anim_map[nodeName];
	if (pNodeAnim) {
		nodeTransform = InterpolateTransform(pNodeAnim, mAnimCurrentTicks);
	}

	// 親と掛け合わせてこのノードのグローバル行列
	aiMatrix4x4 globalTransform = parentTransform * nodeTransform;
	// 
	if (node_bone_map.count(nodeName)) {
		unsigned int boneIndex = node_bone_map[nodeName];
		// 実際の座標にはboneOffsets -> globalTransform -> globalInverseTransformの順にかかる。
		// 頂点をvとすると, 
		// boneOffsetsはvをモデル空間からボーン空間へtransform
		// globalTransformは親ボーンから蓄積された（現在のアニメーション再生時間を考慮）transform
		// globalInverseTransformはモデル全体にかかってるtransformを打ち消す（ワールド座標系で扱いやすく？）
		boneMatrices[boneIndex] = ConvertFbxMatrix(globalInverseTransform * globalTransform * boneOffsets[boneIndex]);
	}

	for (unsigned int i = 0; i < pNode->mNumChildren; ++i) {
		UpdateBoneMatrices_internal(pNode->mChildren[i], globalTransform);
	}
}

aiMatrix4x4 ModelImporter::InterpolateTransform(const aiNodeAnim* pNodeAnim, float animationTime) {
	//if(pNodeAnim->mNumPositionKeys < 2) {}

	aiVector3D interpolatedPos, interpolatedScale;
	aiQuaternion interpolatedRot;
	// Interpolate position
	if(pNodeAnim->mNumPositionKeys > 1) {
		// 1. 指定したアニメーション時間から、見るべきキーフレームを決定
		unsigned int targetPosKeyIdx = 0;
		for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; ++i) {
			if (animationTime < pNodeAnim->mPositionKeys[i + 1].mTime) {
				targetPosKeyIdx = i;
				break;
			}
		}

		// 2. 補間係数を計算
		float t1 = (float)pNodeAnim->mPositionKeys[targetPosKeyIdx].mTime;
		float t2 = (float)pNodeAnim->mPositionKeys[targetPosKeyIdx + 1].mTime;
		float factor = (animationTime - t1) / (t2 - t1);

		// 3. 補間計算
		// ----- Lerp to position -----
		aiVector3D startPos = pNodeAnim->mPositionKeys[targetPosKeyIdx].mValue;
		aiVector3D endPos = pNodeAnim->mPositionKeys[targetPosKeyIdx + 1].mValue;
		interpolatedPos = startPos + (endPos - startPos) * float(factor);
	}

	// Interpolate scaling
	if(pNodeAnim->mNumScalingKeys > 1) {
		// 1. 指定したアニメーション時間から、見るべきキーフレームを決定
		unsigned int targetScaleKeyIdx = 0;
		for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; ++i) {
			if (animationTime < pNodeAnim->mScalingKeys[i + 1].mTime) {
				targetScaleKeyIdx = i;
				break;
			}
		}

		// 2. 補間係数を計算
		float t1 = (float)pNodeAnim->mScalingKeys[targetScaleKeyIdx].mTime;
		float t2 = (float)pNodeAnim->mScalingKeys[targetScaleKeyIdx + 1].mTime;
		float factor = (animationTime - t1) / (t2 - t1);

		// ----- Lerp to scale -----
		aiVector3D startScale = pNodeAnim->mScalingKeys[targetScaleKeyIdx].mValue;
		aiVector3D endScale = pNodeAnim->mScalingKeys[targetScaleKeyIdx + 1].mValue;
		interpolatedScale = startScale + (endScale - startScale) * float(factor);
	}

	// Interpolate rotation
	if(pNodeAnim->mNumRotationKeys > 1) {
		// 1. 指定したアニメーション時間から、見るべきキーフレームを決定
		unsigned int targetRotKeyIdx = 0;
		for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; ++i) {
			if (animationTime < pNodeAnim->mRotationKeys[i + 1].mTime) {
				targetRotKeyIdx = i;
				break;
			}
		}

		// 2. 補間係数を計算
		float t1 = (float)pNodeAnim->mRotationKeys[targetRotKeyIdx].mTime;
		float t2 = (float)pNodeAnim->mRotationKeys[targetRotKeyIdx + 1].mTime;
		float factor = (animationTime - t1) / (t2 - t1);

		// ----- Slerp to rotation -----
		aiQuaternion startRot = pNodeAnim->mRotationKeys[targetRotKeyIdx].mValue;
		aiQuaternion endRot = pNodeAnim->mRotationKeys[targetRotKeyIdx + 1].mValue;
		aiQuaternion::Interpolate(interpolatedRot, startRot, endRot, float(factor));
	}

	// 4. 変換行列を構築
	// ----- translation -----
	aiMatrix4x4 translationMat;
	aiMatrix4x4::Translation(interpolatedPos, translationMat);
	// ----- scaling -----
	aiMatrix4x4 scalingMat;
	aiMatrix4x4::Scaling(interpolatedScale, scalingMat);
	// ----- rotation -----
	aiMatrix4x4 rotationMat = aiMatrix4x4(interpolatedRot.GetMatrix());

	// Assimpは右からかける
	return translationMat * rotationMat * scalingMat;
}

DirectX::XMMATRIX ModelImporter::ConvertFbxMatrix(const aiMatrix4x4& src)
{
	return		DirectX::XMMatrixSet(
		static_cast<FLOAT>(src.a1), static_cast<FLOAT>(src.b1), static_cast<FLOAT>(src.c1), static_cast<FLOAT>(src.d1),
		static_cast<FLOAT>(src.a2), static_cast<FLOAT>(src.b2), static_cast<FLOAT>(src.c2), static_cast<FLOAT>(src.d2),
		static_cast<FLOAT>(src.a3), static_cast<FLOAT>(src.b3), static_cast<FLOAT>(src.c3), static_cast<FLOAT>(src.d3),
		static_cast<FLOAT>(src.a4), static_cast<FLOAT>(src.b4), static_cast<FLOAT>(src.c4), static_cast<FLOAT>(src.d4));

	//return		XMMatrixSet(
	//	static_cast<FLOAT>(src.a1), static_cast<FLOAT>(src.a2), static_cast<FLOAT>(src.a3), static_cast<FLOAT>(src.a4),
	//	static_cast<FLOAT>(src.b1), static_cast<FLOAT>(src.b2), static_cast<FLOAT>(src.b3), static_cast<FLOAT>(src.b4),
	//	static_cast<FLOAT>(src.c1), static_cast<FLOAT>(src.c2), static_cast<FLOAT>(src.c3), static_cast<FLOAT>(src.c4),
	//	static_cast<FLOAT>(src.d1), static_cast<FLOAT>(src.d2), static_cast<FLOAT>(src.d3), static_cast<FLOAT>(src.d4));
}