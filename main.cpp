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

#include <spitfire/math/math.h>
#include <spitfire/math/cFrustum.h>
#include <spitfire/math/cVec2.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cVec4.h>
#include <spitfire/math/cMat4.h>
#include <spitfire/math/cOctree.h>
#include <spitfire/math/cQuaternion.h>
#include <spitfire/math/cColour.h>
#include <spitfire/math/geometry.h>

#include <spitfire/storage/file.h>
#include <spitfire/storage/filesystem.h>
#include <spitfire/util/log.h>

// Breathe headers
#include <breathe/render/model/cFileFormatOBJ.h>
#include <breathe/render/model/cStatic.h>

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
#include "heightmap.h"
#include "main.h"
#include "navigation.h"

/// \brief This class is a derivate of basic_stringbuf which will output all the written data using the OutputDebugString function
template<typename TChar, typename TTraits = std::char_traits<TChar>>
class OutputDebugStringBuf : public std::basic_stringbuf<TChar,TTraits> {
public:
  explicit OutputDebugStringBuf() : _buffer(256) {
    setg(nullptr, nullptr, nullptr);
    setp(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
  }

  ~OutputDebugStringBuf() {
  }

  static_assert(std::is_same<TChar,char>::value || std::is_same<TChar,wchar_t>::value, "OutputDebugStringBuf only supports char and wchar_t types");

  int sync() try {
    MessageOutputer<TChar,TTraits>()(pbase(), pptr());
    setp(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
    return 0;
  } catch(...) {
    return -1;
  }

  int_type overflow(int_type c = TTraits::eof()) {
    auto syncRet = sync();
    if (c != TTraits::eof()) {
      _buffer[0] = c;
      setp(_buffer.data(), _buffer.data() + 1, _buffer.data() + _buffer.size());
    }
    return syncRet == -1 ? TTraits::eof() : 0;
  }


private:
  std::vector<TChar>      _buffer;

  template<typename TChar, typename TTraits>
  struct MessageOutputer;

  template<>
  struct MessageOutputer<char,std::char_traits<char>> {
    template<typename TIterator>
    void operator()(TIterator begin, TIterator end) const {
      std::string s(begin, end);
      OutputDebugStringA(s.c_str());
    }
  };

  template<>
  struct MessageOutputer<wchar_t,std::char_traits<wchar_t>> {
    template<typename TIterator>
    void operator()(TIterator begin, TIterator end) const {
      std::wstring s(begin, end);
      OutputDebugStringW(s.c_str());
    }
  };
};

void RedirectStandardOutputToOutputWindow()
{
#ifndef NDEBUG
#ifdef _WIN32
  if (IsDebuggerPresent()) {
    static OutputDebugStringBuf<char> charDebugOutput;
    std::cout.rdbuf(&charDebugOutput);
    std::cerr.rdbuf(&charDebugOutput);
    std::clog.rdbuf(&charDebugOutput);

    static OutputDebugStringBuf<wchar_t> wcharDebugOutput;
    std::wcout.rdbuf(&wcharDebugOutput);
    std::wcerr.rdbuf(&wcharDebugOutput);
    std::wclog.rdbuf(&wcharDebugOutput);
  }
#endif
#endif
}


struct iVec4 {
  int entries[4];
};

spitfire::math::cVec3 glmUnProject(
  spitfire::math::cVec3 const & win,
  spitfire::math::cMat4 const& model,
  spitfire::math::cMat4 const& proj,
  iVec4 const & viewport
)
{
  const spitfire::math::cMat4 Inverse = (proj * model).GetInverse();

  spitfire::math::cVec4 tmp = spitfire::math::cVec4(win.x, win.y, win.z, 1.0f);
  tmp.x = (tmp.x - float(viewport.entries[0])) / float(viewport.entries[2]);
  tmp.y = (tmp.y - float(viewport.entries[1])) / float(viewport.entries[3]);
//#		if GLM_DEPTH_CLIP_SPACE == GLM_DEPTH_ZERO_TO_ONE
#if 1
  tmp.x = tmp.x * 2.0f - 1.0f;
  tmp.y = tmp.y * 2.0f - 1.0f;
#		else
  tmp = tmp * 2.0f - 1.0f;
#		endif

  spitfire::math::cVec4 obj = Inverse * tmp;
  obj /= obj.w;

  return spitfire::math::cVec3(obj.x, obj.y, obj.z);
}

void CameraRayCast(const spitfire::math::cMat4& matProjection, const spitfire::math::cMat4& matView, int mouseX, int mouseY, spitfire::math::cVec3& origin, spitfire::math::cVec3& direction)
{
  iVec4 viewport;
  glGetIntegerv(GL_VIEWPORT, viewport.entries);
  
  // Invert the y coordinate
  const spitfire::math::cVec2 mouse(mouseX, viewport.entries[3] - mouseY);

  // Get a 3D position for a point on the screen
  const spitfire::math::cVec3 pointOnScreen(mouse.x, mouse.y, 0.0f);
  origin = glmUnProject(pointOnScreen, matView, matProjection, viewport);

  // Get a 3D position for a point a small distance from the screen
  const spitfire::math::cVec3 pointGoingIntoScreen(pointOnScreen + spitfire::math::cVec3(0.0f, 0.0f, 1.0f));
  const spitfire::math::cVec3 destination = glmUnProject(pointGoingIntoScreen, matView, matProjection, viewport);

  // The direction is the normal pointing from the origin to the destination
  direction = (destination - origin).GetNormalised();
}



cHeightmapData heightMapData;

spitfire::math::cVec3 heightMapScale;

std::unique_ptr<spitfire::math::cOctree<spitfire::math::cAABB3>> octree;

void DebugPrintOctreeRecursive(const spitfire::math::cOctree<spitfire::math::cAABB3>& _octree, const std::string& indentation)
{
  const spitfire::math::cVec3 min = _octree.GetMin();
  const spitfire::math::cVec3 max = _octree.GetMax();

  std::cout << indentation << "Node " << max.x - min.x << std::endl;

  if (_octree.GetData() != nullptr) {
    std::cout << indentation << "  Data" << std::endl;

    assert(_octree.GetChild(0) == nullptr);
    assert(_octree.GetChild(1) == nullptr);
    assert(_octree.GetChild(2) == nullptr);
    assert(_octree.GetChild(3) == nullptr);
    assert(_octree.GetChild(4) == nullptr);
    assert(_octree.GetChild(5) == nullptr);
    assert(_octree.GetChild(6) == nullptr);
    assert(_octree.GetChild(7) == nullptr);
  }

  for (size_t i = 0; i < 8; i++) {
    const spitfire::math::cOctree<spitfire::math::cAABB3>* child = _octree.GetChild(i);
    if (child != nullptr) {
      DebugPrintOctreeRecursive(*child, indentation + "  ");
    }
  }
}

void BuildOctree()
{
  // TODO: Get the real height based on the values in the heightmap
  const float fLowestPoint = heightMapData.GetLowestPoint();
  const float fHighestPoint = heightMapData.GetHighestPoint();

  const spitfire::math::cVec3 halfDimension = 0.5f * heightMapScale * spitfire::math::cVec3(heightMapData.GetWidth(), fHighestPoint - fLowestPoint, heightMapData.GetDepth());
  const spitfire::math::cVec3 origin = halfDimension;
  octree.reset(new spitfire::math::cOctree<spitfire::math::cAABB3>(origin, halfDimension));

  const size_t width = heightMapData.GetWidth();
  const size_t depth = heightMapData.GetDepth();
  for (size_t z = 0; z < depth; z++) {
    for (size_t x = 0; x < width; x++) {
      spitfire::math::cAABB3* aabb = new spitfire::math::cAABB3;
      const spitfire::math::cVec3 point0 = heightMapScale * spitfire::math::cVec3(float(x), 0.0f, float(z));
      const spitfire::math::cVec3 point1 = heightMapScale * spitfire::math::cVec3(float(x + 1), 1.0f, float(z + 1));
      aabb->SetExtents(point0, point1);
      octree->insert(aabb);
    }
  }

  DebugPrintOctreeRecursive(*octree, "");
}


// ** cApplication

cApplication::cApplication() :
  fFPS(0.0f),

  bReloadShaders(false),
  bUpdateShaderConstants(true),

  moveCameraForward(SDLK_w),
  moveCameraBack(SDLK_s),
  moveCameraLeft(SDLK_a),
  moveCameraRight(SDLK_d),
  freeLookCamera(SDLK_LSHIFT),

  bIsPhysicsRunning(true),
  bIsWireframe(false),

  bIsDone(false),

  pWindow(nullptr),
  pContext(nullptr),

  selectedObject(-1)
{
  // Set our main thread
  spitfire::util::SetMainThread();

  // Set up our time variables
  spitfire::util::TimeInit();
}

opengl::cResolution cApplication::GetResolution() const
{
  return resolution;
}

void cApplication::CreateText()
{
  assert(font.IsValid());

  // Destroy any existing VBO
  pContext->DestroyStaticVertexBufferObject(textVBO);

  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  opengl::cGeometryBuilder_v2_c4_t2 builder(*pGeometryDataPtr);

  std::list<spitfire::string_t> lines;
  lines.push_back(spitfire::string_t(TEXT("FPS: ")) + spitfire::string::ToString(int(fFPS)));
  lines.push_back(TEXT(""));

  lines.push_back(spitfire::string_t(TEXT("Physics running: ")) + (bIsPhysicsRunning ? TEXT("On") : TEXT("Off")));
  lines.push_back(spitfire::string_t(TEXT("Wireframe: ")) + (bIsWireframe ? TEXT("On") : TEXT("Off")));
  lines.push_back(TEXT(""));

  lines.push_back(spitfire::string_t(TEXT("Selected: ")) + spitfire::string::ToString(selectedObject));
  lines.push_back(spitfire::string_t(TEXT("Camera: ")) + spitfire::string::ToString(camera.GetPosition().x) + TEXT(", ") + spitfire::string::ToString(camera.GetPosition().y) + TEXT(", ") + spitfire::string::ToString(camera.GetPosition().z));
  if (selectedObject >= 0) {
    const spitfire::math::cVec3 position = scene.objects.translations[selectedObject].GetTranslation();
    lines.push_back(spitfire::string_t(TEXT("Object: ")) + spitfire::string::ToString(position.x) + TEXT(", ") + spitfire::string::ToString(position.y) + TEXT(", ") + spitfire::string::ToString(position.z));
  }
  lines.push_back(TEXT(""));


  // Add our lines of text
  const spitfire::math::cColour red(1.0f, 0.0f, 0.0f);
  float y = 0.0f;
  std::list<spitfire::string_t>::const_iterator iter(lines.begin());
  const std::list<spitfire::string_t>::const_iterator iterEnd(lines.end());
  while (iter != iterEnd) {
    font.PushBack(builder, *iter, red, spitfire::math::cVec2(0.0f, y));
    y += 0.04f;

    iter++;
  }

  textVBO.SetData(pGeometryDataPtr);

  textVBO.Compile2D();
}

void cApplication::CreateSquare(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fWidth = 2.0f;

  opengl::cGeometryBuilder builder;
  builder.CreatePlane(fWidth, fWidth, *pGeometryDataPtr, nTextureCoordinates);

  vbo.SetData(pGeometryDataPtr);

  vbo.Compile();
}

void cApplication::CreateCube(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fWidth = 1.0f;

  opengl::cGeometryBuilder builder;
  builder.CreateCube(fWidth, *pGeometryDataPtr, nTextureCoordinates);

  vbo.SetData(pGeometryDataPtr);

  vbo.Compile();
}

void cApplication::CreateSphere(opengl::cStaticVertexBufferObject& vbo, size_t nTextureCoordinates, float fRadius)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const size_t nSegments = 30;

  opengl::cGeometryBuilder builder;
  builder.CreateSphere(fRadius, nSegments, *pGeometryDataPtr, nTextureCoordinates);

  vbo.SetData(pGeometryDataPtr);

  vbo.Compile();
}

void cApplication::CreateGear(opengl::cStaticVertexBufferObject& vbo)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fInnerRadius = 0.25f;
  const float fOuterRadius = 1.0f;
  const float fWidth = 1.0f;
  const size_t nTeeth = 8;
  const float fToothDepth = 0.3f;

