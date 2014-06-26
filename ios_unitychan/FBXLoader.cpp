//
//  FBXImporter.cpp
//  ios_unitychan
//
//  Created by ramemiso on 2014/06/07.
//  Copyright (c) 2014年 ramemiso. All rights reserved.
//

#include <fbxsdk.h>

#include "FBXLoader.h"

std::vector<int> GetIndexList(FbxMesh* mesh)
{
  // あらかじめ三角形化してある
  auto polygonCount = mesh->GetPolygonCount();
  
  std::vector<int> indexList;
  indexList.reserve(polygonCount * 3);
  
  for (int i = 0; i < polygonCount; ++i)
  {
    indexList.push_back(mesh->GetPolygonVertex(i, 0));
    indexList.push_back(mesh->GetPolygonVertex(i, 1));
    indexList.push_back(mesh->GetPolygonVertex(i, 2));
  }
  
  return indexList;
}

std::vector<GLKVector3> GetPositionList(FbxMesh* mesh, const std::vector<int>& indexList)
{
  // コントロールポイントがいわゆる位置座標
  std::vector<GLKVector3> positionList;
  positionList.reserve(indexList.size());
  
  for (auto index : indexList)
  {
    auto controlPoint = mesh->GetControlPointAt(index);
    
    positionList.push_back(GLKVector3Make(controlPoint[0], controlPoint[1], controlPoint[2]));
    
    // wは0.0のみ対応
    assert(controlPoint[3] == 0.0);
  }
  
  return positionList;
}

std::vector<GLKVector3> GetNormalList(FbxMesh* mesh, const std::vector<int>& indexList)
{
  auto elementCount = mesh->GetElementNormalCount();
  
  // 法線要素が1のみ対応
  assert(elementCount == 1);
  
  auto element = mesh->GetElementNormal();
  auto mappingMode = element->GetMappingMode();
  auto referenceMode = element->GetReferenceMode();
  const auto& indexArray = element->GetIndexArray();
  const auto& directArray = element->GetDirectArray();
  
  // eDirctかeIndexDirectのみ対応
  assert((referenceMode == FbxGeometryElement::eDirect) || (referenceMode == FbxGeometryElement::eIndexToDirect));
  
  std::vector<GLKVector3> normalList;
  normalList.reserve(indexList.size());
  
  if (mappingMode == FbxGeometryElement::eByControlPoint)
  {
    // コントロールポイントでマッピング
    for (auto index : indexList)
    {
      auto normalIndex = (referenceMode == FbxGeometryElement::eDirect)
      ? index
      : indexArray.GetAt(index);
      auto normal = directArray.GetAt(normalIndex);
      normalList.push_back(GLKVector3Make(normal[0], normal[1], normal[2]));
      
      // wは1.0のみ対応
      assert(normal[3] == 1.0);
    }
  }
  else if (mappingMode == FbxGeometryElement::eByPolygonVertex)
  {
    // ポリゴンバーテックス（インデックス）でマッピング
    auto indexByPolygonVertex = 0;
    auto polygonCount = mesh->GetPolygonCount();
    for (int i = 0; i < polygonCount; ++i)
    {
      auto polygonSize = mesh->GetPolygonSize(i);
      
      for (int j = 0; j < polygonSize; ++j)
      {
        auto normalIndex = (referenceMode == FbxGeometryElement::eDirect)
        ? indexByPolygonVertex
        : indexArray.GetAt(indexByPolygonVertex);
        auto normal = directArray.GetAt(normalIndex);
        
        normalList.push_back(GLKVector3Make(normal[0], normal[1], normal[2]));
        
        // wは1.0のみ対応
        assert(normal[3] == 1.0);
        
        ++indexByPolygonVertex;
      }
      
    }
  }
  else
  {
    // それ以外のマッピングモードには対応しない
    assert(false);
  }
  
  return normalList;
}

