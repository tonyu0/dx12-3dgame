#include "FbxFileImporter.h"

FLOAT minX = 1e9, maxX = -1e9;
FLOAT minY = 1e9, maxY = -1e9;
FLOAT minZ = 1e9, maxZ = -1e9;

void FbxFileImporter::DisplayIndex(FbxMesh* mesh) {
	//���|���S����
	int polygonNum = mesh->GetPolygonCount();
	//p�ڂ̃|���S���ւ̏���
	for (int p = 0; p < polygonNum; ++p)
		//p�ڂ̃|���S����n�ڂ̒��_�ւ̏���
		for (int n = 0; n < 3; ++n) {
			int index = mesh->GetPolygonVertex(p, n);
			std::cout << "index[" << p + n << "] : " << index << std::endl;
		}
	// Deformer���擾���邼�B
	// FbxDeformer::eSkin;
}

// ControlPoints = ���_�o�b�t�@�A PolugonVertexCount = ���_���W�H
void FbxFileImporter::LoadMesh(FbxMesh* mesh) {
	// add if attibute is mesh
		// position and normal is in mesh
	int	index_count = mesh->GetPolygonVertexCount();
	int* indices = mesh->GetPolygonVertices();
	std::string meshName = mesh->GetName();
	std::cout << "Load " << meshName << " Data" << std::endl;
	// index buffer�̃R�s�[
	for (int i = 0; i < mesh->GetPolygonCount(); ++i) {
		mesh_indices[meshName].push_back(i * 3);
		mesh_indices[meshName].push_back(i * 3 + 1);
		mesh_indices[meshName].push_back(i * 3 + 2);
	}
	mesh_vertices[meshName].resize(index_count);
	auto AddBoneInfo = [](Vertex& v, int boneid, double weight) {
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
	int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int i = 0; i < skinCount; ++i) {
		FbxSkin* skin = (FbxSkin*)mesh->GetDeformer(i, FbxDeformer::eSkin);
		// if not having skin, it also doesnt have bone. skin is a deformer related to bone.
		int clusterNum = skin->GetClusterCount();
		for (int j = 0; j < clusterNum; ++j) {
			FbxCluster* cluster = skin->GetCluster(j);
			bones[j].cluster = cluster;
			bones[j].meshNode = mesh->GetNode();
			int pointNum = cluster->GetControlPointIndicesCount();
			for (int k = 0; k < pointNum; ++k) {
				int index = cluster->GetControlPointIndices()[k];
				double weight = cluster->GetControlPointWeights()[k];
				for (int l = 0; l < index_count; ++l) {
					if (index == indices[l]) {
						AddBoneInfo(mesh_vertices[meshName][l], j, weight);
					}
				}
			}
		}
	}

	FbxVector4* vertices = mesh->GetControlPoints();
	// element���ĉ��H���_�o�b�t�@�����3�{���炢�����B
	//int elementCount = mesh->GetElementNormalCount();
	//auto element = mesh->GetElementNormal();
	//auto mappingMode = element->GetMappingMode();
	//auto referenceMode = element->GetReferenceMode();
	//const auto& indexArray = element->GetIndexArray();
	//const auto& directArray = element->GetDirectArray();
	FbxArray<FbxVector4> normals;
	mesh->GetPolygonVertexNormals(normals);
	FbxStringList uvset_names;
	mesh->GetUVSetNames(uvset_names);
	FbxArray<FbxVector2> uv_buffer;
	mesh->GetPolygonVertexUVs(uvset_names.GetStringAt(0), uv_buffer); // if I use multiple UV Sets, change idx 0 to other id.
	// index
	for (int i = 0; i < index_count; ++i) {
		int idx = indices[i];
		mesh_vertices[meshName][i].pos.x = (FLOAT)vertices[idx][0];
		mesh_vertices[meshName][i].pos.y = (FLOAT)vertices[idx][1];
		mesh_vertices[meshName][i].pos.z = (FLOAT)vertices[idx][2];

		mesh_vertices[meshName][i].normal.x = (FLOAT)normals[i][0];
		mesh_vertices[meshName][i].normal.y = (FLOAT)normals[i][1];
		mesh_vertices[meshName][i].normal.z = (FLOAT)normals[i][2];

		FbxVector2& uv = uv_buffer[i];
		mesh_vertices[meshName][i].uv.x = (float)uv[0];
		mesh_vertices[meshName][i].uv.y = 1.0f - (float)uv[1];
		//	std::cout << "Mesh is  " << meshName << " " << idx << " bannmeno " << mesh_vertices[meshName][i].pos.x << " " << mesh_vertices[meshName][i].boneid[0] << " " << mesh_vertices[meshName][i].weight[0] << std::endl;
	}

	// mesh�ƃ}�e���A���̌��ѕt���B���b�V���P�ʂŕ`�悷��̂ŁA��������}�e���A��������΂���ɂ�������e�N�X�`�������āA����UV�}�b�s���O�B
	if (mesh->GetElementMaterialCount() > 0) {
		// Mesh���̃}�e���A�������擾
		FbxLayerElementMaterial* material = mesh->GetElementMaterial(0);
		// FbxSurfaceMaterial�̃C���f�b�N�X�o�b�t�@����C���f�b�N�X���擾
		int index = material->GetIndexArray().GetAt(0);
		FbxSurfaceMaterial* surface_material = mesh->GetNode()->GetSrcObject<FbxSurfaceMaterial>(index);
		if (surface_material != nullptr)
			mesh_material_name[meshName] = surface_material->GetName();
		else
			mesh_material_name[meshName] = "";
	}
}

