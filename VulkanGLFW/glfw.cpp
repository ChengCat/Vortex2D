//
//  GLFW.cpp
//  Vortex2D
//

#include "glfw.h"

#include <stdexcept>
#include <string>
#include <iostream>

static void ErrorCallback(int error, const char* description)
{
    throw std::runtime_error("GLFW Error: " +
                             std::to_string(error) +
                             " What: " +
                             std::string(description));
}

static PFN_vkCreateDebugReportCallbackEXT pfn_vkCreateDebugReportCallbackEXT;
VkResult vkCreateDebugReportCallbackEXT(VkInstance instance,
                                        const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
                                        const VkAllocationCallbacks* pAllocator,
                                        VkDebugReportCallbackEXT* pCallback)
{
    return pfn_vkCreateDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback);
}

static PFN_vkDestroyDebugReportCallbackEXT pfn_vkDestroyDebugReportCallbackEXT;
void vkDestroyDebugReportCallbackEXT(VkInstance instance,
                                     VkDebugReportCallbackEXT callback,
                                     const VkAllocationCallbacks* pAllocator)
{
    pfn_vkDestroyDebugReportCallbackEXT(instance, callback, pAllocator);
}


VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
                                             VkDebugReportObjectTypeEXT objType,
                                             uint64_t obj,
                                             size_t location,
                                             int32_t code,
                                             const char* layerPrefix,
                                             const char* msg,
                                             void* userData)
{
    std::cout << "validation layer: " << msg << std::endl;
    return VK_FALSE;
}

static const std::vector<const char*> validationLayers = {"VK_LAYER_LUNARG_standard_validation"};
static const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;

    bool IsValid() const
    {
        return !formats.empty() &&
                !presentModes.empty();
    }
};