  opengl::cGeometryBuilder builder;
  builder.CreateGear(fInnerRadius, fOuterRadius, fWidth, nTeeth, fToothDepth, *pGeometryDataPtr);

  vbo.SetData(pGeometryDataPtr);

  vbo.Compile();
}

void cApplication::CreateScreenRectVariableTextureSizeVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight)
{
  const float fTextureWidth = 1.0f;
  const float fTextureHeight = 1.0f;

  CreateScreenRectVBO(staticVertexBufferObject, fWidth, fHeight, fTextureWidth, fTextureHeight);
}

void cApplication::CreateScreenRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight)
{
  const float fTextureWidth = float(resolution.width);
  const float fTextureHeight = float(resolution.height);

  CreateScreenRectVBO(staticVertexBufferObject, fWidth, fHeight, fTextureWidth, fTextureHeight);
}

void cApplication::CreateScreenRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fVBOWidth, float_t fVBOHeight, float_t fTextureWidth, float_t fTextureHeight)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float_t fHalfWidth = fVBOWidth * 0.5f;
  const float_t fHalfHeight = fVBOHeight * 0.5f;
  const spitfire::math::cVec2 vMin(-fHalfWidth, -fHalfHeight);
  const spitfire::math::cVec2 vMax(fHalfWidth, fHalfHeight);

  opengl::cGeometryBuilder_v2_t2 builder(*pGeometryDataPtr);

  // Front facing rectangle
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMin.y), spitfire::math::cVec2(fTextureWidth, fTextureHeight));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMax.y), spitfire::math::cVec2(0.0f, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMax.y), spitfire::math::cVec2(fTextureWidth, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMin.y), spitfire::math::cVec2(0.0f, fTextureHeight));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMax.y), spitfire::math::cVec2(0.0f, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMin.y), spitfire::math::cVec2(fTextureWidth, fTextureHeight));

  staticVertexBufferObject.SetData(pGeometryDataPtr);

  staticVertexBufferObject.Compile2D();
}

void cApplication::CreateGuiRectangle(opengl::cStaticVertexBufferObject& vbo, size_t nTextureWidth, size_t nTextureHeight)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fTextureWidth = float(nTextureWidth);
  const float fTextureHeight = float(nTextureHeight);

  const float fRatioWidthOverHeight = (fTextureWidth / fTextureHeight);

  const float fWidth = 1.0f;
  const float fHeight = fWidth / fRatioWidthOverHeight;

  const float_t fHalfWidth = fWidth * 0.5f;
  const float_t fHalfHeight = fHeight * 0.5f;
  const spitfire::math::cVec3 vMin(-fHalfWidth, 0.0f, -fHalfHeight);
  const spitfire::math::cVec3 vMax(fHalfWidth, 0.0f, fHalfHeight);
  const spitfire::math::cVec3 vNormal(0.0f, 0.0f, -1.0f);

  opengl::cGeometryBuilder_v3_n3_t2 builder(*pGeometryDataPtr);

  // Front facing rectangle
  builder.PushBack(spitfire::math::cVec3(vMin.x, vMin.z, 0.0f), vNormal, spitfire::math::cVec2(0.0f, fTextureHeight));
  builder.PushBack(spitfire::math::cVec3(vMax.x, vMax.z, 0.0f), vNormal, spitfire::math::cVec2(fTextureWidth, 0.0f));
  builder.PushBack(spitfire::math::cVec3(vMin.x, vMax.z, 0.0f), vNormal, spitfire::math::cVec2(0.0f, 0.0f));
  builder.PushBack(spitfire::math::cVec3(vMax.x, vMin.z, 0.0f), vNormal, spitfire::math::cVec2(fTextureWidth, fTextureHeight));
  builder.PushBack(spitfire::math::cVec3(vMax.x, vMax.z, 0.0f), vNormal, spitfire::math::cVec2(fTextureWidth, 0.0f));
  builder.PushBack(spitfire::math::cVec3(vMin.x, vMin.z, 0.0f), vNormal, spitfire::math::cVec2(0.0f, fTextureHeight));

  vbo.SetData(pGeometryDataPtr);

  vbo.Compile();
}