void FbxFileImporter::LoadMaterial(FbxSurfaceMaterial* material)
{
	Material entry_material;
	enum class MaterialOrder
	{
		Ambient,
		Diffuse,
		Specular,
		MaxOrder,
	};

	FbxDouble3 colors[(int)MaterialOrder::MaxOrder];
	FbxDouble factors[(int)MaterialOrder::MaxOrder];
	FbxProperty prop = material->FindProperty(FbxSurfaceMaterial::sAmbient);

	if (material->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
		const char* element_check_list[] = { FbxSurfaceMaterial::sAmbient,FbxSurfaceMaterial::sDiffuse, };
		const char* factor_check_list[] = { FbxSurfaceMaterial::sAmbientFactor, FbxSurfaceMaterial::sDiffuseFactor, };

		for (int i = 0; i < 2; i++) {
			prop = material->FindProperty(element_check_list[i]);
			// ���́uGetSrcObject�v�ƁuGetSrcObjectCount�v�Ńe�N�X�`���̎擾���s���܂����A
			// �擾�Ŏg���^�����ނ���܂�
			// not �}���`�e�N�X�`��: FiFileTexture
			// �}���`�e�N�X�`��: FbxLayeredTexture
			if (prop.IsValid()) {
				colors[i] = prop.Get<FbxDouble3>();
				// FbxFileTexture���擾����
				int texture_num = prop.GetSrcObjectCount<FbxFileTexture>();
				if (texture_num > 0) {
					// prop����FbxFileTexture���擾	
					auto texture = prop.GetSrcObject<FbxFileTexture>(0);
					// �t�@�C�������擾
					std::string file_path = texture->GetRelativeFileName();
					file_path = "C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/FBX/" + file_path.substr(file_path.rfind('\\') + 1, file_path.length());
					std::cout << "LOAD TEXTURE: " << file_path << std::endl;
					// texbuff(����)���󂯎��BWriteToSubresource��
					materialNameToTexture[material->GetName()] = LoadTextureFromFile(file_path);
					//..\
					// �������ϊ�������́A�e���̃e�N�X�`���ǂݍ��݊֐���p���ăe�N�X�`����ǂނ��ށB
					// �`�掞�̃}�e���A�����Ƃ̃e�N�X�`���w�肪�ł���悤�ɁA�}�e���A�����ƃe�N�X�`���������ѕt����B
				}
			}
			else
				colors[i] = FbxDouble3(1.0, 1.0, 1.0);

			prop = material->FindProperty(factor_check_list[i]);
			if (prop.IsValid())
				factors[i] = prop.Get<FbxDouble>();
			else
				factors[i] = 1.0;
		}
	}

	FbxDouble3 color = colors[(int)MaterialOrder::Ambient];
	FbxDouble factor = factors[(int)MaterialOrder::Ambient];
	entry_material.SetAmbient((float)color[0], (float)color[1], (float)color[2], (float)factor);

	color = colors[(int)MaterialOrder::Diffuse];
	factor = factors[(int)MaterialOrder::Diffuse];
	entry_material.SetDiffuse((float)color[0], (float)color[1], (float)color[2], (float)factor);

	raw_materials[material->GetName()] = entry_material;
}