GLFWApp::GLFWApp(uint32_t width, uint32_t height, bool visible, bool validation)
    : mWidth(width)
    , mHeight(height)
    , mValidation(validation)
{
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit())
    {
        throw std::runtime_error("Could not initialise GLFW!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    mWindow = glfwCreateWindow(width, height, "Vortex2D App", nullptr, nullptr);
    if (!mWindow)
    {
        throw std::runtime_error("Error creating GLFW Window");
    }

    std::vector<const char*> extensions;
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions;

    // get the required extensions from GLFW
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (int i = 0; i < glfwExtensionCount; i++)
    {
        extensions.push_back(glfwExtensions[i]);
    }

    // add the validation extension if necessary
    if (validation)
    {
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // configure instance
    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("Vortex2D App");

    vk::InstanceCreateInfo instanceInfo;
    instanceInfo
            .setPApplicationInfo(&appInfo)
            .setEnabledExtensionCount(extensions.size())
            .setPpEnabledExtensionNames(extensions.data());

    // add the validation layer if necessary
    if (validation)
    {
        instanceInfo.setEnabledLayerCount(validationLayers.size());
        instanceInfo.setPpEnabledLayerNames(validationLayers.data());
    }

    mInstance = vk::createInstanceUnique(instanceInfo);

    // add the validation calback if necessary
    if (validation)
    {
        pfn_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT) mInstance->getProcAddr("vkCreateDebugReportCallbackEXT");
        pfn_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT) mInstance->getProcAddr("vkDestroyDebugReportCallbackEXT");

        vk::DebugReportCallbackCreateInfoEXT debugCallbackInfo;
        debugCallbackInfo
                .setPfnCallback(debugCallback)
                .setFlags(vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::eError);

        mDebugCallback = mInstance->createDebugReportCallbackEXTUnique(debugCallbackInfo);
    }

    // create surface
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*mInstance, mWindow, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    mSurface = vk::UniqueSurfaceKHR(surface, vk::SurfaceKHRDeleter(*mInstance));

    // TODO better search than first available device
    // - using swap chain info
    // - using queue info
    // - discrete GPU
    vk::PhysicalDevice physicalDevice = mInstance->enumeratePhysicalDevices().at(0);
    auto properties = physicalDevice.getProperties();
    std::cout << "Device name: " << properties.deviceName << std::endl;

    // get swap chain support details
    SwapChainSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*mSurface);
    details.formats = physicalDevice.getSurfaceFormatsKHR(*mSurface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(*mSurface);

    if (!details.IsValid())
    {
        throw std::runtime_error("Swap chain support invalid");
    }

    // find queue that has compute, graphics and surface support
    int index = GetFamilyIndex(physicalDevice);

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo deviceQueueInfo;
    deviceQueueInfo
            .setQueueFamilyIndex(index)
            .setQueueCount(1)
            .setPQueuePriorities(&queuePriority);

    vk::PhysicalDeviceFeatures deviceFeatures;
    vk::DeviceCreateInfo deviceInfo;
    deviceInfo
            .setQueueCreateInfoCount(1)
            .setPQueueCreateInfos(&deviceQueueInfo)
            .setPEnabledFeatures(&deviceFeatures)
            .setEnabledExtensionCount(deviceExtensions.size())
            .setPpEnabledExtensionNames(deviceExtensions.data());

    // add the validation layer if necessary
    if (mValidation)
    {
        deviceInfo.setEnabledLayerCount(validationLayers.size());
        deviceInfo.setPpEnabledLayerNames(validationLayers.data());
    }

    mDevice = physicalDevice.createDeviceUnique(deviceInfo);
    mQueue = mDevice->getQueue(index, 0);

    vk::SwapchainCreateInfoKHR swapChainInfo;
    swapChainInfo
            .setSurface(*mSurface)
            // TODO choose given the details
            .setImageFormat(vk::Format::eB8G8R8A8Unorm)
            .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
            .setMinImageCount(details.capabilities.minImageCount + 1)
            .setImageExtent({mWidth, mHeight})
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
            .setImageSharingMode(vk::SharingMode::eExclusive)
            .setPreTransform(details.capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            // TODO find if better mode is available
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(true);

    mSwapChain = mDevice->createSwapchainKHRUnique(swapChainInfo);

    // TODO this might need to be moved in RenderWindow or something
    std::vector<vk::Image> swapChainImages = mDevice->getSwapchainImagesKHR(*mSwapChain);
    std::vector<vk::UniqueImageView> swapChainImageViews;
    for (const auto& image : swapChainImages)
    {
        vk::ImageViewCreateInfo imageViewInfo;
        imageViewInfo
                .setImage(image)
                .setViewType(vk::ImageViewType::e2D)
                // TODO set same as for swapChainInfo
                .setFormat(vk::Format::eB8G8R8A8Unorm)
                .setComponents({vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity})
                .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0 ,1});

       swapChainImageViews.push_back(mDevice->createImageViewUnique(imageViewInfo));
    }

    // Create command buffer pool
    vk::CommandPoolCreateInfo commandPoolInfo;
    commandPoolInfo.setQueueFamilyIndex(index);
    auto commandPool = mDevice->createCommandPoolUnique(commandPoolInfo);

}

GLFWApp::~GLFWApp()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void GLFWApp::Run()
{
    while (!glfwWindowShouldClose(mWindow))
    {
        glfwPollEvents();
    }
}

void GLFWApp::SetKeyCallback(GLFWkeyfun cbfun)
{
    glfwSetKeyCallback(mWindow, cbfun);
}

Vortex2D::Renderer::Device GLFWApp::GetDevice()
{
    return {*mDevice, mQueue};
}

int GLFWApp::GetFamilyIndex(vk::PhysicalDevice physicalDevice)
{
    int index = -1;
    const auto& familyProperties = physicalDevice.getQueueFamilyProperties();
    for (int i = 0; i < familyProperties.size(); i++)
    {
        const auto& property = familyProperties[i];
        if (property.queueFlags & vk::QueueFlagBits::eCompute &&
                property.queueFlags & vk::QueueFlagBits::eGraphics &&
                physicalDevice.getSurfaceSupportKHR(i, *mSurface))
        {
            index = i;
        }
    }

    if (index == -1)
    {
        throw std::runtime_error("Suitable physical device not found");
    }

    return index;
}