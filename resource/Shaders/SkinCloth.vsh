//
//  Shader.vsh
//  MikuMikuTest
//
//  Created by ramemiso on 2014/03/21.
//  Copyright (c) 2014年 ramemiso. All rights reserved.
//

#version 300 es

#define ATTRIB_POSITION 0
#define ATTRIB_NORMAL 1
#define ATTRIB_MESH_INDEX 2
#define ATTRIB_TEXCOORD0 3
#define ATTRIB_BONE_INDEX 4
#define ATTRIB_BONE_WEIGHT 5

#define MAX_NODE_COUNT 170

layout (location = ATTRIB_POSITION) in vec4 vsPosition;
layout (location = ATTRIB_NORMAL) in vec3 vsNormal;
layout (location = ATTRIB_MESH_INDEX) in uint vsMeshIndex;
layout (location = ATTRIB_TEXCOORD0) in vec2 vsTexcoord0;
layout (location = ATTRIB_BONE_INDEX) in uvec4 vsBoneIndex;
layout (location = ATTRIB_BONE_WEIGHT) in vec4 vsBoneWeight;

out mediump vec3 light;
out mediump vec3 view;
out mediump vec3 normal;
out mediump vec2 texcoord0;

layout (std140) uniform UniformVs
{
  mat4 modelViewMatrix;
  mat4 projectionMatrix;
  mat4 normalMatrix;

  vec4 lightDirection;
  vec4 cameraPosition;
  
  mat4 nodeMatrixList[MAX_NODE_COUNT];
};

void main()
{
  mat4 boneMatrix;
  boneMatrix = nodeMatrixList[vsBoneIndex.x] * vsBoneWeight.x;
  boneMatrix += nodeMatrixList[vsBoneIndex.y] * vsBoneWeight.y;
  boneMatrix += nodeMatrixList[vsBoneIndex.z] * vsBoneWeight.z;
  boneMatrix += nodeMatrixList[vsBoneIndex.w] * vsBoneWeight.w;
  
  mat4 nodeMatrix = nodeMatrixList[vsMeshIndex] * boneMatrix;
  
  normal = normalize(mat3(normalMatrix) * mat3(nodeMatrix) * vsNormal);
  texcoord0 = vsTexcoord0;
  
  vec4 position = modelViewMatrix * nodeMatrix * vsPosition;
  gl_Position = projectionMatrix * position;
  
  light = lightDirection.xyz;
  view = normalize(cameraPosition.xyz - position.xyz);
}