std::vector<GLKVector2> GetUVList(FbxMesh* mesh, const std::vector<int>& indexList, int uvNo)
{
  std::vector<GLKVector2> uvList;
  
  auto elementCount = mesh->GetElementUVCount();
  if (uvNo + 1 > elementCount)
  {
    return uvList;
  }
  
  auto element = mesh->GetElementUV(uvNo);
  auto mappingMode = element->GetMappingMode();
  auto referenceMode = element->GetReferenceMode();
  const auto& indexArray = element->GetIndexArray();
  const auto& directArray = element->GetDirectArray();
  
  // eDirctかeIndexDirectのみ対応
  assert((referenceMode == FbxGeometryElement::eDirect) || (referenceMode == FbxGeometryElement::eIndexToDirect));
  
  uvList.reserve(indexList.size());
  
  if (mappingMode == FbxGeometryElement::eByControlPoint)
  {
    // コントロールポイントでマッピング
    for (auto index : indexList)
    {
      auto uvIndex = (referenceMode == FbxGeometryElement::eDirect)
      ? index
      : indexArray.GetAt(index);
      auto uv = directArray.GetAt(uvIndex);
      uvList.push_back(GLKVector2Make(uv[0], uv[1]));
    }
  }
  else if (mappingMode == FbxGeometryElement::eByPolygonVertex)
  {
    // ポリゴンバーテックス（インデックス）でマッピング
    auto indexByPolygonVertex = 0;
    auto polygonCount = mesh->GetPolygonCount();
    for (int i = 0; i < polygonCount; ++i)
    {
      auto polygonSize = mesh->GetPolygonSize(i);
      
      for (int j = 0; j < polygonSize; ++j)
      {
        auto uvIndex = (referenceMode == FbxGeometryElement::eDirect)
        ? indexByPolygonVertex
        : indexArray.GetAt(indexByPolygonVertex);
        auto uv = directArray.GetAt(uvIndex);
        
        uvList.push_back(GLKVector2Make(uv[0], uv[1]));
        
        ++indexByPolygonVertex;
      }
      
    }
  }
  else
  {
    // それ以外のマッピングモードには対応しない
    assert(false);
  }
  
  return uvList;
}

void GetWeight(FbxMesh* mesh, const std::vector<int>& indexList, std::vector<ModelBoneWeight>& boneWeightList, std::map<std::string, int>& nodeIdDictionary)
{
  auto skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
  if (skinCount == 0)
  {
    return;
  }
  
  // スキンが1しか対応しない
  assert(skinCount <= 1);
 
  auto controlPointsCount = mesh->GetControlPointsCount();
  using TmpWeight = std::pair<int, float>;
  std::vector<std::vector<TmpWeight>> tmpBoneWeightList(controlPointsCount);
  
  auto skin = static_cast<FbxSkin*>(mesh->GetDeformer(0, FbxDeformer::eSkin));
  auto clusterCount = skin->GetClusterCount();
  
  for (int i = 0; i < clusterCount; ++i)
  {
    auto cluster = skin->GetCluster(i);
    
    // eNormalizeしか対応しない
    assert(cluster->GetLinkMode() == FbxCluster::eNormalize);
    
    auto nodeId = nodeIdDictionary.at(cluster->GetLink()->GetName());
    
    auto indexCount = cluster->GetControlPointIndicesCount();
    auto indices = cluster->GetControlPointIndices();
    auto weights = cluster->GetControlPointWeights();
    
    for (int j = 0; j < indexCount; ++j)
    {
      auto controlPointIndex = indices[j];
      tmpBoneWeightList[controlPointIndex].push_back({nodeId, weights[j]});
    }
  }
  
  // コントロールポイントに対応したウェイトを作成
  std::vector<ModelBoneWeight> boneWeightListControlPoints;
  for (auto& tmpBoneWeight : tmpBoneWeightList)
  {
    // ウェイトの大きさでソート
    std::sort(tmpBoneWeight.begin(), tmpBoneWeight.end(),
              [](const TmpWeight& weightA, const TmpWeight& weightB){ return weightA.second > weightB.second; }
              //[](const TmpWeight& weightA, const TmpWeight& weightB){ return weightA.second < weightB.second; }
    );
    
    // 1頂点に4つより多くウェイトが割り振られているなら影響が少ないものは無視する
    while (tmpBoneWeight.size() > 4)
    {
      tmpBoneWeight.pop_back();
    }
    
    // 4つに満たない場合はダミーを挿入
    while (tmpBoneWeight.size() < 4)
    {
      tmpBoneWeight.push_back({0, 0.0f});
    }
   
    ModelBoneWeight weight;
    float total = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
      weight.boneIndex[i] = tmpBoneWeight[i].first;
      weight.boneWeight.v[i] = tmpBoneWeight[i].second;
      
      total += tmpBoneWeight[i].second;
    }
    
    // ウェイトの正規化
    for (int i = 0; i < 4; ++i)
    {
      weight.boneWeight.v[i] /= total;
    }
    
    boneWeightListControlPoints.push_back(weight);
  }
  
  // インデックスで展開
  for (auto index : indexList)
  {
    boneWeightList.push_back(boneWeightListControlPoints[index]);
  }
}