void cApplication::CreateScreenHalfRectVBO(opengl::cStaticVertexBufferObject& staticVertexBufferObject, float_t fWidth, float_t fHeight)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fHalfTextureWidth = 0.5f * float(resolution.width);
  const float fTextureHeight = float(resolution.height);

  const float_t fHalfWidth = fWidth * 0.5f;
  const float_t fHalfHeight = fHeight * 0.5f;
  const spitfire::math::cVec2 vMin(-fHalfWidth, -fHalfHeight);
  const spitfire::math::cVec2 vMax(fHalfWidth, fHalfHeight);

  opengl::cGeometryBuilder_v2_t2 builder(*pGeometryDataPtr);

  // Front facing rectangle
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMin.y), spitfire::math::cVec2(fHalfTextureWidth, fTextureHeight));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMax.y), spitfire::math::cVec2(0.0f, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMax.y), spitfire::math::cVec2(fHalfTextureWidth, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMin.y), spitfire::math::cVec2(0.0f, fTextureHeight));
  builder.PushBack(spitfire::math::cVec2(vMin.x, vMax.y), spitfire::math::cVec2(0.0f, 0.0f));
  builder.PushBack(spitfire::math::cVec2(vMax.x, vMin.y), spitfire::math::cVec2(fHalfTextureWidth, fTextureHeight));

  staticVertexBufferObject.SetData(pGeometryDataPtr);

  staticVertexBufferObject.Compile2D();
}

void cApplication::CreateScene()
{
  // Create our heightmap
  heightMapData.LoadFromFile(TEXT("textures/heightmap.png"));

  pContext->CreateTexture(textureDiffuse, TEXT("textures/diffuse.png"));
  pContext->CreateTexture(textureDetail, TEXT("textures/detail.png"));


  heightMapScale.Set(0.5f, 10.0f, 0.5f);

  const uint8_t* pBuffer = heightMapData.GetLightmapBuffer();
  const size_t widthLightmap = heightMapData.GetLightmapWidth();
  const size_t depthLightmap = heightMapData.GetLightmapDepth();
  pContext->CreateTextureFromBuffer(textureLightMap, pBuffer, widthLightmap, depthLightmap, opengl::PIXELFORMAT::R8G8B8A8);


  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectHeightmapTriangles);
  CreateHeightmapTriangles(staticVertexBufferObjectHeightmapTriangles, heightMapData, heightMapScale);

  BuildOctree();

  DebugAddOctreeLines();

  // Use Cornflower blue as the sky colour
  // http://en.wikipedia.org/wiki/Cornflower_blue
  const spitfire::math::cColour cornFlowerBlue(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f);

  scene.skyColour = cornFlowerBlue;

  for (size_t i = 0; i < 100; i++) {
    const float x = spitfire::math::randomZeroToOnef() * 100.0f;
    const float z = spitfire::math::randomZeroToOnef() * 100.0f;
    const spitfire::math::cVec3 position(x, heightMapScale.y * heightMapData.GetHeight(x / heightMapScale.x, z / heightMapScale.z), z);

    scene.objects.translations.push_back(spitfire::math::cMat4::TranslationMatrix(position));

    const spitfire::math::cVec3 rotationDegrees(spitfire::math::randomf(-180.0f, 180.0f), 0.0f, 0.0f);

    scene.objects.rotations.push_back(spitfire::math::cMat4::RotationMatrix(rotationDegrees).GetRotation());

    switch (spitfire::math::random(3)) {
      case 0: {
        scene.objects.types.push_back(TYPE::SOLDIER);
        break;
      }
      case 1: {
        scene.objects.types.push_back(TYPE::BULLET);
        break;
      }
      case 2: {
        scene.objects.types.push_back(TYPE::TREE);
        break;
      }
    }
  }
}

void cApplication::CreateNavigationMesh()
{
  astar::config<Node> cfg;
  cfg.node_limit = 1000;
  cfg.cost_limit = 1000;
  cfg.route_cost = 0.0f;

  std::vector<spitfire::math::cVec3> nodePositions;
  std::vector<std::pair<size_t, size_t>> edges;

  const float fScale = 10.0f;
  const float fOffsetY = 0.5f;

  const size_t width = 10;
  const size_t height = 10;

  // Create a grid of node positions and connections
  for (size_t z = 0; z < height; z++) {
    for (size_t x = 0; x < width; x++) {
      const float fJitteredX = (fScale * float(x + 1)) + spitfire::math::randomMinusOneToPlusOnef() * 4.0f;
      const float fJitteredZ = (fScale * float(z + 1)) + spitfire::math::randomMinusOneToPlusOnef() * 4.0f;
      const float fY = (heightMapScale.y * heightMapData.GetHeight(fJitteredX / heightMapScale.x, fJitteredZ / heightMapScale.z)) + fOffsetY;
      const spitfire::math::cVec3 position(fJitteredX, fY, fJitteredZ);
      nodePositions.push_back(position);
    }
  }


  // Create our edges
  // Create these edges:
  // +<--->+
  // ^
  // |
  // v
  // +
  for (size_t z = 0; z < height - 1; z++) {
    for (size_t x = 0; x < width - 1; x++) {
      const size_t top_left = (z * width) + x;
      const size_t top_right = (z * width) + x + 1;
      const size_t bottom_left = ((z + 1) * width) + x;

      // Edges from left to right, right to left
      edges.push_back(std::make_pair(top_left, top_right));
      edges.push_back(std::make_pair(top_right, top_left));

      // Edges from top to bottom, bottom to top
      edges.push_back(std::make_pair(top_left, bottom_left));
      edges.push_back(std::make_pair(bottom_left, top_left));
    }
  }

  // Create the right hand edges:
  // +
  // ^
  // |
  // v
  // +
  for (size_t z = 0; z < height - 1; z++) {
    const size_t x = width - 1;

    const size_t top_left = (z * width) + x;
    const size_t bottom_left = ((z + 1) * width) + x;

    // Edges from top to bottom, bottom to top
    edges.push_back(std::make_pair(top_left, bottom_left));
    edges.push_back(std::make_pair(bottom_left, top_left));
  }

  // Create the bottom edges:
  // +<--->+
  for (size_t x = 0; x < width - 1; x++) {
    const size_t z = height - 1;

    const size_t top_left = (z * width) + x;
    const size_t top_right = (z * width) + x + 1;

    // Edges from left to right, right to left
    edges.push_back(std::make_pair(top_left, top_right));
    edges.push_back(std::make_pair(top_right, top_left));
  }


  // Create our navigation mesh
  navigationMesh.SetNodesAndEdges(nodePositions, edges);

  const Node& from = navigationMesh.GetNode(0);
  const Node& to = navigationMesh.GetNode((width * height) - 1);
  
  
  std::list<Node> path;
  const bool result = astar::astar(from, to, path, &astar::straight_distance_heuristic<Node>, cfg);

  std::cout<<"Nodes size: "<<nodePositions.size()<<std::endl;
  std::cout<<"Path size: "<<path.size()<<std::endl;
  size_t i = 0;
  for (auto node : path) {
    std::cout<<"node["<<i<<"]: "<<node.debug_identifier<<std::endl;
    i++;
  }
  std::cout<<"cfg.result_nodes_examined: "<<cfg.result_nodes_examined<<std::endl;
  std::cout<<"cfg.result_nodes_pending: "<<cfg.result_nodes_pending<<std::endl;
  std::cout<<"cfg.result_nodes_opened: "<<cfg.result_nodes_opened<<std::endl;
  std::cout<<"cfg.route_length: "<<cfg.route_length<<std::endl;
  std::cout<<"cfg.route_cost: "<<cfg.route_cost<<std::endl;
}

