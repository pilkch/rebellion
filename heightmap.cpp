#include <libvoodoomm/cImage.h>

#include <spitfire/util/log.h>

#include "heightmap.h"

int round_up_or_down(float n)
{
  return (n - ((int)n) >= 0.5) ? (int)n + 1 : (int)n;
}


cHeightmapData::cHeightmapData() :
  width(0),
  depth(0),
  fLowestPoint(0.0f),
  fHighestPoint(0.0f),
  widthLightmap(0),
  depthLightmap(0)
{
}

bool cHeightmapData::LoadFromFile(const spitfire::string_t& sFilename)
{
  voodoo::cImage image;
  if (!image.LoadFromFile(sFilename)) {
    LOG("cHeightmapData::LoadFromFile Could not load \"", sFilename, "\"");
    return false;
  }

  width = image.GetWidth();
  depth = image.GetHeight();

  // Create heightmap data
  const size_t n = width * depth;

  heightmap.resize(n, 0);

  const uint8_t* pPixels = image.GetPointerToBuffer();

  for (size_t y = 0; y < depth; y++) {
    for (size_t x = 0; x < width; x++) {
      const float fValue = float(pPixels[(y * width) + x]) / 255.0f;
      heightmap[(y * width) + x] = fValue;

      if (fValue < fLowestPoint) fLowestPoint = fValue;
      if (fValue > fHighestPoint) fHighestPoint = fValue;
    }
  }


  // Calculate shadowmap texture
  std::vector<spitfire::math::cColour> _lightmap;
  _lightmap.resize(n, spitfire::math::cColour());

  // I find that an exagerated z scale gives a better, more obvious result
  const float fScaleY = 100.0f;
  const spitfire::math::cColour ambientColour(1.0f, 1.0f, 1.0f);
  const spitfire::math::cColour shadowColour = 0.5f * ambientColour;
  const size_t size = width;
  const spitfire::math::cVec3 sunPosition(50.0f, 100.0f, 100.0f);
  GenerateLightmap(heightmap, _lightmap, fScaleY, ambientColour, shadowColour, size, sunPosition);

  widthLightmap = width;
  depthLightmap = depth;


#if 0
  // For debugging
  {
    const size_t nLightmap = widthLightmap * depthLightmap;

    // Copy the lightmap from the cColour vector to the uint8_t vector
    lightmap.resize(nLightmap * 4, 0);

    for (size_t y = 0; y < depthLightmap; y++) {
      for (size_t x = 0; x < widthLightmap; x++) {
        const size_t src = (y * widthLightmap) + x;
        const size_t dst = 4 * ((y * widthLightmap) + x);
        lightmap[dst + 0] = uint8_t(_lightmap[src].r * 255.0f);
        lightmap[dst + 1] = uint8_t(_lightmap[src].g * 255.0f);
        lightmap[dst + 2] = uint8_t(_lightmap[src].b * 255.0f);
        lightmap[dst + 3] = uint8_t(_lightmap[src].a * 255.0f);
      }
    }
  }

  voodoo::cImage generated;
  const uint8_t* pBuffer = &lightmap[0];
  generated.CreateFromBuffer(pBuffer, widthLightmap, depthLightmap, opengl::PIXELFORMAT::R8G8B8A8);
  generated.SaveToBMP(TEXT("/home/chris/dev/tests/openglmm_heightmap/textures/generated.bmp"));

  exit(0);
#endif


  {
    // Smooth the lightmap
    std::vector<spitfire::math::cColour> smoothed;

    SmoothImage(_lightmap, widthLightmap, depthLightmap, 10, smoothed);

    _lightmap = smoothed;
  }


  /*for (size_t i = 0; i < 3; i++) {
  // Double the resolution of the lightmap
  std::vector<spitfire::math::cColour> lightmapCopy;

  DoubleImageSize(_lightmap, widthLightmap, depthLightmap, lightmapCopy);

  _lightmap = lightmapCopy;
  widthLightmap *= 2;
  depthLightmap *= 2;


  // Smooth the lightmap
  std::vector<spitfire::math::cColour> smoothed;

  SmoothImage(_lightmap, widthLightmap, depthLightmap, 10, smoothed);

  _lightmap = smoothed;
  }*/

  const size_t nLightmap = widthLightmap * depthLightmap;


  // Copy the lightmap from the cColour vector to the uint8_t vector
  lightmap.resize(nLightmap * 4, 0);

  for (size_t y = 0; y < depthLightmap; y++) {
    for (size_t x = 0; x < widthLightmap; x++) {
      const size_t src = (y * widthLightmap) + x;
      const size_t dst = 4 * ((y * widthLightmap) + x);
      lightmap[dst + 0] = uint8_t(_lightmap[src].r * 255.0f);
      lightmap[dst + 1] = uint8_t(_lightmap[src].g * 255.0f);
      lightmap[dst + 2] = uint8_t(_lightmap[src].b * 255.0f);
      lightmap[dst + 3] = uint8_t(_lightmap[src].a * 255.0f);
    }
  }

  return true;
}

