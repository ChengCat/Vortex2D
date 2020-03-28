//
//  Common.h
//  Vortex
//

#ifndef Vortex2D_Common_h
#define Vortex2D_Common_h

#include <Vortex2D/Renderer/Gpu.h>

#include <glm/mat2x2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#ifdef _WIN32
#ifdef VORTEX2D_API_EXPORTS
#define VORTEX2D_API __declspec(dllexport)
#else
#define VORTEX2D_API __declspec(dllimport)
#endif
#else
#define VORTEX2D_API
#endif

namespace Vortex2D
{
namespace Renderer
{
/**
 * @brief A binary SPIRV shader, to be feed to vulkan.
 */
class SpirvBinary
{
public:
  template <std::size_t N>
  SpirvBinary(const uint32_t (&spirv)[N]) : mData(spirv), mSize(N * 4)
  {
  }

  const uint32_t* data() const { return mData; }

  std::size_t size() const { return mSize; }

  std::size_t words() const { return mSize / 4; }

private:
  const uint32_t* mData;
  std::size_t mSize;
};

}  // namespace Renderer
}  // namespace Vortex2D

#endif