// https://help.autodesk.com/view/FBX/2017/ENU/?guid=__cpp_ref__common_2_common_8cxx_example_html
bool FbxFileImporter::SaveScene(FbxManager* pManager, FbxDocument* pScene, const char* pFilename, int pFileFormat, bool pEmbedMedia)
{
#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(pManager->GetIOSettings()))
#endif
	int lMajor, lMinor, lRevision;
	bool lStatus = true;
	// Create an exporter.
	FbxExporter* lExporter = FbxExporter::Create(pManager, "");
	if (pFileFormat < 0 || pFileFormat >= pManager->GetIOPluginRegistry()->GetWriterFormatCount())
	{
		// Write in fall back format in less no ASCII format found
		pFileFormat = pManager->GetIOPluginRegistry()->GetNativeWriterFormat();
		//Try to export in ASCII if possible
		int lFormatIndex, lFormatCount = pManager->GetIOPluginRegistry()->GetWriterFormatCount();
		for (lFormatIndex = 0; lFormatIndex < lFormatCount; lFormatIndex++)
		{
			if (pManager->GetIOPluginRegistry()->WriterIsFBX(lFormatIndex))
			{
				FbxString lDesc = pManager->GetIOPluginRegistry()->GetWriterFormatDescription(lFormatIndex);
				const char* lASCII = "ascii";
				if (lDesc.Find(lASCII) >= 0)
				{
					pFileFormat = lFormatIndex;
					break;
				}
			}
		}
	}
	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.
	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL, true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE, true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED, pEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE, true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO, true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION, true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);
	// Initialize the exporter by providing a filename.
	if (lExporter->Initialize(pFilename, pFileFormat, pManager->GetIOSettings()) == false)
	{
		FBXSDK_printf("Call to FbxExporter::Initialize() failed.\n");
		FBXSDK_printf("Error returned: %s\n\n", lExporter->GetStatus().GetErrorString());
		return false;
	}
	FbxManager::GetFileFormatVersion(lMajor, lMinor, lRevision);
	FBXSDK_printf("FBX file format version %d.%d.%d\n\n", lMajor, lMinor, lRevision);
	// Export the scene.
	lStatus = lExporter->Export(pScene);
	// Destroy the exporter.
	lExporter->Destroy();
	return lStatus;
}

void FbxFileImporter::CreateFbxManager() {
	// FBX �ǂݍ��݌n
	//FbxManager��FbxScene�I�u�W�F�N�g���쐬
	FbxManager* manager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);
	scene = FbxScene::Create(manager, "");

	//�f�[�^���C���|�[�g
	//const char* filename = "assets/FBX/Alicia_solid.FBX";
	//const char* filename = "C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/FBX/Alicia.fbx";
	const char* filename = "C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/WhipperNude/model/WhipperNude.fbx";
	FbxImporter* importer = FbxImporter::Create(manager, "");
	importer->Initialize(filename, -1, manager->GetIOSettings());
	int maj, min, rev;
	importer->GetFileVersion(maj, min, rev);
	std::cout << maj << " " << min << " " << rev << " "
		<< std::endl;
	importer->Import(scene);
	importer->Destroy();
	// �O�p�|���S����
	//FbxGeometryConverter geometryConverter(manager);

	// Animation Preparation
	FbxArray<FbxString*> AnimStackNameArray;
	int AnimStackNumber = 0; // select animation
	scene->FillAnimStackNameArray(AnimStackNameArray);
	for (int i = 0; i < AnimStackNameArray.Size(); ++i) {
		std::cout << *AnimStackNameArray[i] << std::endl;
	}
	FbxAnimStack* AnimationStack = scene->FindMember<FbxAnimStack>(AnimStackNameArray[7]->Buffer());
	scene->SetCurrentAnimationStack(AnimationStack); // set this animation stack to use

	FbxTakeInfo* takeInfo = scene->GetTakeInfo(*(AnimStackNameArray[AnimStackNumber]));
	animStartTime = takeInfo->mLocalTimeSpan.GetStart();
	animStopTime = takeInfo->mLocalTimeSpan.GetStop();
	frameTime.SetTime(0, 0, 0, 1, 0, scene->GetGlobalSettings().GetTimeMode());
	timeCount = animStartTime;

	// ����ň�̃��b�V���̃|���S���ɕ����̃}�e���A�������蓖�Ă��Ă��Ă��A��̃��b�V���ɂ���̃}�e���A���Ƃ��ĕ��������B
	//geometryConverter.SplitMeshesPerMaterial(scene, true);
	// geometryConverter.Triangulate(scene, true);
	// SaveScene(manager, scene, "Alicia.fbx", 0, 0);
	// ���󂱂�ŏ����o����maj=7, min=7, rev=0�ɂȂ�Ablender�œǂ߂Ȃ��`���ɂȂ�

	int materialCount = scene->GetMaterialCount();
	for (int i = 0; i < materialCount; ++i) {
		FbxSurfaceMaterial* surfaceMaterial = scene->GetMaterial(i);
		LoadMaterial(surfaceMaterial);

	}
	int meshCount = scene->GetSrcObjectCount<FbxMesh>();
	for (int i = 0; i < meshCount; ++i) {
		FbxMesh* mesh = scene->GetSrcObject<FbxMesh>(i);
		std::string texturePath = "C:/Users/sator/source/repos/dx12-3dgame/dx12-3dgame/assets/WhipperNude/texture/middle/" + (std::string)mesh->GetName() + ".tga";
		materialNameToTexture[mesh->GetName()] = LoadTextureFromFile(texturePath);
		LoadMesh(mesh);
	}
	//manager->Destroy();
}

