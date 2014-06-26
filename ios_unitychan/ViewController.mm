//
//  RMViewController.m
//  MikuMikuTest
//
//  Created by ramemiso on 2014/03/21.
//  Copyright (c) 2014年 ramemiso. All rights reserved.
//

#import "ViewController.h"
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#include "FBXLoader.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// Uniform index.
enum
{
  UNIFORM_VS,
  UNIFORM_DIFFUSE_TEXTURE,
  UNIFORM_FALLOFF_TEXTURE,
  UNIFORM_RIMLIGHT_TEXTURE,
  UNIFORM_SPECULAR_TEXTURE,
  NUM_UNIFORMS
};
GLint uniformsSkin[NUM_UNIFORMS];
GLint uniformsCloth[NUM_UNIFORMS];

// Attribute index.
enum
{
  ATTRIB_VERTEX,
  ATTRIB_NORMAL,
  ATTRIB_TEXCOORD0,
  ATTRIB_BONE_INDEX,
  ATTRIB_BONE_WEIGHT,
  NUM_ATTRIBUTES
};

struct AppMaterial
{
  GLuint diffuseTexture;
  GLuint falloffTexture;
  GLuint specularTexture;
	GLuint rimlightTexture;
  GLuint repeatSampler;
  GLuint clampSampler;
  
  bool diffuseHasAlpha;
  
  const ModelMaterial* modelMaterial;
};

struct AppMesh
{
  GLuint vertexArray;
  GLuint vertexBuffer;
  GLuint indexBuffer;
  
  const AppMaterial* material;
  const ModelMesh* modelMesh;
  
  int modelMeshId;
};

#define MAX_NODE_COUNT 170
struct UniformVs
{
  GLKMatrix4 modelViewMatrix;
  GLKMatrix4 projectionMatrix;
  GLKMatrix4 normalMatrix;
 
  GLKVector4 lightDirection;
  GLKVector4 cameraPosition;
  
  GLKMatrix4 nodeMatrixList[MAX_NODE_COUNT];
};

@interface ViewController () {
  GLuint _programSkin;
  GLuint _programCloth;
  GLuint _uniformBufferVs;
  
  float _rotation;
  UniformVs _uniformVs;
  
  FBXLoader _fbxLoader;
  std::map<std::string, GLuint> _textureDictionary;
  std::vector<AppMaterial> _materialList;
  std::vector<AppMesh> _opacityMeshList;
  std::vector<AppMesh> _transparencyMeshList;
}
@property (strong, nonatomic) EAGLContext *context;

- (void)setupGL;
- (void)tearDownGL;

- (GLuint)loadShaders:(NSString*) vertex fragment:(NSString*)fragment;
- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file;
- (BOOL)linkProgram:(GLuint)prog;
- (BOOL)validateProgram:(GLuint)prog;
@end

@implementation ViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  
  if (!self.context) {
    NSLog(@"Failed to create ES context");
  }
  self.preferredFramesPerSecond = 60;
  
  GLKView *view = (GLKView *)self.view;
  view.context = self.context;
  view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
  view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
  view.drawableStencilFormat = GLKViewDrawableStencilFormat8;
  
  //  view.drawableMultisample = GLKViewDrawableMultisample4X;
  [self setupGL];
}

- (void)dealloc
{
  [self tearDownGL];
  
  if ([EAGLContext currentContext] == self.context) {
    [EAGLContext setCurrentContext:nil];
  }
}

- (void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
  
  if ([self isViewLoaded] && ([[self view] window] == nil)) {
    self.view = nil;
    
    [self tearDownGL];
    
    if ([EAGLContext currentContext] == self.context) {
      [EAGLContext setCurrentContext:nil];
    }
    self.context = nil;
  }
  
  // Dispose of any resources that can be recreated.
}

