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
#include "astar.h"
#include "main.h"
#include "util.h"

struct KeyBoolPair {
  KeyBoolPair(unsigned int _key) : key(_key), bDown(false) {}

  void Process(unsigned int _key, bool _bDown) { if (_key == key) bDown = _bDown; }

  unsigned int key;
  bool bDown;
};




struct Node;

struct Edge {
  Node* from;
  Node* to;
  float cost;

  bool operator==(const Edge& rhs) const { return ((from == rhs.from) && (to == rhs.to) && spitfire::math::IsApproximatelyEqual(cost, rhs.cost)); }
};

typedef float cost_type; //typedef required, must be scalar type

struct Node {
  // Iterator for fetching connected nodes, and costs
  struct iterator {
    iterator(std::vector<Edge>::iterator _iter) : iter(_iter) {}

    //copy constructor and assignment operator should be available

    typedef float cost_type; //typedef required, must be scalar type

    // Node
    const Node& value() const { return *(iter->to); }

    // Cost/distance to node
    cost_type cost() const { return iter->cost; }

    // Next node
    iterator& operator++() { iter++; return *this; }

    // Used by search
    bool operator!=(const iterator& rhs) { return (iter != rhs.iter); }

  private:
    std::vector<Edge>::iterator iter;
  };

  // Get first, and past-end iterators
  iterator begin() { return iterator(edges.begin()); }
  iterator end() { return iterator(edges.end()); }

  // Equality operator, required
  // note: fuzzy equality may be useful
  bool operator==(const Node& rhs) const { return (position.IsApproximatelyEqual(rhs.position) && (edges == rhs.edges)); }

  spitfire::math::cVec3 position;

  std::vector<Edge> edges;

  size_t debug_identifier;
};


class NavigationMesh {
public:
  void SetNodesAndEdges(const std::vector<spitfire::math::cVec3>& nodePositions, const std::vector<std::pair<size_t, size_t>>& _edges);

  size_t GetNodeCount() const { return nodes.size(); }
  size_t GetEdgeCount() const { return edges.size(); }

  const Node& GetNode(size_t index) const { return nodes[index]; }
  Node& GetNode(size_t index) { return nodes[index]; }
  const Edge& GetEdge(size_t index) const { return edges[index]; }
  Edge& GetEdge(size_t index) { return edges[index]; }

private:
  std::vector<Node> nodes;
  std::vector<Edge> edges;
};


enum class TYPE {
  SOLDIER,
  BULLET,
  TREE,
};


struct Scene {
  spitfire::math::cColour skyColour;

  struct Objects {
    std::vector<spitfire::math::cMat4> translations;
    std::vector<spitfire::math::cQuaternion> rotations;
    std::vector<TYPE> types;
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
  void DebugAddOctreeLines();
  void DebugAddOctreeLinesRecursive(const spitfire::math::cOctree<spitfire::math::cAABB3>& node);
  void CreateRayCastLineStaticVertexBuffer();

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

  opengl::cStaticVertexBufferObject staticVertexBufferDebugNavigationMesh;
  opengl::cStaticVertexBufferObject staticVertexBufferDebugNavigationMeshWayPointLines;

  ssize_t selectedObject;

  std::vector<spitfire::math::cLine3> rayCastLines;
  opengl::cStaticVertexBufferObject staticVertexBufferObjectRayCasts;
};

#endif // MAIN_H