void FbxFileImporter::UpdateBoneMatrices() {
	int meshCount = scene->GetSrcObjectCount<FbxMesh>();
	timeCount += frameTime;
	if (timeCount > animStopTime) { timeCount = animStartTime; }
	for (int i = 0; bones[i].meshNode != nullptr; ++i) {
		FbxNode* meshNode = bones[i].meshNode;
		FbxCluster* cluster = bones[i].cluster;

		FbxAMatrix globalPosition = meshNode->EvaluateGlobalTransform(timeCount);
		FbxVector4 t0 = meshNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		FbxVector4 r0 = meshNode->GetGeometricRotation(FbxNode::eSourcePivot);
		FbxVector4 s0 = meshNode->GetGeometricScaling(FbxNode::eSourcePivot);
		FbxAMatrix geometryOffset = FbxAMatrix(t0, r0, s0);

		FbxAMatrix referenceGlobalInitPosition;
		FbxAMatrix clusterGlobalInitPosition;
		FbxAMatrix clusterGlobalCurrentPosition;
		FbxAMatrix clusterRelativeInitPosition;
		FbxAMatrix clusterRelativeCurrentPositionInverse;
		cluster->GetTransformMatrix(referenceGlobalInitPosition);
		referenceGlobalInitPosition *= geometryOffset;
		cluster->GetTransformLinkMatrix(clusterGlobalInitPosition);
		clusterGlobalCurrentPosition = cluster->GetLink()->EvaluateGlobalTransform(timeCount);
		clusterRelativeInitPosition = clusterGlobalInitPosition.Inverse() * referenceGlobalInitPosition;
		clusterRelativeCurrentPositionInverse = globalPosition.Inverse() * clusterGlobalCurrentPosition;
		FbxAMatrix vertexTransformMatrix = clusterRelativeCurrentPositionInverse * clusterRelativeInitPosition;

		// BoneMatrix = InitialPoseMatrix * FramePoseMatrix
		//FbxAMatrix initPositionMatrix;
		//FbxAMatrix currentPositionMatrix;
		//bones[i]->GetTransformLinkMatrix(initPositionMatrix); // initMat
		//currentPositionMatrix = bones[i]->GetLink()->EvaluateGlobalTransform(timeCount);
		//FbxAMatrix finalTransformMatrix = initPositionMatrix.Inverse() * currentPositionMatrix;
		boneMatrices[i] = ConvertFbxMatrix(vertexTransformMatrix);
	}
}


// Texture load ��p�N���X�����
// Texture��ǂݍ��݁AID3D12Resource�ԋp�܂ŁAmap�͍s��Ȃ�
std::wstring FbxFileImporter::GetWideStringFromString(const std::string& str) {
	//�Ăяo��1���(�����񐔂𓾂�)
	auto num1 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, nullptr, 0);

	std::wstring wstr;//string��wchar_t��
	wstr.resize(num1);//����ꂽ�����񐔂Ń��T�C�Y

	//�Ăяo��2���(�m�ۍς݂�wstr�ɕϊ���������R�s�[)
	auto num2 = MultiByteToWideChar(CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(), -1, &wstr[0], num1);

	assert(num1 == num2);//�ꉞ�`�F�b�N
	return wstr;
}
//using LoadLambda_t = std::function<HRESULT(const std::wstring& path, TexMetadata*, ScratchImage&)>;
//std::map <std::string, LoadLambda_t> loadLambdaTable;

