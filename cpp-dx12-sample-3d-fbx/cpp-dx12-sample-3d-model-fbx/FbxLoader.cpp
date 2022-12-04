#include "FbxLoader.h"

FbxLoader::FbxLoader()
{
}

bool FbxLoader::Load(const std::string& filePath, VertexInfo* vertexInfo)
{
	// �}�l�[�W���[������
	auto manager = FbxManager::Create();

	// �C���|�[�^�[������
	auto importer = FbxImporter::Create(manager, "");
	if (!importer->Initialize(filePath.c_str(), -1, manager->GetIOSettings()))
	{
		return false;
	}

	// �V�[���쐬
	auto scene = FbxScene::Create(manager, "");
	importer->Import(scene);
	importer->Destroy();

	// �O�p�|���S���ւ̃R���o�[�g
	FbxGeometryConverter geometryConverter(manager);
	if (!geometryConverter.Triangulate(scene, true))
	{
		return false;
	}

	// ���b�V���擾
	auto mesh = scene->GetSrcObject<FbxMesh>();
	if (!mesh)
	{
		return false;
	}

	// UV�Z�b�g���̎擾
	// * ���݂̎������Ƃ�1��UV�Z�b�g���ɂ����Ή����Ă��Ȃ�...
	FbxStringList uvSetNameList;
	mesh->GetUVSetNames(uvSetNameList);
	const char* uvSetName = uvSetNameList.GetStringAt(0);

	// ���_���W���̃��X�g�𐶐�
	std::vector<std::vector<float>> vertexInfoList;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++)
	{
		// ���_���W��ǂݍ���Őݒ�
		auto point = mesh->GetControlPointAt(i);
		std::vector<float> vertex;
		vertex.push_back(point[0]);
		vertex.push_back(point[1]);
		vertex.push_back(point[2]);
		vertexInfoList.push_back(vertex);
	}

	// ���_���̏����擾����
	std::vector<unsigned short> indices;
	std::vector<std::array<int, 2>> oldNewIndexPairList;
	for (int polIndex = 0; polIndex < mesh->GetPolygonCount(); polIndex++) // �|���S�����̃��[�v
	{
		for (int polVertexIndex = 0; polVertexIndex < mesh->GetPolygonSize(polIndex); polVertexIndex++) // ���_���̃��[�v
		{
			// �C���f�b�N�X���W
			auto vertexIndex = mesh->GetPolygonVertex(polIndex, polVertexIndex);

			// ���_���W
			std::vector<float> vertexInfo = vertexInfoList[vertexIndex];

			// �@�����W
			FbxVector4 normalVec4;
			mesh->GetPolygonVertexNormal(polIndex, polVertexIndex, normalVec4);

			// UV���W
			FbxVector2 uvVec2;
			bool isUnMapped;
			mesh->GetPolygonVertexUV(polIndex, polVertexIndex, uvSetName, uvVec2, isUnMapped);

			// �C���f�b�N�X���W�̃`�F�b�N�ƍč̔�
			if (!IsExistNormalUVInfo(vertexInfo))
			{
				// �@�����W��UV���W�����ݒ�̏ꍇ�A���_���ɕt�^���čĐݒ�
				vertexInfoList[vertexIndex] = CreateVertexInfo(vertexInfo, normalVec4, uvVec2);
			}
			else if (!IsSetNormalUV(vertexInfo, normalVec4, uvVec2))
			{
				// �����꒸�_�C���f�b�N�X�̒��Ŗ@�����W��UV���W���قȂ�ꍇ�A
				// �V���Ȓ��_�C���f�b�N�X�Ƃ��č쐬����
				vertexIndex = CreateNewVertexIndex(vertexInfo, normalVec4, uvVec2, vertexInfoList, vertexIndex, oldNewIndexPairList);
			}

			// �C���f�b�N�X���W��ݒ�
			indices.push_back(vertexIndex);
		}
	}


	// ���_���𐶐�
	std::vector<Vertex> vertices;
	for (int i = 0; i < vertexInfoList.size(); i++)
	{
		std::vector<float> vertexInfo = vertexInfoList[i];
		vertices.push_back(Vertex{
			{
				vertexInfo[0], vertexInfo[1], vertexInfo[2]
			},
			{
				vertexInfo[3], vertexInfo[4], vertexInfo[5]
			},
			{
				vertexInfo[6], 1.0f - vertexInfo[7] // Blender�ō쐬�����ꍇ�AV�l�͔��]������
			}
		});
	}
	

	// �}�l�[�W���[�A�V�[���̔j��
	scene->Destroy();
	manager->Destroy();

	// �ԋp�l�ɐݒ�
	*vertexInfo = {
		vertices,
		indices
	};

	return true;
}