void cApplication::CreateHeightmapTriangles(opengl::cStaticVertexBufferObject& staticVertexBufferObject, const cHeightmapData& data, const spitfire::math::cVec3& scale)
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  const float fDetailMapRepeat = 10.0f;
  const float fDetailMapWidth = fDetailMapRepeat;

  // NOTE: Diffuse and lightmap will have the duplicated texture coordinates (0..1)
  // Detail map will have repeated texture coordinates (0..fDetailMapRepeat)
  opengl::cGeometryBuilder_v3_n3_t2_t2_t2 builder(*pGeometryDataPtr);

  const size_t width = data.GetWidth();
  const size_t depth = data.GetDepth();
  for (size_t z = 0; z < depth - 1; z++) {
    for (size_t x = 0; x < width - 1; x++) {
      const float fDiffuseAndLightmapU = float(x) / float(width);
      const float fDiffuseAndLightmapV = float(z) / float(depth);
      const float fDiffuseAndLightmapU2 = (float(x) + 1.0f) / float(width);
      const float fDiffuseAndLightmapV2 = (float(z) + 1.0f) / float(depth);

      const float fDetailMapU = float(x) / float(width) * fDetailMapWidth;
      const float fDetailMapV = float(z) / float(depth) * fDetailMapWidth;
      const float fDetailMapU2 = (float(x) + 1.0f) / float(width) * fDetailMapWidth;
      const float fDetailMapV2 = (float(z) + 1.0f) / float(depth) * fDetailMapWidth;

      builder.PushBack(scale * spitfire::math::cVec3(float(x + 1), data.GetHeight(x + 1, z + 1), float(z + 1)), data.GetNormal(x + 1, z + 1, scale), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDetailMapU2, fDetailMapV2));
      builder.PushBack(scale * spitfire::math::cVec3(float(x + 1), data.GetHeight(x + 1, z), float(z)), data.GetNormal(x + 1, z, scale), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV), spitfire::math::cVec2(fDetailMapU2, fDetailMapV));
      builder.PushBack(scale * spitfire::math::cVec3(float(x), data.GetHeight(x, z), float(z)), data.GetNormal(x, z, scale), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV), spitfire::math::cVec2(fDetailMapU, fDetailMapV));
      builder.PushBack(scale * spitfire::math::cVec3(float(x), data.GetHeight(x, z + 1), float(z + 1)), data.GetNormal(x, z + 1, scale), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDetailMapU, fDetailMapV2));
      builder.PushBack(scale * spitfire::math::cVec3(float(x + 1), data.GetHeight(x + 1, z + 1), float(z + 1)), data.GetNormal(x, z + 1, scale), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDiffuseAndLightmapU2, fDiffuseAndLightmapV2), spitfire::math::cVec2(fDetailMapU2, fDetailMapV2));
      builder.PushBack(scale * spitfire::math::cVec3(float(x), data.GetHeight(x, z), float(z)), data.GetNormal(x, z, scale), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV), spitfire::math::cVec2(fDiffuseAndLightmapU, fDiffuseAndLightmapV), spitfire::math::cVec2(fDetailMapU, fDetailMapV));
    }
  }

  staticVertexBufferObject.SetData(pGeometryDataPtr);

  staticVertexBufferObject.Compile();
}

void cApplication::CreateNavigationMeshDebugShapes()
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  opengl::cGeometryBuilder builder;

  const float fRadius = 0.5f;
  const size_t nSegments = 5;

  const size_t nTextureCoordinates = 0;

  const size_t n = navigationMesh.GetNodeCount();
  for (size_t i = 0; i < n; i++) {
    const Node& node = navigationMesh.GetNode(i);

    builder.CreateSphere(node.position, fRadius, nSegments, *pGeometryDataPtr, nTextureCoordinates);
  }

  pContext->CreateStaticVertexBufferObject(staticVertexBufferDebugNavigationMesh);

  staticVertexBufferDebugNavigationMesh.SetData(pGeometryDataPtr);

  staticVertexBufferDebugNavigationMesh.Compile();
}

void cApplication::CreateNavigationMeshDebugWayPointLines()
{
  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  opengl::cGeometryBuilder_v3_n3 builder(*pGeometryDataPtr);

  // TODO: Remove this normal
  const spitfire::math::cVec3 normal;

  const size_t n = navigationMesh.GetNodeCount();
  for (size_t i = 0; i < n; i++) {
    const Node& node = navigationMesh.GetNode(i);

    for (auto edge : node.edges) {
      builder.PushBack(node.position, normal);
      builder.PushBack(edge.to->position, normal);
    }
  }

  pContext->CreateStaticVertexBufferObject(staticVertexBufferDebugNavigationMeshWayPointLines);

  staticVertexBufferDebugNavigationMeshWayPointLines.SetData(pGeometryDataPtr);

  staticVertexBufferDebugNavigationMeshWayPointLines.Compile();
}

bool cApplication::Create()
{
  const opengl::cCapabilities& capabilities = system.GetCapabilities();

  resolution = capabilities.GetCurrentResolution();
  if ((resolution.width < 720) || (resolution.height < 480) || ((resolution.pixelFormat != opengl::PIXELFORMAT::R8G8B8A8) && (resolution.pixelFormat != opengl::PIXELFORMAT::R8G8B8))) {
    LOGERROR("Current screen resolution is not adequate ", resolution.width, "x", resolution.height);
    return false;
  }

  #ifdef BUILD_DEBUG
  // Override the resolution
  opengl::cSystem::GetWindowedTestResolution16By9(resolution.width, resolution.height);
  #else
  resolution.width = 1000;
  resolution.height = 562;
  #endif
  resolution.pixelFormat = opengl::PIXELFORMAT::R8G8B8A8;

  // Set our required resolution
  pWindow = system.CreateWindow(TEXT("OpenGLmm Shaders Test"), resolution, false);
  if (pWindow == nullptr) {
    LOGERROR("Window could not be created");
    return false;
  }

  pContext = pWindow->GetContext();
  if (pContext == nullptr) {
    LOGERROR("Context could not be created");
    return false;
  }

  // Create our font
  pContext->CreateFont(font, TEXT("fonts/pricedown.ttf"), 32, TEXT("shaders/font.vert"), TEXT("shaders/font.frag"));
  assert(font.IsValid());

  CreateShaders();

  // Create our text VBO
  pContext->CreateStaticVertexBufferObject(textVBO);


  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectGuiRectangle);
  CreateGuiRectangle(staticVertexBufferObjectGuiRectangle, 1000.0f, 500.0f);


  const float fRadius = 1.0f;

  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectCube0);
  CreateCube(staticVertexBufferObjectCube0, 0);
  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectSphere0);
  CreateSphere(staticVertexBufferObjectSphere0, 0, fRadius);
  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectGear0);
  CreateGear(staticVertexBufferObjectGear0);


  // Create our scene
  CreateScene();

  // Create navigation mesh
  CreateNavigationMesh();

  // Create our debug shapes
  CreateNavigationMeshDebugShapes();
  CreateNavigationMeshDebugWayPointLines();


  // Setup our event listeners
  pWindow->SetWindowEventListener(*this);
  pWindow->SetInputEventListener(*this);

  return true;
}

