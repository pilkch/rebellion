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
#include "util.h"

namespace util
{
  // This class is a derivate of basic_stringbuf which will output all the written data using the OutputDebugString function
  template<typename TChar, typename TTraits = std::char_traits<TChar>>
  class OutputDebugStringBuf : public std::basic_stringbuf<TChar,TTraits> {
  public:
    explicit OutputDebugStringBuf() : _buffer(256)
    {
      setg(nullptr, nullptr, nullptr);
      setp(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
    }

    ~OutputDebugStringBuf() {}

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

}


// ** cFreeLookCamera

cFreeLookCamera::cFreeLookCamera() :
fRotationRight(0.0f),
fRotationUp(0.0f)
{
}

spitfire::math::cVec3 cFreeLookCamera::GetPosition() const
{
  return position;
}

void cFreeLookCamera::SetPosition(const spitfire::math::cVec3& _position)
{
  position = _position;
}

void cFreeLookCamera::SetRotation(const spitfire::math::cQuaternion& rotation)
{
  const spitfire::math::cVec3 euler = rotation.GetEuler();
  fRotationRight = euler.x;
  fRotationUp = euler.z;
}

void cFreeLookCamera::LookAt(const spitfire::math::cVec3& eye, const spitfire::math::cVec3& target, const spitfire::math::cVec3 up)
{
  spitfire::math::cMat4 matView;
  matView.LookAt(eye, target, up);

  SetPosition(eye);
  SetRotation(matView.GetRotation());
}

void cFreeLookCamera::MoveX(float xmmod)
{
  const spitfire::math::cQuaternion rotation = GetRotation();
  position += (-rotation) * spitfire::math::cVec3(xmmod, 0.0f, 0.0f);
}

void cFreeLookCamera::MoveZ(float ymmod)
{
  const spitfire::math::cQuaternion rotation = GetRotation();
  position += (-rotation) * spitfire::math::cVec3(0.0f, 0.0f, -ymmod);
}

void cFreeLookCamera::RotateX(float xrmod)
{
  fRotationRight += xrmod;
}

void cFreeLookCamera::RotateY(float yrmod)
{
  fRotationUp += yrmod;
}

spitfire::math::cQuaternion cFreeLookCamera::GetRotation() const
{
  spitfire::math::cQuaternion up;
  up.SetFromAxisAngle(spitfire::math::cVec3(1.0f, 0.0f, 0.0f), spitfire::math::DegreesToRadians(fRotationUp));
  spitfire::math::cQuaternion right;
  right.SetFromAxisAngle(spitfire::math::cVec3(0.0f, 1.0f, 0.0f), spitfire::math::DegreesToRadians(fRotationRight));

  return (up * right);
}

spitfire::math::cMat4 cFreeLookCamera::CalculateViewMatrix() const
{
  spitfire::math::cMat4 matTranslation;
  matTranslation.TranslateMatrix(-position);

  const spitfire::math::cQuaternion rotation = -GetRotation();
  const spitfire::math::cMat4 matRotation = rotation.GetMatrix();

  spitfire::math::cMat4 matTargetTranslation;
  matTargetTranslation.TranslateMatrix(spitfire::math::cVec3(0.0f, 0.0f, 1.0f));

  return ((matTargetTranslation * matRotation) * matTranslation);
}

spitfire::math::cMat4 cFreeLookCamera::CalculateLookAtMatrix() const
{
  // TODO: CHECK IF THIS DOES THE SAME THING AS THE ABOVE?

  const spitfire::math::cVec3 eye = position;

  const spitfire::math::cQuaternion rotation = GetRotation();
  const spitfire::math::cVec3 target = position + ((-rotation) * spitfire::math::cVec3(0.0f, 0.0f, -1.0f));

  const spitfire::math::cVec3 up = (-rotation) * spitfire::math::cVec3(0.0f, 1.0f, 0.0f);

  spitfire::math::cMat4 matView;
  matView.LookAt(eye, target, up);

  return matView;
}


// ** cSimplePostRenderShader

cSimplePostRenderShader::cSimplePostRenderShader(const spitfire::string_t _sName, const spitfire::string_t& _sFragmentShaderFilePath) :
sName(_sName),
sFragmentShaderFilePath(_sFragmentShaderFilePath),
bOn(false)
{
}


namespace util
{
  spitfire::math::cColour ChangeLuminance(const spitfire::math::cColour& colourRGB, float fLuminanceDifferenceMinusOneToPlusOne)
  {
    spitfire::math::cColourHSL colourHSL;
    colourHSL.SetFromRGBA(colourRGB);
    colourHSL.fLuminance0To1 += fLuminanceDifferenceMinusOneToPlusOne;
    return colourHSL.GetRGBA();
  }
}