- (GLuint)loadTexture:(const std::string&) name alpha:(bool*)alpha
{
  if (name.size() == 0)
  {
    return 0;
  }
  
  auto it = _textureDictionary.find(name);
  if (it != _textureDictionary.end())
  {
    return it->second;
  }
  
  auto basePath = @"resource/Textures/";
  auto difPath = [NSString stringWithUTF8String:name.c_str()];
  auto resPath = [basePath stringByAppendingString:difPath];
  resPath = [resPath stringByDeletingPathExtension];
  
  auto texPath = [[NSBundle mainBundle] pathForResource:resPath ofType:@"tga"];
  auto options = @{
                   GLKTextureLoaderGenerateMipmaps: @YES,
                   GLKTextureLoaderOriginBottomLeft: @YES,
                   };
  auto tex = [GLKTextureLoader textureWithContentsOfFile:texPath options:options error:nil];
  _textureDictionary.insert({name, tex.name});
  
  if (alpha != nullptr)
  {
    *alpha = (tex.alphaState != GLKTextureInfoAlphaStateNone);
  }
  
  return tex.name;
}

- (void)setupGL
{
  [EAGLContext setCurrentContext:self.context];
  
  _programSkin = [self loadShaders:@"SkinCloth" fragment:@"Skin"];
  uniformsSkin[UNIFORM_VS] = glGetUniformBlockIndex(_programSkin, "UniformVs");
  uniformsSkin[UNIFORM_DIFFUSE_TEXTURE] = glGetUniformLocation(_programSkin, "diffuseTexture");
  uniformsSkin[UNIFORM_FALLOFF_TEXTURE] = glGetUniformLocation(_programSkin, "falloffTexture");
  uniformsSkin[UNIFORM_RIMLIGHT_TEXTURE] = glGetUniformLocation(_programSkin, "rimlightTexture");
  uniformsSkin[UNIFORM_SPECULAR_TEXTURE] = -1;
  
  _programCloth = [self loadShaders:@"SkinCloth" fragment:@"Cloth"];
  uniformsCloth[UNIFORM_VS] = glGetUniformBlockIndex(_programCloth, "UniformVs");
  uniformsCloth[UNIFORM_DIFFUSE_TEXTURE] = glGetUniformLocation(_programCloth, "diffuseTexture");
  uniformsCloth[UNIFORM_FALLOFF_TEXTURE] = glGetUniformLocation(_programCloth, "falloffTexture");
  uniformsCloth[UNIFORM_RIMLIGHT_TEXTURE] = glGetUniformLocation(_programCloth, "rimlightTexture");
  uniformsCloth[UNIFORM_SPECULAR_TEXTURE] = glGetUniformLocation(_programCloth, "specularTexture");
  
  glGenBuffers(1, &_uniformBufferVs);
  
  glEnable(GL_DEPTH_TEST);
  
  auto fbxPath = [[NSBundle mainBundle] pathForResource:@"resource/unitychan" ofType:@"fbx"];
  _fbxLoader.Initialize(fbxPath.UTF8String);
  
  auto fbxAnimationPath = [[NSBundle mainBundle] pathForResource:@"resource/unitychan_WAIT04" ofType:@"fbx"];
  _fbxLoader.LoadAnimation(fbxAnimationPath.UTF8String);
  
  for (int i = 0; i < _fbxLoader.GetMaterialCount(); ++i)
  {
    auto& modelMaterial = _fbxLoader.GetMaterial(i);
    
    AppMaterial material;
    material.modelMaterial = &modelMaterial;
    
    // テクスチャ読み込み
    material.diffuseTexture = [self loadTexture:modelMaterial.diffuseTextureName alpha:&material.diffuseHasAlpha];
    material.falloffTexture = [self loadTexture:modelMaterial.falloffTextureName alpha:nullptr];
    material.specularTexture = [self loadTexture:modelMaterial.specularTextureName alpha:nullptr];
    material.rimlightTexture = [self loadTexture:"FO_RIM1.tga" alpha:nullptr];
    
    // リピート用サンプラー
    glGenSamplers(1, &material.repeatSampler);
    glSamplerParameteri(material.repeatSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(material.repeatSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(material.repeatSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(material.repeatSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // クランプ用サンプラー
    glGenSamplers(1, &material.clampSampler);
    glSamplerParameteri(material.clampSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(material.clampSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(material.clampSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(material.clampSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    _materialList.push_back(material);
  }
  
  for (int i = 0; i < _fbxLoader.GetMeshCount(); ++i)
  {
    auto& modelMesh = _fbxLoader.GetMesh(i);
    auto& modelVertexList = modelMesh.vertexList;
    auto& modelIndexList = modelMesh.indexList;
    
    AppMesh mesh;
    mesh.modelMeshId = i;
    mesh.modelMesh = &modelMesh;
    mesh.material = &_materialList[_fbxLoader.GetMaterialId(modelMesh.materialName)];
  
    glGenVertexArrays(1, &mesh.vertexArray);
    glBindVertexArray(mesh.vertexArray);
    
    glGenBuffers(1, &mesh.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(modelVertexList[0]) * modelVertexList.size(), modelVertexList.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_VERTEX, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), BUFFER_OFFSET(offsetof(ModelVertex, position)));
    glEnableVertexAttribArray(ATTRIB_NORMAL);
    glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_SHORT, GL_TRUE, sizeof(ModelVertex), BUFFER_OFFSET(offsetof(ModelVertex, normal)));
    glEnableVertexAttribArray(ATTRIB_TEXCOORD0);
    glVertexAttribPointer(ATTRIB_TEXCOORD0, 2, GL_SHORT, GL_TRUE, sizeof(ModelVertex), BUFFER_OFFSET(offsetof(ModelVertex, uv0)));
    glEnableVertexAttribArray(ATTRIB_BONE_INDEX);
    glVertexAttribIPointer(ATTRIB_BONE_INDEX, 4, GL_UNSIGNED_BYTE, sizeof(ModelVertex), BUFFER_OFFSET(offsetof(ModelVertex, boneIndex)));
    glEnableVertexAttribArray(ATTRIB_BONE_WEIGHT);
    glVertexAttribPointer(ATTRIB_BONE_WEIGHT, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ModelVertex), BUFFER_OFFSET(offsetof(ModelVertex, boneWeight)));
    
    glGenBuffers(1, &mesh.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(modelIndexList[0]) * modelIndexList.size(), modelIndexList.data(), GL_STATIC_DRAW);
    
    glBindVertexArray(0);
    
    
    if (mesh.material->diffuseHasAlpha)
    {
      _transparencyMeshList.push_back(mesh);
    }
    else
    {
      _opacityMeshList.push_back(mesh);
    }
  }
  
}

- (void)tearDownGL
{
  [EAGLContext setCurrentContext:self.context];
  
  glDeleteProgram(_programSkin);
  glDeleteProgram(_programCloth);
  glDeleteBuffers(1, &_uniformBufferVs);
  
  for (auto& pair : _textureDictionary)
  {
    glDeleteTextures(1, &pair.second);
  }
  for (auto& material : _materialList)
  {
    glDeleteSamplers(1, &material.clampSampler);
    glDeleteSamplers(1, &material.repeatSampler);
  }
  for (auto& mesh : _opacityMeshList)
  {
    glDeleteBuffers(1, &mesh.indexBuffer);
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteVertexArrays(1, &mesh.vertexArray);
  }
  for (auto& mesh : _transparencyMeshList)
  {
    glDeleteBuffers(1, &mesh.indexBuffer);
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteVertexArrays(1, &mesh.vertexArray);
  }
  
  _fbxLoader.Finalize();
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
  _uniformVs.lightDirection = GLKVector4Make(300.0f, 500.0f, 1000.0f, 0.0f);
  _uniformVs.lightDirection = GLKVector4Normalize(_uniformVs.lightDirection);
  _uniformVs.cameraPosition = GLKVector4Make(0.0f, 150.0f, 250.0f, 0.0f);
  float aspect = fabsf(self.view.bounds.size.width / self.view.bounds.size.height);
  GLKMatrix4 projectionMatrix = GLKMatrix4MakePerspective(GLKMathDegreesToRadians(40.0f), aspect, 1.0f, 1000.0f);
  
  GLKMatrix4 viewMatrix = GLKMatrix4MakeLookAt(_uniformVs.cameraPosition.x, _uniformVs.cameraPosition.y, _uniformVs.cameraPosition.z,
                                               0.0f, 100.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f);
  
  GLKMatrix4 modelMatrix = GLKMatrix4MakeTranslation(0.0f, 0.0f, 0.0f);
  //modelMatrix = GLKMatrix4Rotate(modelMatrix, _rotation, 0.0f, 1.0f, 0.0f);
  modelMatrix = GLKMatrix4Rotate(modelMatrix, GLKMathDegreesToRadians(-30), 0.0f, 1.0f, 0.0f);
  
  
  _uniformVs.normalMatrix = GLKMatrix4InvertAndTranspose(modelMatrix, NULL);
  _uniformVs.modelViewMatrix = GLKMatrix4Multiply(viewMatrix, modelMatrix);
  _uniformVs.projectionMatrix = projectionMatrix;
  
  
  _rotation += self.timeSinceLastUpdate * 0.5f;
  
  _fbxLoader.Update(self.timeSinceLastUpdate * 60.0f);
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
  glClearColor(0.65f, 0.65f, 0.65f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  // ノード行列取得
  _fbxLoader.GetNodeMatrixList(_uniformVs.nodeMatrixList, MAX_NODE_COUNT);
  
  // ユニフォームバッファにコピー
  glBindBuffer(GL_UNIFORM_BUFFER, _uniformBufferVs);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(_uniformVs), &_uniformVs, GL_DYNAMIC_DRAW);
  
  auto drawFunc = [self](const std::vector<AppMesh>& meshList)
  {
    for (int i = 0; i < meshList.size(); ++i)
    {
      auto& mesh = meshList[i];
      
      GLuint program = 0;
      GLint* uniforms = nullptr;
      
      
      // スペキュラが無ければスキン
      if (mesh.material->specularTexture == 0)
      {
        program = _programSkin;
        uniforms = uniformsSkin;
      }
      else
      {
        program = _programCloth;
        uniforms = uniformsCloth;
      }
      
      glUseProgram(program);
      glBindBufferBase(GL_UNIFORM_BUFFER, uniforms[UNIFORM_VS], _uniformBufferVs);
      
      glBindVertexArray(mesh.vertexArray);
      
      // diffuse
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, mesh.material->diffuseTexture);
      glBindSampler(0, mesh.material->repeatSampler);
      glUniform1i(uniforms[UNIFORM_DIFFUSE_TEXTURE], 0);
      
      // falloff
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, mesh.material->falloffTexture);
      glBindSampler(1, mesh.material->clampSampler);
      glUniform1i(uniforms[UNIFORM_FALLOFF_TEXTURE], 1);
      
      // rimlight
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, mesh.material->rimlightTexture);
      glBindSampler(2, mesh.material->clampSampler);
      glUniform1i(uniforms[UNIFORM_RIMLIGHT_TEXTURE], 2);
      
      // specular
      if (mesh.material->specularTexture != 0)
      {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mesh.material->specularTexture);
        glBindSampler(3, mesh.material->repeatSampler);
        glUniform1i(uniforms[UNIFORM_SPECULAR_TEXTURE], 3);
      }
      
      glDrawElements(GL_TRIANGLES, (GLsizei)mesh.modelMesh->indexList.size(), GL_UNSIGNED_SHORT, nullptr);
    }
  };
  
  // 不透明のメッシュを描画
  glDisable(GL_BLEND);
  drawFunc(_opacityMeshList);
  
  // 半透明のメッシュを描画
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  drawFunc(_transparencyMeshList);
}

#pragma mark -  OpenGL ES 2 shader compilation

- (GLuint)loadShaders:(NSString*) vertex fragment:(NSString*)fragment;
{
  GLuint vertShader, fragShader;
  NSString *vertShaderPathname, *fragShaderPathname;
  
  // Create shader program.
  auto program = glCreateProgram();
  
  // Create and compile vertex shader.
  auto vertexShaderPath = [@"resource/Shaders/" stringByAppendingString:vertex];
  vertShaderPathname = [[NSBundle mainBundle] pathForResource:vertexShaderPath ofType:@"vsh"];
  if (![self compileShader:&vertShader type:GL_VERTEX_SHADER file:vertShaderPathname]) {
    NSLog(@"Failed to compile vertex shader");
    return NO;
  }
  
  // Create and compile fragment shader.
  auto fragmentShaderPath = [@"resource/Shaders/" stringByAppendingString:fragment];
  fragShaderPathname = [[NSBundle mainBundle] pathForResource:fragmentShaderPath ofType:@"fsh"];
  if (![self compileShader:&fragShader type:GL_FRAGMENT_SHADER file:fragShaderPathname]) {
    NSLog(@"Failed to compile fragment shader");
    return NO;
  }
  
  // Attach vertex shader to program.
  glAttachShader(program, vertShader);
  
  // Attach fragment shader to program.
  glAttachShader(program, fragShader);
  
  // Link program.
  if (![self linkProgram:program]) {
    NSLog(@"Failed to link program: %d", program);
    
    if (vertShader) {
      glDeleteShader(vertShader);
      vertShader = 0;
    }
    if (fragShader) {
      glDeleteShader(fragShader);
      fragShader = 0;
    }
    if (program) {
      glDeleteProgram(program);
      program = 0;
    }
    
    return 0;
  }
  
  
  // Release vertex and fragment shaders.
  if (vertShader) {
    glDetachShader(program, vertShader);
    glDeleteShader(vertShader);
  }
  if (fragShader) {
    glDetachShader(program, fragShader);
    glDeleteShader(fragShader);
  }
  
  return program;
}

- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type file:(NSString *)file
{
  GLint status;
  const GLchar *source;
  
  source = (GLchar *)[[NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil] UTF8String];
  if (!source) {
    NSLog(@"Failed to load vertex shader");
    return NO;
  }
  
  *shader = glCreateShader(type);
  glShaderSource(*shader, 1, &source, NULL);
  glCompileShader(*shader);
  
#if defined(DEBUG)
  GLint logLength;
  glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetShaderInfoLog(*shader, logLength, &logLength, log);
    NSLog(@"Shader compile log:\n%s", log);
    free(log);
  }
#endif
  
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
  if (status == 0) {
    glDeleteShader(*shader);
    return NO;
  }
  
  return YES;
}

- (BOOL)linkProgram:(GLuint)prog
{
  GLint status;
  glLinkProgram(prog);
  
#if defined(DEBUG)
  GLint logLength;
  glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetProgramInfoLog(prog, logLength, &logLength, log);
    NSLog(@"Program link log:\n%s", log);
    free(log);
  }
#endif
  
  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (status == 0) {
    return NO;
  }
  
  return YES;
}

- (BOOL)validateProgram:(GLuint)prog
{
  GLint logLength, status;
  
  glValidateProgram(prog);
  glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
  if (logLength > 0) {
    GLchar *log = (GLchar *)malloc(logLength);
    glGetProgramInfoLog(prog, logLength, &logLength, log);
    NSLog(@"Program validate log:\n%s", log);
    free(log);
  }
  
  glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
  if (status == 0) {
    return NO;
  }
  
  return YES;
}

@end
