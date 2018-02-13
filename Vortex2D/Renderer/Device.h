//
//  Device.h
//  Vortex2D
//

#ifndef Vortex2d_Device_h
#define Vortex2d_Device_h

#include <Vortex2D/Renderer/Common.h>
#include <Vortex2D/Renderer/DescriptorSet.h>
#include <Vortex2D/Utils/vk_mem_alloc.h>
#include <map>

namespace Vortex2D { namespace Renderer {

class SpirvBinary
{
public:

    template<std::size_t N>
    SpirvBinary(const uint32_t (&spirv)[N])
        : mData(spirv)
        , mSize(N * 4)
    {
    }

    const uint32_t* data() const
    {
        return mData;
    }

    std::size_t size() const
    {
        return mSize;
    }

    std::size_t words() const
    {
        return mSize / 4;
    }

private:
    const uint32_t* mData;
    std::size_t mSize;
};

class Device
{
public:
    Device(vk::PhysicalDevice physicalDevice, bool validation = true);
    Device(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, bool validation = true);
    Device(vk::PhysicalDevice physicalDevice, int familyIndex, bool validation = true);
    ~Device();

    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    // Vulkan handles and helpers
    vk::Device Handle() const;
    vk::Queue Queue() const;
    vk::PhysicalDevice GetPhysicalDevice() const;
    int GetFamilyIndex() const;

    // Command buffer functions
    std::vector<vk::CommandBuffer> CreateCommandBuffers(uint32_t size) const;
    void FreeCommandBuffers(vk::ArrayProxy<const vk::CommandBuffer> commandBuffers) const;

    // Memory allocator
    VmaAllocator Allocator() const;
    LayoutManager& GetLayoutManager() const;

    vk::ShaderModule GetShaderModule(const SpirvBinary& spirv) const;

private:
    vk::PhysicalDevice mPhysicalDevice;
    int mFamilyIndex;
    vk::UniqueDevice mDevice;
    vk::Queue mQueue;
    vk::UniqueCommandPool mCommandPool;
    vk::UniqueDescriptorPool mDescriptorPool;
    VmaAllocator mAllocator;

    mutable std::map<const uint32_t*, vk::UniqueShaderModule> mShaders;
    mutable LayoutManager mLayoutManager;
};

}}

#endif