void cApplication::Destroy()
{
  ASSERT(pContext != nullptr);

  pContext->DestroyStaticVertexBufferObject(staticVertexBufferDebugNavigationMesh);
  pContext->DestroyStaticVertexBufferObject(staticVertexBufferDebugNavigationMeshWayPointLines);

  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectRayCasts);

  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectGear0);
  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectSphere0);
  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectCube0);


  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectHeightmapTriangles);

  pContext->DestroyTexture(textureDetail);
  pContext->DestroyTexture(textureLightMap);
  pContext->DestroyTexture(textureDiffuse);


  pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectGuiRectangle);

  // Destroy our text VBO
  pContext->DestroyStaticVertexBufferObject(textVBO);

  // Destroy our font
  if (font.IsValid()) pContext->DestroyFont(font);

  DestroyShaders();

  pContext = nullptr;

  if (pWindow != nullptr) {
    system.DestroyWindow(pWindow);
    pWindow = nullptr;
  }
}

void cApplication::CreateShaders()
{
  pContext->CreateShader(shaderHeightmap, TEXT("shaders/heightmap.vert"), TEXT("shaders/heightmap.frag"));
  assert(shaderHeightmap.IsCompiledProgram());

  pContext->CreateShader(shaderColour, TEXT("shaders/colour.vert"), TEXT("shaders/colour.frag"));
  assert(shaderColour.IsCompiledProgram());
}

void cApplication::DestroyShaders()
{
  if (shaderHeightmap.IsCompiledProgram()) pContext->DestroyShader(shaderHeightmap);

  if (shaderColour.IsCompiledProgram()) pContext->DestroyShader(shaderColour);
}

void cApplication::RenderScreenRectangleDepthTexture(float x, float y, opengl::cStaticVertexBufferObject& vbo, const opengl::cTextureFrameBufferObject& texture, opengl::cShader& shader)
{
  spitfire::math::cMat4 matModelView2D;
  matModelView2D.SetTranslation(x, y, 0.0f);

  pContext->BindShader(shader);

  pContext->BindTextureDepthBuffer(0, texture);

  pContext->BindStaticVertexBufferObject2D(vbo);

  {
    pContext->SetShaderProjectionAndModelViewMatricesRenderMode2D(opengl::MODE2D_TYPE::Y_INCREASES_DOWN_SCREEN, matModelView2D);

    pContext->DrawStaticVertexBufferObjectTriangles2D(vbo);
  }

  pContext->UnBindStaticVertexBufferObject2D(vbo);

  pContext->UnBindTextureDepthBuffer(0, texture);

  pContext->UnBindShader(shader);
}

void cApplication::RenderScreenRectangle(float x, float y, opengl::cStaticVertexBufferObject& vbo, const opengl::cTexture& texture, opengl::cShader& shader)
{
  spitfire::math::cMat4 matModelView2D;
  matModelView2D.SetTranslation(x, y, 0.0f);

  pContext->BindShader(shader);

  pContext->BindTexture(0, texture);

  pContext->BindStaticVertexBufferObject2D(vbo);

  {
    pContext->SetShaderProjectionAndModelViewMatricesRenderMode2D(opengl::MODE2D_TYPE::Y_INCREASES_DOWN_SCREEN, matModelView2D);

    pContext->DrawStaticVertexBufferObjectTriangles2D(vbo);
  }

  pContext->UnBindStaticVertexBufferObject2D(vbo);

  pContext->UnBindTexture(0, texture);

  pContext->UnBindShader(shader);
}

void cApplication::RenderScreenRectangle(opengl::cTexture& texture, opengl::cShader& shader, opengl::cStaticVertexBufferObject& vbo)
{
  RenderScreenRectangle(0.5f, 0.5f, vbo, texture, shader);
}

void cApplication::RenderScreenRectangleShaderAndTextureAlreadySet(opengl::cStaticVertexBufferObject& vbo)
{
  spitfire::math::cMat4 matModelView2D;
  matModelView2D.SetTranslation(0.5f, 0.5f, 0.0f);

  pContext->BindStaticVertexBufferObject2D(vbo);

  {
    pContext->SetShaderProjectionAndModelViewMatricesRenderMode2D(opengl::MODE2D_TYPE::Y_INCREASES_DOWN_SCREEN, matModelView2D);

    pContext->DrawStaticVertexBufferObjectTriangles2D(vbo);
  }

  pContext->UnBindStaticVertexBufferObject2D(vbo);
}

ssize_t cApplication::CollideRayWithObjects(const spitfire::math::cRay3& ray) const
{
  const float fRadius = 1.0f;
  float fDepth = 0.0f;

  const size_t n = scene.objects.translations.size();
  for (size_t i = 0; i < n; i++) {
    spitfire::math::cSphere sphere;
    sphere.SetPosition(scene.objects.translations[i].GetTranslation());
    sphere.SetRadius(fRadius);

    if (ray.CollideWithSphere(sphere, fDepth)) {
      return i;
    }
  }

  return -1;
}

float cApplication::CollideRayWithHeightmap(const spitfire::math::cRay3& ray) const
{
  assert(octree != nullptr);

  std::vector<const spitfire::math::cOctree<spitfire::math::cAABB3>*> nodes;
  octree->CollideRay(ray, nodes);

  const float fGridSize = 0.1f;

  for (auto pNode : nodes) {
    const spitfire::math::cAABB3* data = pNode->GetData();
    assert(data != nullptr);

    const spitfire::math::cVec3 nodeMin = data->GetMin();
    const spitfire::math::cVec3 nodeMax = data->GetMax();
    const spitfire::math::cVec3 nodeDimensions(nodeMax - nodeMin);
    const size_t nGridWidth = nodeDimensions.x / fGridSize;
    const size_t nGridDepth = nodeDimensions.z / fGridSize;
    for (size_t z = 0; z < nGridDepth; ++z) {
      for (size_t x = 0; x < nGridWidth; ++x) {
        const float fX0 = nodeMin.x + (x * fGridSize);
        const float fZ0 = nodeMin.z + (z * fGridSize);
        const spitfire::math::cVec3 point0(fX0, heightMapScale.y * heightMapData.GetHeight(fX0 / heightMapScale.x, fZ0 / heightMapScale.z), fZ0);

        const float fX1 = nodeMin.x + ((x + 1) * fGridSize);
        const float fZ1 = nodeMin.z + (z * fGridSize);
        const spitfire::math::cVec3 point1(fX1, heightMapScale.y * heightMapData.GetHeight(fX1 / heightMapScale.x, fZ1 / heightMapScale.z), fZ1);

        const float fX2 = nodeMin.x + (x * fGridSize);
        const float fZ2 = nodeMin.z + ((z + 1) * fGridSize);
        const spitfire::math::cVec3 point2(fX2, heightMapScale.y * heightMapData.GetHeight(fX2 / heightMapScale.x, fZ2 / heightMapScale.z), fZ2);

        const float fX3 = nodeMin.x + ((x + 1) * fGridSize);
        const float fZ3 = nodeMin.z + ((z + 1) * fGridSize);
        const spitfire::math::cVec3 point3(fX3, heightMapScale.y * heightMapData.GetHeight(fX3 / heightMapScale.x, fZ3 / heightMapScale.z), fZ3);

        float fDepth = 0.0f;
        if (ray.CollideWithTriangle(point0, point1, point2, fDepth) || ray.CollideWithTriangle(point1, point2, point3, fDepth)) {
          return fDepth;
        }
      }
    }
  }

  // No collision, return an invalid depth
  return -1.0f;
}