std::vector<ModelKey> GetKeyList(FbxAnimCurve* curve, bool isRotate)
{
  std::vector<ModelKey> keyList;
  
  if (curve == nullptr)
  {
    return keyList;
  }
  
  auto keyCount = curve->KeyGetCount();
  
  for (int i = 0; i < keyCount; ++i)
  {
    auto fbxKey = curve->KeyGet(i);
    
    auto time = fbxKey.GetTime();
    auto value = fbxKey.GetValue();
    auto interpolation = fbxKey.GetInterpolation();
    assert(interpolation == FbxAnimCurveDef::eInterpolationCubic);
    
    auto tangentMode = fbxKey.GetTangentMode();
    assert(tangentMode == FbxAnimCurveDef::eTangentAuto);
    
//    auto rightSlope = key.GetDataFloat(FbxAnimCurveDef::eRightSlope);
//    auto nextLeftSlope = key.GetDataFloat(FbxAnimCurveDef::eNextLeftSlope);
//    auto tangentWeightMode = key.GetTangentWeightMode();
//    auto tangentVelocityMode = key.GetTangentVelocityMode();
    
    ModelKey key;
    key.frame = (double)time.Get() / FbxTime::GetOneFrameValue(FbxTime::eFrames60);
    key.value = value;
    
    // 回転ならディグリーからラジアンに変換しておく
    if (isRotate)
    {
      key.value = GLKMathDegreesToRadians(key.value);
    }
    
    keyList.push_back(key);
  }
  
  // 全フレーム同じ値なら省略する
  auto firstValue = keyList[0].value;
  auto isSame = true;
  
  for (int i = 1; i < keyCount; ++i)
  {
    if (firstValue != keyList[i].value)
    {
      isSame = false;
      break;
    }
  }
  
  if (isSame)
  {
    //keyList.resize(1);
  }
  
  return keyList;
}

bool FBXLoader::Initialize(const char* filepath)
{
  this->sdkManager = FbxManager::Create();
  
  auto ios = FbxIOSettings::Create(this->sdkManager, IOSROOT);
  this->sdkManager->SetIOSettings(ios);
  
  auto importer = FbxImporter::Create(this->sdkManager, "");
  
  if (!importer->Initialize(filepath, -1, this->sdkManager->GetIOSettings()))
  {
    return false;
  }
  
  this->fbxScene = FbxScene::Create(this->sdkManager, "originalScene");
  importer->Import(this->fbxScene);
  importer->Destroy();
  
  // あらかじめポリゴンを全て三角形化しておく
  FbxGeometryConverter geometryConverter(this->sdkManager);
  geometryConverter.Triangulate(this->fbxScene, true);
  
  // ノード名からノードIDを取得できるように辞書に登録
  auto nodeCount = this->fbxScene->GetNodeCount();
  printf("nodeCount: %d\n", nodeCount);
  for (int i = 0; i < nodeCount; ++i)
  {
    auto fbxNode = this->fbxScene->GetNode(i);
    this->nodeIdDictionary.insert({fbxNode->GetName(), i});
    
    auto fbxNodeParent = fbxNode->GetParent();
    
    ModelNode node;
    node.nodeName = fbxNode->GetName();
    node.nodeId = i;
    if (fbxNodeParent != nullptr)
    {
      node.parentName = fbxNodeParent->GetName();
      node.parentId = this->nodeIdDictionary.at(node.parentName);
    }
    else
    {
      node.parentName.clear();
      node.parentId = -1;
    }
    node.animNodeId = -1;
    
    auto localMatrix = fbxNode->EvaluateLocalTransform();
    auto localMatrixPtr = (double*)localMatrix;
    for (int j = 0; j < 16; ++j)
    {
      node.localMatrix.m[j] = localMatrixPtr[j];
    }
    
    auto invBasePoseMatrix = fbxNode->EvaluateGlobalTransform().Inverse();
    auto invBasePoseMatrixPtr = (double*)invBasePoseMatrix;
    for (int j = 0; j < 16; ++j)
    {
      node.invBaseposeMatrix.m[j] = invBasePoseMatrixPtr[j];
    }
    
    this->nodeList.push_back(node);
    this->nodeMatrixList.push_back(GLKMatrix4Identity);
  }
  
  // シーンに含まれるマテリアルの解析
  auto materialCount = this->fbxScene->GetMaterialCount();
  this->materialList.reserve(materialCount);
  printf("materialCount: %d\n", materialCount);
  for (int i = 0; i < materialCount; ++i)
  {
    auto fbxMaterial = this->fbxScene->GetMaterial(i);
    auto modelMaterial = this->ParseMaterial(fbxMaterial);
    this->materialList.push_back(modelMaterial);
    this->materialIdDictionary.insert({modelMaterial.materialName, i});
  }
  
  // シーンに含まれるメッシュの解析
  auto meshCount = this->fbxScene->GetMemberCount<FbxMesh>();
  this->meshList.reserve(meshCount);
  printf("meshCount: %d\n", meshCount);
  for (int i = 0; i < meshCount; ++i)
  {
    auto fbxMesh = this->fbxScene->GetMember<FbxMesh>(i);
    this->meshList.push_back(this->ParseMesh(fbxMesh));
  }
  
  
  return true;
}

