//
//  FBXImporter.h
//  ios_unitychan
//
//  Created by ramemiso on 2014/06/07.
//  Copyright (c) 2014年 ramemiso. All rights reserved.
//

#ifndef __ios_unitychan__FBXImporter__
#define __ios_unitychan__FBXImporter__

#include <iostream>
#include <fbxsdk.h>

struct ModelBoneWeight
{
  uint8_t boneIndex[4];
  GLKVector4 boneWeight;
};

struct ModelVertex
{
  GLKVector3 position;
  GLKVector3 normal;
  GLKVector2 uv0;
  uint8_t boneIndex[4];
  GLKVector4 boneWeight;
  
  bool operator == (const ModelVertex& v) const
  {
    return std::memcmp(this, &v, sizeof(ModelVertex)) == 0;
  }
};

struct ModelMesh
{
  std::string nodeName;
  std::string materialName;
  
  std::vector<ModelVertex> vertexList;
  std::vector<uint16_t> indexList;
  
  GLKMatrix4 invMeshBaseposeMatrix;
  std::vector<std::string> boneNodeNameList;
  std::vector<GLKMatrix4> invBoneBaseposeMatrixList;
};

struct ModelMaterial
{
  std::string materialName;
  
  std::string diffuseTextureName;
  std::string normalTextureName;
  std::string specularTextureName;
  std::string falloffTextureName;
  std::string reflectionMapTextureName;
};


class FBXLoader
{
public:
  
  bool Initialize(const char* filepath);
  void Finalize();
  
  bool LoadAnimation(const char* filepath);
  
  int GetMeshCount() const
  {
    return (int)this->meshList.size();
  }
  const ModelMesh& GetMesh(int index) const
  {
    return this->meshList[index];
  }
  
  int GetMaterialCount() const
  {
    return (int)this->materialList.size();
  }
  
  const int GetMaterialId(const std::string& materialName) const
  {
    return this->materialIdDictionary.at(materialName);
  }
  
  const ModelMaterial& GetMaterial(int materialId) const
  {
    return this->materialList[materialId];
  }
  
  float GetAnimationStartFrame() const
  {
    return this->animationStartFrame;
  }
  
  float GetAnimationEndFrame() const
  {
    return this->animationEndFrame;
  }
  
  void GetMeshMatrix(float frame, int meshId, GLKMatrix4* out_matrix) const;
  void GetBoneMatrix(float frame, int meshId, GLKMatrix4* out_matrixList, int matrixCount) const;
  
private:
  
  ModelMesh ParseMesh(FbxMesh* mesh);
  ModelMaterial ParseMaterial(FbxSurfaceMaterial* material);
  
private:
  
  FbxManager* sdkManager;
  FbxScene* fbxScene;
  
  std::vector<ModelMesh> meshList;
  std::vector<ModelMaterial> materialList;
  std::map<std::string, int> materialIdDictionary;
  std::map<std::string, int> nodeIdDictionary;
  
  FbxScene* fbxSceneAnimation;
  std::map<std::string, int> nodeIdDictionaryAnimation;
  float animationStartFrame;
  float animationEndFrame;
};

#endif /* defined(__ios_unitychan__FBXImporter__) */