void cHeightmapData::DoubleImageSize(const std::vector<spitfire::math::cColour>& source, size_t widthSource, size_t heightSource, std::vector<spitfire::math::cColour>& destination)
{
  const size_t widthDestination = 2 * widthSource;
  const size_t heightDestination = 2 * heightSource;
  const size_t n = widthDestination * heightDestination;

  destination.resize(n, spitfire::math::cColour());

  for (size_t y = 0; y < heightSource; y++) {
    for (size_t x = 0; x < widthSource; x++) {
      const spitfire::math::cColour& colour = source[(y * widthSource) + x];
      destination[(2 * ((y * widthDestination) + x)) + 0] = colour;
      destination[(2 * ((y * widthDestination) + x)) + 1] = colour;
      destination[widthDestination + (2 * ((y * widthDestination) + x)) + 0] = colour;
      destination[widthDestination + (2 * ((y * widthDestination) + x)) + 1] = colour;
    }
  }
}

void cHeightmapData::SmoothImage(const std::vector<spitfire::math::cColour>& source, size_t _width, size_t _height, size_t iterations, std::vector<spitfire::math::cColour>& destination) const
{
  const size_t n = _width * _height;

  destination.resize(n, spitfire::math::cColour());

  std::vector<spitfire::math::cColour> temp = source;

  spitfire::math::cColour surrounding[5];

  for (size_t i = 0; i < iterations; i++) {
    for (size_t y = 0; y < depthLightmap; y++) {
      for (size_t x = 0; x < widthLightmap; x++) {

        surrounding[0] = surrounding[1] = surrounding[2] = surrounding[3] = surrounding[4] = GetLightmapPixel(temp, x, y);

        // We sample from the 4 surrounding pixels in a cross shape
        if (x != 0) surrounding[0] = GetLightmapPixel(temp, x - 1, y);
        if ((y + 1) < depthLightmap) surrounding[1] = GetLightmapPixel(temp, x, y + 1);
        if (y != 0) surrounding[3] = GetLightmapPixel(temp, x, y - 1);
        if ((x + 1) < widthLightmap) surrounding[4] = GetLightmapPixel(temp, x + 1, y);

        //const spitfire::math::cColour averageOfSurrounding = 0.25f * (surrounding[0] + surrounding[1] + surrounding[3] + surrounding[4]);
        //const spitfire::math::cColour colour = 0.5f * (surrounding[2] + averageOfSurrounding);
        const spitfire::math::cColour colour = 0.25f * (surrounding[0] + surrounding[1] + surrounding[3] + surrounding[4]);

        const size_t index = (y * widthLightmap) + x;

        destination[index] = colour;
      }
    }


    // If we are still going then set temp to destination for the next iteration
    if ((i + 1) < iterations) temp = destination;
  }
}

spitfire::math::cColour cHeightmapData::GetLightmapPixel(const std::vector<spitfire::math::cColour>& _lightmap, size_t x, size_t y) const
{
  assert(x < widthLightmap);
  assert(y < depthLightmap);

  const size_t index = (y * widthLightmap) + x;
  return _lightmap[index];
}

float cHeightmapData::GetHeight(size_t x, size_t y) const
{
  if (((y * width) + x) >= heightmap.size()) return 0.0f;

  return heightmap[(y * depth) + x];
}