void cApplication::HandleSelectionAndOrders(int mouseX, int mouseY)
{
  const spitfire::math::cMat4 matProjection = pContext->CalculateProjectionMatrix();
  const spitfire::math::cMat4 matView = camera.CalculateViewMatrix();

  // Get the origin and direction of our mouse selection
  spitfire::math::cVec3 origin;
  spitfire::math::cVec3 direction;
  CameraRayCast(matProjection, matView, mouseX, mouseY, origin, direction);

  if (direction.GetLength() < 0.01f) {
    return;
  }

  // Add a ray
  //AddRayCastLine(spitfire::math::cLine3(origin, direction * 1000.0f));


  const float fLength = 10000.0f;

  spitfire::math::cRay3 ray;
  ray.SetOriginAndDirection(origin, direction);
  ray.SetLength(fLength);

  // Collide with our objects
  const ssize_t object = CollideRayWithObjects(ray);
  if (object != -1) {
    // An object was selected
    selectedObject = object;
    return;
  }

  // Collide with our heightmap
  const float fDepth = CollideRayWithHeightmap(ray);
  if (fDepth >= 0.0f) {
    // The heightmap was clicked on
    const spitfire::math::cVec3 point = origin + (direction * fDepth);

    // Add a ray
    AddRayCastLine(spitfire::math::cLine3(origin, point));
    CreateRayCastLineStaticVertexBuffer();
  }
}

void cApplication::_OnWindowEvent(const opengl::cWindowEvent& event)
{
  LOG("");

  if (event.IsQuit()) {
    LOG("Quiting");
    bIsDone = true;
  }
}

void cApplication::_OnMouseEvent(const opengl::cMouseEvent& event)
{
  // These are a little too numerous to log every single one
  //LOG("");

  if (freeLookCamera.bDown && event.IsMouseMove()) {
    //LOG("Mouse move");

    if (fabs(event.GetX() - (pWindow->GetWidth() * 0.5f)) > 0.5f) {
      camera.RotateX(0.08f * (event.GetX() - (pWindow->GetWidth() * 0.5f)));
    }

    if (fabs(event.GetY() - (pWindow->GetHeight() * 0.5f)) > 1.5f) {
      camera.RotateY(0.05f * (event.GetY() - (pWindow->GetHeight() * 0.5f)));
    }
  } else if (event.IsButtonUp() && (event.GetButton() == SDL_BUTTON_LEFT)) {
    HandleSelectionAndOrders(event.GetX(), event.GetY());
  }
}

void cApplication::_OnKeyboardEvent(const opengl::cKeyboardEvent& event)
{
  //LOG("");

  moveCameraForward.Process(event.GetKeyCode(), event.IsKeyDown());
  moveCameraBack.Process(event.GetKeyCode(), event.IsKeyDown());
  moveCameraLeft.Process(event.GetKeyCode(), event.IsKeyDown());
  moveCameraRight.Process(event.GetKeyCode(), event.IsKeyDown());
  freeLookCamera.Process(event.GetKeyCode(), event.IsKeyDown());

  if (event.IsKeyDown()) {
    switch (event.GetKeyCode()) {
      case SDLK_ESCAPE: {
        LOG("Escape key pressed, quiting");
        bIsDone = true;
        break;
      }

      case SDLK_SPACE: {
        //LOG("spacebar down");
        bIsPhysicsRunning = false;
        break;
      }
    }
  } else if (event.IsKeyUp()) {
    switch (event.GetKeyCode()) {
      case SDLK_SPACE: {
        LOG("spacebar up");
        bIsPhysicsRunning = true;
        break;
      }

      case SDLK_F5: {
        bReloadShaders = true;
        bUpdateShaderConstants = true;
        break;
      }
      case SDLK_x: {
        bIsWireframe = !bIsWireframe;
        break;
      }
    }
  }
}

std::vector<std::string> cApplication::GetInputDescription() const
{
  std::vector<std::string> description;
  description.push_back("W forward");
  description.push_back("A left");
  description.push_back("S backward");
  description.push_back("D right");
  description.push_back("Space pause rotation");
  description.push_back("F5 reload shaders");
  description.push_back("1 toggle wireframe");
  description.push_back("2 toggle directional light");
  description.push_back("3 toggle point light");
  description.push_back("4 toggle spot light");
  description.push_back("5 toggle split screen post render");
  description.push_back("Y toggle post render sepia");
  description.push_back("U toggle post render noir");
  description.push_back("I toggle post render matrix");
  description.push_back("O toggle post render teal and orange");
  description.push_back("P toggle post render colour blind simulation");
  description.push_back("L switch colour blind mode");
  description.push_back("Esc quit");

  return description;
}

void cApplication::AddRayCastLine(const spitfire::math::cLine3& line)
{
  // Add a ray
  rayCastLines.push_back(line);
}

void cApplication::DebugAddOctreeLines()
{
  DebugAddOctreeLinesRecursive(*octree);

  CreateRayCastLineStaticVertexBuffer();
}

void cApplication::DebugAddOctreeLinesRecursive(const spitfire::math::cOctree<spitfire::math::cAABB3>& node)
{
  // Add 6 lines for the extents of this node
  const spitfire::math::cVec3 min = node.GetMin();
  const spitfire::math::cVec3 max = node.GetMax();
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(min.x, min.y, min.z), spitfire::math::cVec3(max.x, min.y, min.z)));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(max.x, min.y, min.z), spitfire::math::cVec3(max.x, min.y, max.z)));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(max.x, min.y, max.z), spitfire::math::cVec3(min.x, min.y, max.z)));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(min.x, min.y, max.z), spitfire::math::cVec3(min.x, min.y, min.z)));

  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(min.x, min.y, min.z), spitfire::math::cVec3(min.x, max.y, min.z)));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(min.x, min.y, min.z), spitfire::math::cVec3(min.x, min.y, max.z)));

  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(min.x, max.y, max.z), max));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(max.x, min.y, max.z), max));
  AddRayCastLine(spitfire::math::cLine3(spitfire::math::cVec3(max.x, max.y, min.z), max));

  // Add boxes for each of the child nodes
  for (size_t i = 0; i < 8; i++) {
    const spitfire::math::cOctree<spitfire::math::cAABB3>* child = node.GetChild(i);
    if (child != nullptr) DebugAddOctreeLinesRecursive(*child);
  }
}

void cApplication::CreateRayCastLineStaticVertexBuffer()
{
  // Recreate our ray casts vertex buffer object
  if (staticVertexBufferObjectGear0.IsCompiled()) pContext->DestroyStaticVertexBufferObject(staticVertexBufferObjectRayCasts);

  pContext->CreateStaticVertexBufferObject(staticVertexBufferObjectRayCasts);

  opengl::cGeometryDataPtr pGeometryDataPtr = opengl::CreateGeometryData();

  opengl::cGeometryBuilder_v3_n3 builder(*pGeometryDataPtr);

  // TODO: Remove this normal
  const spitfire::math::cVec3 normal;

  const size_t n = rayCastLines.size();
  for (size_t i = 0; i < n; i++) {
    builder.PushBack(rayCastLines[i].GetOrigin(), normal);
    builder.PushBack(rayCastLines[i].GetDestination(), normal);
  }

  staticVertexBufferObjectRayCasts.SetData(pGeometryDataPtr);

  staticVertexBufferObjectRayCasts.Compile();
}