void FBXLoader::Finalize()
{
  this->sdkManager->Destroy();
  
  this->meshList.clear();
  this->materialList.clear();
  this->materialIdDictionary.clear();
  this->nodeIdDictionary.clear();
  
}

bool FBXLoader::LoadAnimation(const char* filepath)
{
  auto importer = FbxImporter::Create(this->sdkManager, "");
  
  if (!importer->Initialize(filepath, -1, this->sdkManager->GetIOSettings()))
  {
    return false;
  }
  
  this->fbxSceneAnimation = FbxScene::Create(this->sdkManager, "animationScene");
  importer->Import(this->fbxSceneAnimation);
  
  // アニメーションフレーム数取得
  auto animStackCount = importer->GetAnimStackCount();
  assert(animStackCount == 1);
  auto takeInfo = importer->GetTakeInfo(0);
  
  auto importOffset = takeInfo->mImportOffset;
  auto startTime = takeInfo->mLocalTimeSpan.GetStart();
  auto stopTime = takeInfo->mLocalTimeSpan.GetStop();
  
  this->anim.startFrame = (importOffset.Get() + startTime.Get()) / FbxTime::GetOneFrameValue(FbxTime::eFrames60);
  this->anim.endFrame = (importOffset.Get() + stopTime.Get()) / FbxTime::GetOneFrameValue(FbxTime::eFrames60);
  importer->Destroy();
  
  auto animStack = this->fbxSceneAnimation->GetSrcObject<FbxAnimStack>();
  auto animLayerCount = animStack->GetMemberCount<FbxAnimLayer>();
  assert(animStackCount == 1);
  
  auto animLayer = animStack->GetMember<FbxAnimLayer>();
  
  FbxTime time;
  time.Set(FbxTime::GetOneFrameValue(FbxTime::eFrames60) * 2);
 
  // ノード名からノードIDを取得できるように辞書に登録
  auto nodeCount = this->fbxSceneAnimation->GetNodeCount();
  printf("animationNodeCount: %d\n", nodeCount);
  for (int i = 0; i < nodeCount; ++i)
  {
    auto fbxNode = this->fbxSceneAnimation->GetNode(i);
    
    ModelFCurve fCurve;
    fCurve.nodeName = fbxNode->GetName();
    
    fCurve.transXList = GetKeyList(fbxNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X), false);
    fCurve.transYList = GetKeyList(fbxNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y), false);
    fCurve.transZList = GetKeyList(fbxNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z), false);
    
    fCurve.rotateXList = GetKeyList(fbxNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X), true);
    fCurve.rotateYList = GetKeyList(fbxNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y), true);
    fCurve.rotateZList = GetKeyList(fbxNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z), true);
    
    fCurve.scaleXList = GetKeyList(fbxNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X), false);
    fCurve.scaleYList = GetKeyList(fbxNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y), false);
    fCurve.scaleZList = GetKeyList(fbxNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z), false);
    
    this->anim.fCurveList.push_back(fCurve);
    
    auto nodeId = this->nodeIdDictionary[fbxNode->GetName()];
    auto& node = this->nodeList[nodeId];
    node.animNodeId = i;
    
    auto localMatrix = fbxNode->EvaluateLocalTransform(time);
    auto localMatrixPtr = (double*)localMatrix;
    for (int j = 0; j < 16; ++j)
    {
      node.localMatrix.m[j] = localMatrixPtr[j];
    }
  }
  
  return true;
}