spitfire::math::cVec3 cHeightmapData::GetNormalOfTriangle(const spitfire::math::cVec3& p0, const spitfire::math::cVec3& p1, const spitfire::math::cVec3& p2) const
{
  const spitfire::math::cVec3 v0 = p1 - p0;
  const spitfire::math::cVec3 v1 = p2 - p0;

  return v0.CrossProduct(v1);
}

spitfire::math::cVec3 cHeightmapData::GetNormal(size_t x, size_t y, const spitfire::math::cVec3& scale) const
{
  assert(((y * depth) + x) < heightmap.size());

  // Get the height of the target point and the 4 heights in a cross shape around the target
  spitfire::math::cVec3 points[5];

  points[0] = points[1] = points[2] = points[3] = points[4] = spitfire::math::cVec3(float(x), float(y), GetHeight(x, y));

  // First column
  if (x != 0) points[0] = spitfire::math::cVec3(float(x - 1), float(y), GetHeight(x - 1, y));

  // Second column
  if ((x != 0) && (y != 0)) points[1] = spitfire::math::cVec3(float(x), float(y - 1), GetHeight(x, y - 1));
  //points[2] = spitfire::math::cVec3(float(x), float(y), GetHeight(x, y));
  if ((y + 1) < depth) points[3] = spitfire::math::cVec3(float(x), float(y + 1), GetHeight(x, y + 1));

  // Third column
  if ((x + 1) < width) points[4] = spitfire::math::cVec3(float(x + 1), float(y), GetHeight(x + 1, y));

  points[0] *= scale;
  points[1] *= scale;
  points[2] *= scale;
  points[3] *= scale;
  points[4] *= scale;

  spitfire::math::cVec3 normal;

  normal += GetNormalOfTriangle(points[0], points[2], points[1]);
  normal += GetNormalOfTriangle(points[0], points[2], points[3]);
  normal += GetNormalOfTriangle(points[4], points[2], points[1]);
  normal += GetNormalOfTriangle(points[4], points[2], points[3]);

  normal.Normalise();

  return normal;
}

const uint8_t* cHeightmapData::GetLightmapBuffer() const
{
  assert(!lightmap.empty());
  return &lightmap[0];
}

// http://www.cyberhead.de/download/articles/shadowmap/

void cHeightmapData::GenerateLightmap(const std::vector<float>& heightmap, std::vector<spitfire::math::cColour>& _lightmap, float fScaleY, const spitfire::math::cColour& ambientColour, const spitfire::math::cColour& shadowColour, size_t size, const spitfire::math::cVec3& sunPosition)
{
  assert(!_lightmap.empty());

  const size_t n = size * size;
  for (size_t index = 0; index < n; index++) {
    _lightmap[index] = ambientColour;
  }

  /*spitfire::math::cVec3 CurrentPos;
  spitfire::math::cVec3 LightDir;

  int LerpX, LerpZ;

  // For every pixel on the map
  for (size_t z = 0; z < size; ++z) {
  for (size_t x = 0; x < size; ++x) {
  // Set current position in terrain
  CurrentPos.Set(float(x), (heightmap[(z * size) + x] * fScaleY), float(z));

  // Calc new direction of lightray
  LightDir = (sunPosition - CurrentPos).GetNormalised();

  //Start the test
  while (
  (CurrentPos.x >= 0) &&
  (CurrentPos.x < size) &&
  (CurrentPos.z >= 0) &&
  (CurrentPos.z < size) &&
  (CurrentPos != sunPosition) && (CurrentPos.y < 255)
  ) {
  CurrentPos += LightDir;

  // Round the values up or down to the nearest int
  LerpX = round_up_or_down(CurrentPos.x);
  LerpZ = round_up_or_down(CurrentPos.z);

  const size_t index = (LerpZ * size) + LerpX;
  if (index < n) {
  // Hit?
  if (CurrentPos.y <= (heightmap[index] * fScaleY)) {
  _lightmap[(z * size) + x] = shadowColour;
  break;
  }
  }
  }
  }
  }*/
}