// �@���AUV��񂪑��݂��Ă��邩�H
bool FbxLoader::IsExistNormalUVInfo(const std::vector<float>& vertexInfo)
{
	return vertexInfo.size() == 8; // ���_3 + �@��3 + UV2
}

// ���_���𐶐�
std::vector<float> FbxLoader::CreateVertexInfo(const std::vector<float>& vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2)
{
	std::vector<float> newVertexInfo;
	// �ʒu���W
	newVertexInfo.push_back(vertexInfo[0]);
	newVertexInfo.push_back(vertexInfo[1]);
	newVertexInfo.push_back(vertexInfo[2]);
	// �@�����W
	newVertexInfo.push_back(normalVec4[0]);
	newVertexInfo.push_back(normalVec4[1]);
	newVertexInfo.push_back(normalVec4[2]);
	// UV���W
	newVertexInfo.push_back(uvVec2[0]);
	newVertexInfo.push_back(uvVec2[1]);
	return newVertexInfo;
}

// �V���Ȓ��_�C���f�b�N�X�𐶐�����
int FbxLoader::CreateNewVertexIndex(const std::vector<float>& vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2,
	std::vector<std::vector<float>>& vertexInfoList, int oldIndex, std::vector<std::array<int, 2>>& oldNewIndexPairList)
{
	// �쐬�ς̏ꍇ�A�Y���̃C���f�b�N�X��Ԃ�
	for (int i = 0; i < oldNewIndexPairList.size(); i++)
	{
		int newIndex = oldNewIndexPairList[i][1];
		if (oldIndex == oldNewIndexPairList[i][0]
			&& IsSetNormalUV(vertexInfoList[newIndex], normalVec4, uvVec2))
		{
			return newIndex;
		}
	}
	// �쐬�ςłȂ��ꍇ�A�V���Ȓ��_�C���f�b�N�X�Ƃ��č쐬
	std::vector<float> newVertexInfo = CreateVertexInfo(vertexInfo, normalVec4, uvVec2);
	vertexInfoList.push_back(newVertexInfo);
	// �쐬�����C���f�b�N�X����ݒ�
	int newIndex = vertexInfoList.size() - 1;
	std::array<int, 2> oldNewIndexPair{ oldIndex , newIndex };
	oldNewIndexPairList.push_back(oldNewIndexPair);
	return newIndex;
}

// vertexInfo�ɖ@���AUV���W���ݒ�ς��ǂ����H
bool FbxLoader::IsSetNormalUV(const std::vector<float> vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2)
{
	// �@���AUV���W�����l�Ȃ�ݒ�ςƂ݂Ȃ�
	return fabs(vertexInfo[3] - normalVec4[0]) < FLT_EPSILON
		&& fabs(vertexInfo[4] - normalVec4[1]) < FLT_EPSILON
		&& fabs(vertexInfo[5] - normalVec4[2]) < FLT_EPSILON
		&& fabs(vertexInfo[6] - uvVec2[0]) < FLT_EPSILON
		&& fabs(vertexInfo[7] - uvVec2[1]) < FLT_EPSILON;
}
