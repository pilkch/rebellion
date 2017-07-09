#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include <spitfire/math/math.h>
#include <spitfire/math/cVec3.h>
#include <spitfire/math/cColour.h>
#include <spitfire/util/string.h>

class cHeightmapData
{
public:
  cHeightmapData();

  bool LoadFromFile(const spitfire::string_t& sFilename);

  size_t GetWidth() const { return width; }
  size_t GetDepth() const { return depth; }
  float GetLowestPoint() const { return fLowestPoint; }
  float GetHighestPoint() const { return fHighestPoint; }

  float GetHeight(size_t x, size_t y) const;
  spitfire::math::cVec3 GetNormal(size_t x, size_t y, const spitfire::math::cVec3& scale) const;

  size_t GetLightmapWidth() const { return widthLightmap; }
  size_t GetLightmapDepth() const { return depthLightmap; }
  const uint8_t* GetLightmapBuffer() const;

private:
  spitfire::math::cVec3 GetNormalOfTriangle(const spitfire::math::cVec3& p0, const spitfire::math::cVec3& p1, const spitfire::math::cVec3& p2) const;

  static void GenerateLightmap(const std::vector<float>& heightmap, std::vector<spitfire::math::cColour>& lightmap, float fScaleY, const spitfire::math::cColour& ambientColour, const spitfire::math::cColour& shadowColor, size_t size, const spitfire::math::cVec3& sunPosition);

  static void DoubleImageSize(const std::vector<spitfire::math::cColour>& source, size_t width, size_t height, std::vector<spitfire::math::cColour>& destination);
  void SmoothImage(const std::vector<spitfire::math::cColour>& source, size_t width, size_t height, size_t iterations, std::vector<spitfire::math::cColour>& destination) const;

  spitfire::math::cColour GetLightmapPixel(const std::vector<spitfire::math::cColour>& lightmap, size_t x, size_t y) const;

  std::vector<float> heightmap;
  size_t width;
  size_t depth;

  float fLowestPoint;
  float fHighestPoint;

  std::vector<uint8_t> lightmap;
  size_t widthLightmap;
  size_t depthLightmap;
};

#endif // HEIGHTMAP_H