ModelMesh FBXLoader::ParseMesh(FbxMesh* mesh)
{
  assert(mesh != nullptr);
  
  auto node = mesh->GetNode();
  
  // マテリアルが１のものしか対応しない
  assert(node->GetMaterialCount() == 1);
  
  ModelMesh modelMesh;
  modelMesh.nodeName = node->GetName();
  modelMesh.materialName = node->GetMaterial(0)->GetName();
  
  printf(">> mesh: %s, %s\n", modelMesh.nodeName.c_str(), modelMesh.materialName.c_str());
  
  // インデックス取得
  auto indexList = GetIndexList(mesh);
  
  // 全て展開した状態の頂点取得
  auto positionList = GetPositionList(mesh, indexList);
  auto normalList = GetNormalList(mesh, indexList);
  auto uv0List = GetUVList(mesh, indexList, 0);
  
  std::vector<ModelBoneWeight> boneWeightList;
  GetWeight(mesh, indexList, boneWeightList, this->nodeIdDictionary);
  
  // 念のためサイズチェック
  assert(indexList.size() == positionList.size());
  assert(indexList.size() == normalList.size());
  assert(indexList.size() == uv0List.size());
  assert((indexList.size() == boneWeightList.size()) || (boneWeightList.size() == 0));
  
  // 頂点をインターリーブドアレイに
  std::vector<ModelVertex> modelVertexList;
  modelVertexList.reserve(indexList.size());
  
  auto meshIndex = this->nodeIdDictionary.at(modelMesh.nodeName);
  
  for (int i = 0; i < indexList.size(); ++i)
  {
    ModelVertex vertex;
    vertex.position = positionList[i];
    for (int j = 0; j < 3; ++j)
    {
      vertex.normal[j] = normalList[i].v[j] * std::numeric_limits<int16_t>::max();
    }
    
    if (uv0List.size() == 0)
    {
      vertex.uv0[0] = 0;
      vertex.uv0[1] = 0;
    }
    else
    {
      vertex.uv0[0] = uv0List[i].x * std::numeric_limits<int16_t>::max();
      vertex.uv0[1] = uv0List[i].y * std::numeric_limits<int16_t>::max();
    }
    
    if (boneWeightList.size() > 0)
    {
      for (int j = 0; j < 4; ++j)
      {
        vertex.boneIndex[j] = boneWeightList[i].boneIndex[j];
        vertex.boneWeight[j] = boneWeightList[i].boneWeight.v[j] * std::numeric_limits<uint8_t>::max();
      }
    }
    else
    {
      for (int j = 0; j < 4; ++j)
      {
        vertex.boneIndex[j] = 0;
        vertex.boneWeight[j] = 0;
      }
      vertex.boneWeight[0] = std::numeric_limits<uint8_t>::max();
    }
    
    //printf("weight[%d]: %f, %f, %f, %f\n", i, vertex.boneWeight.x, vertex.boneWeight.y, vertex.boneWeight.z, vertex.boneWeight.w);
    modelVertexList.push_back(vertex);
  }
  
  // この時点でglDrawArrays()で描画可能
  
  // 重複頂点を除いてインデックス作成
  auto& modelVertexListOpt = modelMesh.vertexList;
  modelVertexListOpt.reserve(modelVertexList.size());
  
  auto& modelIndexList = modelMesh.indexList;
  modelIndexList.reserve(indexList.size());
  
  for (auto& vertex : modelVertexList)
  {
    // 重複しているか？
    auto it = std::find(modelVertexListOpt.begin(), modelVertexListOpt.end(), vertex);
    if (it == modelVertexListOpt.end())
    {
      // 重複していない
      modelIndexList.push_back(modelVertexListOpt.size());
      modelVertexListOpt.push_back(vertex);
    }
    else
    {
      // 重複している
      auto index = std::distance(modelVertexListOpt.begin(), it);
      modelIndexList.push_back(index);
    }
  }
  
  // これでglDrawElements()で描画可能
  printf("Opt: %lu -> %lu\n", modelVertexList.size(), modelVertexListOpt.size());
  
  return modelMesh;
}

