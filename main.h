#ifndef MAIN_H
#define MAIN_H

#include <cassert>
#include <cmath>

#include <string>
#include <iostream>
#include <sstream>

#include <algorithm>
#include <map>
#include <vector>
#include <list>

// OpenGL headers
#include <GL/GLee.h>
#include <GL/glu.h>

// SDL headers
#include <SDL2/SDL_image.h>

// Spitfire headers
#include <spitfire/spitfire.h>
#include <spitfire/util/timer.h>
#include <spitfire/util/thread.h>

#include <spitfire/math/math.h>
#include <spitfire/math/cVec2.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cVec4.h>
#include <spitfire/math/cMat4.h>
#include <spitfire/math/cQuaternion.h>
#include <spitfire/math/cColour.h>
#include <spitfire/math/geometry.h>

#include <spitfire/storage/file.h>
#include <spitfire/storage/filesystem.h>
#include <spitfire/util/log.h>

// libopenglmm headers
#include <libopenglmm/libopenglmm.h>
#include <libopenglmm/cContext.h>
#include <libopenglmm/cFont.h>
#include <libopenglmm/cGeometry.h>
#include <libopenglmm/cShader.h>
#include <libopenglmm/cSystem.h>
#include <libopenglmm/cTexture.h>
#include <libopenglmm/cVertexBufferObject.h>
#include <libopenglmm/cWindow.h>

// Application headers
#include "ai.h"
#include "astar.h"
#include "main.h"
#include "navigation.h"
#include "util.h"

struct KeyBoolPair {
  KeyBoolPair(unsigned int _key) : key(_key), bDown(false) {}

  void Process(unsigned int _key, bool _bDown) { if (_key == key) bDown = _bDown; }

  unsigned int key;
  bool bDown;
};



enum class TYPE {
  SOLDIER,
  BULLET,
  TREE,
};


struct Scene {
  spitfire::math::cColour skyColour;

  struct Objects {
    std::vector<spitfire::math::cVec3> positions;
    std::vector<spitfire::math::cQuaternion> rotations;
    std::vector<TYPE> types;
    std::map<size_t, aiagentid_t> aiagentids;
  } objects;
};


class cHeightmapData;


// ** cApplication

class cApplication : public opengl::cWindowEventListener, public opengl::cInputEventListener
{
public:
  cApplication();

  bool Create();
  void Destroy();

  void Run();

  opengl::cResolution GetResolution() const;

protected:
  // Called from cTronGlow, cHeatHaze, cHDR and cLensFlareDirt
  void CreateScreenRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fVBOWidth, float_t fVBOHeight, float_t fTextureWidth, float_t fTextureHeight);
  void RenderScreenRectangle(opengl::cTexture& texture, opengl::cShader& shader);
  void RenderScreenRectangle(opengl::cTexture& texture, opengl::cShader& shader, opengl::cStaticVertexBufferObject& staticVertexBufferObject);
  void RenderScreenRectangleShaderAndTextureAlreadySet(opengl::cStaticVertexBufferObject& staticVertexBufferObject);
  void RenderScreenRectangleShaderAndTextureAlreadySet();

private:
  void CreateScene();
  void CreateNavigationMesh();
  
  void CreateShaders();
  void DestroyShaders();

  void CreateHeightmapTriangles(opengl::cStaticVertexBufferObject& staticVertexBufferObject, const cHeightmapData& data, const spitfire::math::cVec3& scale);

  void CreateText();
  void CreateSquare(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates);
  void CreateCube(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates);
  void CreateSphere(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates, float fRadius);
  void CreateGear(opengl::cStaticVertexBufferObject& vbo);

  void CreateScreenRectVariableTextureSizeVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight);
  void CreateScreenRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight);
  void CreateScreenHalfRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight);
  void CreateGuiRectangle(opengl::cStaticVertexBufferObject& staticVertexBufferObject, size_t nTextureWidth, size_t nTextureHeight);

  void CreateNavigationMeshDebugShapes();
  void CreateNavigationMeshDebugWayPointLines();

  void RenderScreenRectangleDepthTexture(float x, float y, opengl::cStaticVertexBufferObject& vbo, const opengl::cTextureFrameBufferObject& texture, opengl::cShader& shader);
  void RenderScreenRectangle(float x, float y, opengl::cStaticVertexBufferObject& vbo, const opengl::cTexture& texture, opengl::cShader& shader);
  void RenderDebugScreenRectangleVariableSize(float x, float y, const opengl::cTexture& texture);

  void RenderFrame();

  void _OnWindowEvent(const opengl::cWindowEvent& event);
  void _OnMouseEvent(const opengl::cMouseEvent& event);
  void _OnKeyboardEvent(const opengl::cKeyboardEvent& event);

  std::vector<std::string> GetInputDescription() const;

  ssize_t CollideRayWithObjects(const spitfire::math::cRay3& ray) const;
  float CollideRayWithHeightmap(const spitfire::math::cRay3& ray) const;
  void HandleSelectionAndOrders(int mouseX, int mouseY);

  void AddRayCastLine(const spitfire::math::cLine3& line);
  void DebugAddRayCastBox(const spitfire::math::cAABB3& aabb);

  void DebugAddQuadtreeLines();
  void DebugAddQuadtreeLinesRecursive(const spitfire::math::cQuadtree<spitfire::math::cAABB2>& node);
  void CreateRayCastLineStaticVertexBuffer();

  void AddGreenDebugLine(const spitfire::math::cLine3& line);
  void CreateGreenDebugLinesStaticVertexBuffer();

  void CreateDebugTargetTraceLinesStaticVertexBuffer();

  float fFPS;

  bool bReloadShaders;
  bool bUpdateShaderConstants;

  KeyBoolPair moveCameraForward;
  KeyBoolPair moveCameraBack;
  KeyBoolPair moveCameraLeft;
  KeyBoolPair moveCameraRight;
  KeyBoolPair freeLookCamera;


  bool bIsPhysicsRunning;
  bool bIsWireframe;

  bool bIsDone;

  opengl::cSystem system;

  opengl::cResolution resolution;

  opengl::cWindow* pWindow;

  opengl::cContext* pContext;

  cFreeLookCamera camera;


  opengl::cFont font;
  opengl::cStaticVertexBufferObject textVBO;


  opengl::cTexture textureDiffuse;
  opengl::cTexture textureLightMap;
  opengl::cTexture textureDetail;

  opengl::cShader shaderHeightmap;

  opengl::cStaticVertexBufferObject staticVertexBufferObjectHeightmapTriangles;


  opengl::cStaticVertexBufferObject navigationMeshVBO;
  
  opengl::cShader shaderColour;

  opengl::cStaticVertexBufferObject staticVertexBufferObjectCube0;
  opengl::cStaticVertexBufferObject staticVertexBufferObjectSphere0;
  opengl::cStaticVertexBufferObject staticVertexBufferObjectGear0;
  

  opengl::cStaticVertexBufferObject staticVertexBufferObjectGuiRectangle;


  NavigationMesh navigationMesh;

  Scene scene;
  AISystem ai;

  opengl::cStaticVertexBufferObject staticVertexBufferDebugNavigationMesh;
  opengl::cStaticVertexBufferObject staticVertexBufferDebugNavigationMeshWayPointLines;
  opengl::cStaticVertexBufferObject staticVertexBufferGreenDebugTraceLines;
  opengl::cStaticVertexBufferObject staticVertexBufferDebugTargetTraceLines;

  ssize_t selectedObject;

  opengl::cStaticVertexBufferObject staticVertexBufferObjectRayCasts;
};

#endif // MAIN_H