void cApplication::RenderFrame()
{
  assert(textVBO.IsCompiled());

  const spitfire::math::cMat4 matProjection = pContext->CalculateProjectionMatrix();

  const spitfire::math::cMat4 matView = camera.CalculateViewMatrix();

  {
    // Render the scene into a texture for later
    pContext->SetClearColour(scene.skyColour);

    pContext->BeginRenderToScreen();

    if (bIsWireframe) pContext->EnableWireframe();


    // Render terrain
    {
      pContext->BindTexture(0, textureDiffuse);
      pContext->BindTexture(1, textureLightMap);
      pContext->BindTexture(2, textureDetail);

      pContext->BindShader(shaderHeightmap);

      spitfire::math::cMat4 matModel;

      pContext->BindStaticVertexBufferObject(staticVertexBufferObjectHeightmapTriangles);
      pContext->SetShaderProjectionAndViewAndModelMatrices(matProjection, matView, matModel);
      pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectHeightmapTriangles);
      pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectHeightmapTriangles);

      pContext->UnBindShader(shaderHeightmap);

      pContext->UnBindTexture(2, textureDetail);
      pContext->UnBindTexture(1, textureLightMap);
      pContext->UnBindTexture(0, textureDiffuse);
    }

    {
      // Render objects
      pContext->BindShader(shaderColour);

      const spitfire::math::cColour white(1.0f, 1.0f, 1.0f);
      const spitfire::math::cColour red(1.0f, 0.0f, 0.0f);
      const spitfire::math::cColour blue(0.0f, 0.0f, 1.0f);
      const spitfire::math::cColour orange(1.0f, 1.0f, 0.0f);

      const size_t n = scene.objects.types.size();
      for (size_t i = 0; i < n; i++) {
        if (selectedObject == ssize_t(i)) {
          pContext->SetShaderConstant("colour", orange);
          pContext->BindStaticVertexBufferObject(staticVertexBufferObjectCube0);
          pContext->SetShaderProjectionAndModelViewMatrices(matProjection, matView * scene.objects.translations[i] * scene.objects.rotations[i].GetMatrix());
          pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectCube0);
          pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectCube0);
        } else {
          switch (scene.objects.types[i]) {
            case TYPE::SOLDIER: {
              pContext->SetShaderConstant("colour", red);
              pContext->BindStaticVertexBufferObject(staticVertexBufferObjectCube0);
              pContext->SetShaderProjectionAndModelViewMatrices(matProjection, matView * scene.objects.translations[i] * scene.objects.rotations[i].GetMatrix());
              pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectCube0);
              pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectCube0);
              break;
            }
            case TYPE::BULLET: {
              pContext->SetShaderConstant("colour", white);
              pContext->BindStaticVertexBufferObject(staticVertexBufferObjectSphere0);
              pContext->SetShaderProjectionAndModelViewMatrices(matProjection, matView * scene.objects.translations[i] * scene.objects.rotations[i].GetMatrix());
              pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectSphere0);
              pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectSphere0);
            }
            case TYPE::TREE: {
              pContext->SetShaderConstant("colour", blue);
              pContext->BindStaticVertexBufferObject(staticVertexBufferObjectGear0);
              pContext->SetShaderProjectionAndModelViewMatrices(matProjection, matView * scene.objects.translations[i] * scene.objects.rotations[i].GetMatrix());
              pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectGear0);
              pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectGear0);
            }
          }
        }
      }

      pContext->UnBindShader(shaderColour);
    }


    opengl::cSystem::GetErrorString();


    if (staticVertexBufferDebugNavigationMesh.IsCompiled()) {
      // Render our ray casts
      pContext->BindShader(shaderColour);

      const spitfire::math::cColour green(0.0f, 1.0f, 0.0f);

      pContext->SetShaderConstant("colour", green);

      const spitfire::math::cMat4 matModel;

      pContext->BindStaticVertexBufferObject(staticVertexBufferDebugNavigationMesh);
      pContext->SetShaderProjectionAndViewAndModelMatrices(matProjection, matView, matModel);
      pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferDebugNavigationMesh);
      pContext->UnBindStaticVertexBufferObject(staticVertexBufferDebugNavigationMesh);

      pContext->UnBindShader(shaderColour);
    }

    if (staticVertexBufferDebugNavigationMeshWayPointLines.IsCompiled()) {
      // Render our ray casts
      pContext->BindShader(shaderColour);

      const spitfire::math::cColour darkGreen(0.0f, 0.5f, 0.0f);

      pContext->SetShaderConstant("colour", darkGreen);

      const spitfire::math::cMat4 matModel;

      pContext->BindStaticVertexBufferObject(staticVertexBufferDebugNavigationMeshWayPointLines);
      pContext->SetShaderProjectionAndViewAndModelMatrices(matProjection, matView, matModel);
      pContext->DrawStaticVertexBufferObjectLines(staticVertexBufferDebugNavigationMeshWayPointLines);
      pContext->UnBindStaticVertexBufferObject(staticVertexBufferDebugNavigationMeshWayPointLines);

      pContext->UnBindShader(shaderColour);
    }

    opengl::cSystem::GetErrorString();


    if (staticVertexBufferObjectRayCasts.IsCompiled()) {
      // Render our ray casts
      pContext->BindShader(shaderColour);

      const spitfire::math::cColour green(0.0f, 1.0f, 0.0f);
      
      pContext->SetShaderConstant("colour", green);

      const spitfire::math::cMat4 matModel;

      pContext->BindStaticVertexBufferObject(staticVertexBufferObjectRayCasts);
      pContext->SetShaderProjectionAndViewAndModelMatrices(matProjection, matView, matModel);
      pContext->DrawStaticVertexBufferObjectLines(staticVertexBufferObjectRayCasts);
      pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectRayCasts);

      pContext->UnBindShader(shaderColour);
    }


    opengl::cSystem::GetErrorString();


    if (bIsWireframe) pContext->DisableWireframe();

    // Render some gui in 3d space
    /*{
    pContext->DisableDepthTesting();

    const size_t n = testImages.size();
    for (size_t i = 0; i < n; i++) {
    pContext->BindShader(shaderPassThrough);

    cTextureVBOPair* pPair = testImages[i];

    pContext->BindTexture(0, *(pPair->pTexture));

    pContext->BindStaticVertexBufferObject(staticVertexBufferObjectGuiRectangle);

    const spitfire::math::cVec3 position(2.5f - float(i), 1.8f, -5.0f);
    spitfire::math::cMat4 matTranslation;
    matTranslation.SetTranslation(position);

    pContext->SetShaderProjectionAndModelViewMatrices(matProjection, matTranslation);

    pContext->DrawStaticVertexBufferObjectTriangles(staticVertexBufferObjectGuiRectangle);

    pContext->UnBindStaticVertexBufferObject(staticVertexBufferObjectGuiRectangle);

    pContext->UnBindTexture(0, *(pPair->pTexture));

    pContext->UnBindShader(shaderPassThrough);
    }

    pContext->EnableDepthTesting();
    }*/




    // Now draw an overlay of our rendered textures
    pContext->BeginRenderMode2D(opengl::MODE2D_TYPE::Y_INCREASES_DOWN_SCREEN);



    spitfire::math::cVec2 position(0.0f + (0.5f * 0.25f), 0.0f + (0.5f * 0.25f));