DirectX::XMMATRIX FbxFileImporter::ConvertFbxMatrix(FbxAMatrix& src)
{
	return		XMMatrixSet(
		static_cast<FLOAT>(src.Get(0, 0)), static_cast<FLOAT>(src.Get(0, 1)), static_cast<FLOAT>(src.Get(0, 2)), static_cast<FLOAT>(src.Get(0, 3)),
		static_cast<FLOAT>(src.Get(1, 0)), static_cast<FLOAT>(src.Get(1, 1)), static_cast<FLOAT>(src.Get(1, 2)), static_cast<FLOAT>(src.Get(1, 3)),
		static_cast<FLOAT>(src.Get(2, 0)), static_cast<FLOAT>(src.Get(2, 1)), static_cast<FLOAT>(src.Get(2, 2)), static_cast<FLOAT>(src.Get(2, 3)),
		static_cast<FLOAT>(src.Get(3, 0)), static_cast<FLOAT>(src.Get(3, 1)), static_cast<FLOAT>(src.Get(3, 2)), static_cast<FLOAT>(src.Get(3, 3)));
}

// �V�F�[�_�[���\�[�X�r���[�̓}�e���A���Ɠ����쐬
// �V�F�[�_�[���\�[�X�r���[�ƃ}�e���A���̃r���[�͓����f�B�X�N���v�^�q�[�v�ɒu���B
// �}�e���A���ƃe�N�����Ⴛ�ꂼ��̃��[�g�p�����[�^�[���܂Ƃ߂�B�f�B�X�N���v�^�����W�͕�����B
// 1. �e�N�X�`�����[�h�A�@2. ���\�[�X�̍쐬�A 3. �f�[�^�R�s�[

ID3D12Resource* FbxFileImporter::LoadTextureFromFile(std::string& texPath) {
	// psd -> tga �g����
//	texPath.replace(texPath.find("psd"), 3, "tga");
	// ������literal���Q�ƂŎ󂯎���Ă�Ƃ����ŃG���[


	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end()) {
		//�e�[�u���ɓ��ɂ������烍�[�h����̂ł͂Ȃ��}�b�v����
		//���\�[�X��Ԃ�
		return _resourceTable[texPath];
	}
	//WIC�e�N�X�`���̃��[�h
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	std::wstring wtexpath = GetWideStringFromString(texPath);//�e�N�X�`���̃t�@�C���p�X
	std::string ext = GetExtension(texPath);//�g���q���擾
	//auto result = loadLambdaTable[ext](wtexpath,
	//	&metadata,
	//	scratchImg);
	HRESULT result = LoadFromTGAFile(wtexpath.c_str(), &metadata, scratchImg);

	std::cout << "LOADING: " << texPath << std::endl;
	if (FAILED(result)) {
		return nullptr;
	}
	auto img = scratchImg.GetImage(0, 0, 0);//���f�[�^���o

	//WriteToSubresource�œ]������p�̃q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;//����Ȑݒ�Ȃ̂�default�ł�upload�ł��Ȃ�
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//���C�g�o�b�N��
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//�]����L0�܂�CPU�����璼��
	texHeapProp.CreationNodeMask = 0;//�P��A�_�v�^�̂���0
	texHeapProp.VisibleNodeMask = 0;//�P��A�_�v�^�̂���0

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = static_cast<UINT>(metadata.width);//��
	resDesc.Height = static_cast<UINT>(metadata.height);//����
	resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resDesc.SampleDesc.Count = 1;//�ʏ�e�N�X�`���Ȃ̂ŃA���`�F�����Ȃ�
	resDesc.SampleDesc.Quality = 0;//
	resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);//�~�b�v�}�b�v���Ȃ��̂Ń~�b�v���͂P��
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//���C�A�E�g�ɂ��Ă͌��肵�Ȃ�
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;//�Ƃ��Ƀt���O�Ȃ�

	std::cout << "CREATE BUFFER: " << texPath << std::endl;
	ID3D12Resource* texbuff = nullptr;
	result = Device->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,//���Ɏw��Ȃ�
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result)) {
		return nullptr;
	}

	std::cout << "LOADING: " << texPath << std::endl;
	result = texbuff->WriteToSubresource(0,
		nullptr,//�S�̈�փR�s�[
		img->pixels,//���f�[�^�A�h���X
		static_cast<UINT>(img->rowPitch),//1���C���T�C�Y
		static_cast<UINT>(img->slicePitch)//�S�T�C�Y
	);
	if (FAILED(result)) {
		return nullptr;
	}
	std::cout << texPath << " is LOADED!!!" << std::endl;

	_resourceTable[texPath] = texbuff;
	return texbuff;
}

