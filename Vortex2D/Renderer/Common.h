//
//  Common.h
//  Vortex
//

#ifndef Vortex_Common_h
#define Vortex_Common_h

#include <vulkan/vulkan.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_SWIZZLE
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/mat2x2.hpp>
#include <glm/mat4x4.hpp>

#define GLSL(src) "#version 450 core\n" #src

#endif