#if 0
    if (bIsTronGlow) {
      position.x = 0.0f + (0.5f * 0.25f);

      // Draw the lens flare and dirt textures for debugging purposes
      // Down the side
      RenderScreenRectangle(position.x, position.y, staticVertexBufferObjectScreen2DTeapot, lensFlareDirt.GetTextureLensColor(), shaderScreen1D); position.y += 0.25f;
      RenderDebugScreenRectangleVariableSize(position.x, position.y, lensFlareDirt.GetTextureLensDirt()); position.y += 0.25f;
      RenderScreenRectangle(position.x, position.y, staticVertexBufferObjectScreen2DTeapot, lensFlareDirt.GetTextureLensStar(), shaderScreen2D); position.y += 0.25f;

      // Along the bottom
      position.Set(0.0f + (0.5f * 0.25f), 0.75f + (0.5f * 0.25f));
      RenderDebugScreenRectangleVariableSize(position.x, position.y, lensFlareDirt.GetTempA()); position.x += 0.25f;
      RenderDebugScreenRectangleVariableSize(position.x, position.y, lensFlareDirt.GetTempB()); position.x += 0.25f;
      RenderDebugScreenRectangleVariableSize(position.x, position.y, lensFlareDirt.GetTempC()); position.x += 0.25f;
    }
#endif


    // Draw the text overlay
    {
      pContext->BindFont(font);

      // Rendering the font in the middle of the screen
      spitfire::math::cMat4 matModelView;
      matModelView.SetTranslation(0.02f, 0.05f, 0.0f);

      pContext->SetShaderProjectionAndModelViewMatricesRenderMode2D(opengl::MODE2D_TYPE::Y_INCREASES_DOWN_SCREEN, matModelView);

      pContext->BindStaticVertexBufferObject2D(textVBO);

      {
        pContext->DrawStaticVertexBufferObjectTriangles2D(textVBO);
      }

      pContext->UnBindStaticVertexBufferObject2D(textVBO);

      pContext->UnBindFont(font);
    }

    pContext->EndRenderMode2D();

    pContext->EndRenderToScreen(*pWindow);
  }
}

void cApplication::Run()
{
  LOG("");

  assert(pContext != nullptr);
  assert(pContext->IsValid());
  assert(font.IsValid());
  assert(shaderColour.IsCompiledProgram());

  assert(textureDiffuse.IsValid());
  assert(textureLightMap.IsValid());
  assert(textureDetail.IsValid());
  assert(shaderHeightmap.IsCompiledProgram());

  assert(staticVertexBufferObjectHeightmapTriangles.IsCompiled());

  assert(staticVertexBufferObjectCube0.IsCompiled());
  assert(staticVertexBufferObjectSphere0.IsCompiled());
  assert(staticVertexBufferObjectGear0.IsCompiled());

  // Print the input instructions
  const std::vector<std::string> inputDescription = GetInputDescription();
  const size_t n = inputDescription.size();
  for (size_t i = 0; i < n; i++) LOG(inputDescription[i]);

  // Set up the camera
  camera.SetPosition(spitfire::math::cVec3(36.5f, 27.0f, 35.0f));
  camera.RotateX(90.0f);
  camera.RotateY(45.0f);

  
  spitfire::durationms_t T0 = spitfire::util::GetTimeMS();
  uint32_t Frames = 0;

  spitfire::durationms_t previousUpdateInputTime = spitfire::util::GetTimeMS();
  spitfire::durationms_t previousUpdateTime = spitfire::util::GetTimeMS();
  spitfire::durationms_t currentTime = spitfire::util::GetTimeMS();

  const float fLightLuminanceIncrease0To1 = 0.3f;

  // Green directional light
  const spitfire::math::cVec3 lightDirectionalPosition(5.0f, 5.0f, 5.0f);
  const spitfire::math::cColour lightDirectionalAmbientColour(0.2f, 0.25f, 0.2f);
  const spitfire::math::cColour lightDirectionalDiffuseColour(1.5f, 1.35f, 1.2f);
  const spitfire::math::cColour lightDirectionalSpecularColour(0.0f, 1.0f, 0.0f);

  // Red point light
  const spitfire::math::cColour lightPointColour(0.25f, 0.0f, 0.0f);
  const float lightPointAmbient = 0.15f;
  const float lightPointConstantAttenuation = 0.3f;
  const float lightPointLinearAttenuation = 0.007f;
  const float lightPointExpAttenuation = 0.00008f;

  // Blue spot light
  const spitfire::math::cVec3 lightSpotPosition(0.0f, 5.0f, 4.0f);
  const spitfire::math::cVec3 lightSpotDirection(0.0f, 1.0f, 0.0f);
  const spitfire::math::cColour lightSpotColour(0.0f, 0.0f, 1.0f);
  //const float lightSpotAmbient = 0.15f;
  //const float lightSpotConstantAttenuation = 0.3f;
  const float lightSpotLinearAttenuation = 0.002f;
  //const float lightSpotExpAttenuation = 0.00008f;
  const float fLightSpotLightConeAngle = 40.0f;
  const float lightSpotConeCosineAngle = cosf(spitfire::math::DegreesToRadians(fLightSpotLightConeAngle));

  // Material
  const spitfire::math::cColour materialAmbientColour(1.0f, 1.0f, 1.0f);
  const spitfire::math::cColour materialDiffuseColour(1.0f, 1.0f, 1.0f);
  const spitfire::math::cColour materialSpecularColour(1.0f, 1.0f, 1.0f);
  const float fMaterialShininess = 25.0f;

  const float fFogStart = 5.0f;
  const float fFogEnd = 15.0f;
  //const float fFogDensity = 0.5f;


  const uint32_t uiUpdateInputDelta = uint32_t(1000.0f / 120.0f);
  const uint32_t uiUpdateDelta = uint32_t(1000.0f / 60.0f);

  spitfire::durationms_t currentSimulationTime = 0;

  while (!bIsDone) {
    // Update state
    currentTime = spitfire::util::GetTimeMS();

    if ((currentTime - previousUpdateInputTime) > uiUpdateInputDelta) {
      // Update window events
      pWindow->ProcessEvents();

      if (pWindow->IsActive() && freeLookCamera.bDown) {
        // Hide the cursor
        pWindow->ShowCursor(false);

        // Keep the cursor locked to the middle of the screen so that when the mouse moves, it is in relative pixels
        pWindow->WarpCursorToMiddleOfScreen();
      } else {
        // Show the cursor
        pWindow->ShowCursor(true);
      }

      previousUpdateInputTime = currentTime;
    }

    if ((currentTime - previousUpdateTime) > uiUpdateDelta) {
      // Update the camera
      const float fDistance = 0.1f;
      if (moveCameraForward.bDown) camera.MoveZ(fDistance);
      if (moveCameraBack.bDown) camera.MoveZ(-fDistance);
      if (moveCameraLeft.bDown) camera.MoveX(-fDistance);
      if (moveCameraRight.bDown) camera.MoveX(fDistance);


      if (bIsPhysicsRunning) {
        currentSimulationTime++;

        // TODO: Update objects
      }

      previousUpdateTime = currentTime;
    }

    if (bReloadShaders) {
      LOG("Reloading shaders");

      // Destroy and create our shaders
      DestroyShaders();
      CreateShaders();

      bReloadShaders = false;
    }

    if (bUpdateShaderConstants) {
      // Set our shader constants


      bUpdateShaderConstants = false;
    }


    // Update our text
    CreateText();
    
    // Render a frame to the screen
    RenderFrame();

    // Gather our frames per second
    Frames++;
    {
      spitfire::durationms_t t = spitfire::util::GetTimeMS();
      if (t - T0 >= 1000) {
        float seconds = (t - T0) / 1000.0f;
        fFPS = Frames / seconds;
        //LOG(Frames, " frames in ", seconds, " seconds = ", fFPS, " FPS");
        T0 = t;
        Frames = 0;
      }
    }
  };

  pWindow->ShowCursor(true);
}

int main(int argc, char** argv)
{
  bool bIsSuccess = true;

  RedirectStandardOutputToOutputWindow();

  {
    cApplication application;

    bIsSuccess = application.Create();
    if (bIsSuccess) application.Run();

    application.Destroy();
  }

  return bIsSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
}