ModelMaterial FBXLoader::ParseMaterial(FbxSurfaceMaterial* material)
{
  ModelMaterial modelMaterial;
  modelMaterial.materialName = material->GetName();
  
  printf(">> material: %s\n", modelMaterial.materialName.c_str());
  
  // CGFXのみ対応
	auto implementation = GetImplementation(material, FBXSDK_IMPLEMENTATION_CGFX);
  assert(implementation != nullptr);
  
  auto rootTable = implementation->GetRootTable();
  auto entryCount = rootTable->GetEntryCount();
  
  for (int i = 0; i < entryCount; ++i)
  {
    auto entry = rootTable->GetEntry(i);
    
    auto fbxProperty = material->FindPropertyHierarchical(entry.GetSource());
    if (!fbxProperty.IsValid())
    {
      fbxProperty = material->RootProperty.FindHierarchical(entry.GetSource());
    }
    
    auto textureCount = fbxProperty.GetSrcObjectCount<FbxTexture>();
    if (textureCount > 0)
    {
      std::string src = entry.GetSource();
      
      for (int j = 0; j < fbxProperty.GetSrcObjectCount<FbxFileTexture>(); ++j)
      {
        auto tex = fbxProperty.GetSrcObject<FbxFileTexture>(j);
        std::string texName = tex->GetFileName();
        texName = texName.substr(texName.find_last_of('/') + 1);
        
        if (src == "Maya|DiffuseTexture")
        {
          modelMaterial.diffuseTextureName = texName;
        }
        else if (src == "Maya|NormalTexture")
        {
          modelMaterial.normalTextureName = texName;
        }
        else if (src == "Maya|SpecularTexture")
        {
          modelMaterial.specularTextureName = texName;
        }
        else if (src == "Maya|FalloffTexture")
        {
          modelMaterial.falloffTextureName = texName;
        }
        else if (src == "Maya|ReflectionMapTexture")
        {
          modelMaterial.reflectionMapTextureName = texName;
        }
      }
    }
  }
  
  printf("diffuseTexture: %s\n", modelMaterial.diffuseTextureName.c_str());
  printf("normalTexture: %s\n", modelMaterial.normalTextureName.c_str());
  printf("specularTexture: %s\n", modelMaterial.specularTextureName.c_str());
  printf("falloffTexture: %s\n", modelMaterial.falloffTextureName.c_str());
  printf("reflectionMapTexture: %s\n", modelMaterial.reflectionMapTextureName.c_str());
  
  return modelMaterial;
}

void FBXLoader::Update(float dt)
{
  for (auto& modelNode : this->nodeList)
  {
    auto& nodeMatrix = this->nodeMatrixList[modelNode.nodeId];
    
    if (modelNode.animNodeId >= 0)
    {
      auto& fCurve = this->anim.fCurveList[modelNode.animNodeId];
    }
    
//    GLKVector3 trans;
//    if (fCurve.transXList.size() > 1)
//    {
//      trans.x = fCurve.transXList[1].value;
//    }
//    if (fCurve.transYList.size() > 1)
//    {
//      trans.y = fCurve.transYList[1].value;
//    }
//    if (fCurve.transZList.size() > 1)
//    {
//      trans.z = fCurve.transZList[1].value;
//    }
    
    nodeMatrix = this->nodeList[modelNode.nodeId].localMatrix;
    
    if (modelNode.parentId >= 0)
    {
      auto& parentMatrix = this->nodeMatrixList[modelNode.parentId];
      //nodeMatrix = GLKMatrix4Multiply(nodeMatrix, parentMatrix);
      nodeMatrix = GLKMatrix4Multiply(parentMatrix, nodeMatrix);
    }
  }
  
  for (auto& modelNode : this->nodeList)
  {
    auto& nodeMatrix = this->nodeMatrixList[modelNode.nodeId];
    auto& invBaseposeMatrix = this->nodeList[modelNode.nodeId].invBaseposeMatrix;
    nodeMatrix = GLKMatrix4Multiply(nodeMatrix, invBaseposeMatrix);
  }
  
//  for (int i = 0; i < this->nodeMatrixList.size(); ++i)
//  {
//    this->nodeMatrixList[i] = GLKMatrix4Identity;
//  }
}

void FBXLoader::GetNodeMatrixList(GLKMatrix4* out_matrixList, int matrixCount) const
{
  assert(this->nodeMatrixList.size() < matrixCount);
  
  memcpy(out_matrixList, this->nodeMatrixList.data(), this->nodeMatrixList.size() * sizeof(GLKMatrix4));
  
}

