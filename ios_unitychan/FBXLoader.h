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

struct ModelKey
{
  float frame;
  float value;
};

struct ModelFCurve
{
  std::string nodeName;
  
  std::vector<ModelKey> transXList;
  std::vector<ModelKey> transYList;
  std::vector<ModelKey> transZList;
  
  std::vector<ModelKey> rotateXList;
  std::vector<ModelKey> rotateYList;
  std::vector<ModelKey> rotateZList;
  
  std::vector<ModelKey> scaleXList;
  std::vector<ModelKey> scaleYList;
  std::vector<ModelKey> scaleZList;
};

struct ModelAnim
{
  float startFrame;
  float endFrame;
  
  std::vector<ModelFCurve> fCurveList;
};

struct ModelVertex
{
  GLKVector3 position;
  int16_t normal[3];
  int16_t pad;
  int16_t uv0[2];
  uint8_t boneIndex[4];
  uint8_t boneWeight[4];
  
  bool operator == (const ModelVertex& v) const
  {
    return std::memcmp(this, &v, sizeof(ModelVertex)) == 0;
  }
};

struct ModelNode
{
  std::string nodeName;
  std::string parentName;
  
  int nodeId;
  int parentId;
  
  GLKVector3 baseTrans;
  GLKVector3 baseRotate;
  GLKVector3 baseScale;
  GLKMatrix4 invBaseposeMatrix;
  
  int animNodeId;
};

struct ModelMesh
{
  std::string nodeName;
  std::string materialName;
  
  std::vector<ModelVertex> vertexList;
  std::vector<uint16_t> indexList;
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
  
  int GetNodeId(const std::string& nodeName) const
  {
    return this->nodeIdDictionary.at(nodeName);
  }
  
  int GetMaterialCount() const
  {
    return (int)this->materialList.size();
  }
  
  int GetMaterialId(const std::string& materialName) const
  {
    return this->materialIdDictionary.at(materialName);
  }
  
  const ModelMaterial& GetMaterial(int materialId) const
  {
    return this->materialList[materialId];
  }
  
  void Update(float dt);
  void GetNodeMatrixList(GLKMatrix4* out_matrixList, int matrixCount) const;
  
private:
  
  ModelMesh ParseMesh(FbxMesh* mesh);
  ModelMaterial ParseMaterial(FbxSurfaceMaterial* material);
  
private:
  
  FbxManager* sdkManager;
  FbxScene* fbxScene;
  
  std::vector<ModelNode> nodeList;
  std::vector<ModelMesh> meshList;
  std::vector<ModelMaterial> materialList;
  std::map<std::string, int> materialIdDictionary;
  std::map<std::string, int> nodeIdDictionary;
  
  FbxScene* fbxSceneAnimation;
  std::vector<GLKMatrix4> nodeMatrixList;
  ModelAnim anim;
  float animFrame;
};

#endif /* defined(__ios_unitychan__FBXImporter__) */
