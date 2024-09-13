#pragma once

#ifdef VK_USE_PLATFORM_WIN32_KHR
#include "winApp.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#include "xlibApp.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include "xcbApp.h"
#endif // VK_USE_PLATFORM_XCB_KHR

#include "magma/magma.h"
#include "rapid/rapid.h"
#include "shaderReflectionFactory.h"
#include "rayTracingPipeline.h"
#include "timer.h"

#if !defined(VK_KHR_acceleration_structure) ||\
    !defined(VK_KHR_ray_tracing_pipeline) ||\
    !defined(VK_KHR_ray_query) ||\
    !defined(VK_KHR_deferred_host_operations) ||\
    !defined(VK_KHR_buffer_device_address) ||\
    !defined(VK_KHR_spirv_1_4) ||\
    !defined(VK_KHR_shader_float_controls) ||\
    !defined(VK_EXT_descriptor_indexing)
#error Newer Vulkan SDK is required
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
typedef Win32App PlatformApp;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
typedef XlibApp PlatformApp;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
typedef XcbApp PlatformApp;
#endif // VK_USE_PLATFORM_XCB_KHR

class VulkanRayTracingApp : public PlatformApp
{
public:
    enum Buffer : uint8_t;
    enum class PresentationWait : uint8_t;

    struct View
    {
        rapid::matrix viewInv;
        rapid::matrix projInv;
        rapid::matrix viewProjInv;
    };

    VulkanRayTracingApp(const AppEntry& entry, const std::tstring& caption, uint32_t width, uint32_t height);
    ~VulkanRayTracingApp();
    void close() override;
    virtual void render(uint32_t bufferIndex) = 0;
    virtual void onIdle() override;
    virtual void onPaint() override;

protected:
    void createInstance();
    void createLogicalDevice();
    void createRenderPass();
    void createSwapchain();
    void createFramebuffer();
    void createCommandBuffers();
    void createSyncPrimitives();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();

    std::shared_ptr<magma::Buffer> allocateScratchBuffer(VkDeviceSize size);
    void submitCommandBuffer(uint32_t bufferIndex);
    void submitCopyImageCommands();
    void submitCopyBufferCommands();

    std::shared_ptr<magma::IAllocator> allocator;
    std::shared_ptr<magma::Instance> instance;
    std::shared_ptr<magma::DebugReportCallback> debugReportCallback;
    std::shared_ptr<magma::Surface> surface;
    std::shared_ptr<magma::PhysicalDevice> physicalDevice;
    std::shared_ptr<magma::Device> device;
    std::unique_ptr<magma::Swapchain> swapchain;
    std::unique_ptr<magma::InstanceExtensions> instanceExtensions;
    std::unique_ptr<magma::DeviceExtensions> extensions;
    std::vector<std::shared_ptr<magma::ImageView>> swapchainImageViews;
    std::shared_ptr<magma::RenderPass> renderPass;
    std::vector<std::shared_ptr<magma::Framebuffer>> framebuffers;

    std::shared_ptr<magma::CommandPool> commandPools[3];
    std::vector<std::shared_ptr<magma::CommandBuffer>> commandBuffers;
    std::shared_ptr<magma::CommandBuffer> cmdImageCopy;
    std::shared_ptr<magma::CommandBuffer> cmdBufferCopy;
    std::shared_ptr<magma::CommandBuffer> cmdCompute;

    std::shared_ptr<magma::Queue> graphicsQueue;
    std::shared_ptr<magma::Queue> computeQueue;
    std::shared_ptr<magma::Queue> transferQueue;
    std::shared_ptr<magma::Semaphore> presentFinished;
    std::shared_ptr<magma::Semaphore> renderFinished;
    std::vector<std::unique_ptr<magma::Fence>> waitFences;
    const std::unique_ptr<magma::Fence> nullFence;
    const std::unique_ptr<magma::Fence> *waitFence;

    std::shared_ptr<magma::DescriptorPool> descriptorPool;
    std::vector<std::shared_ptr<magma::DescriptorSet>> swapchainDescriptorSets;
    std::shared_ptr<magma::UniformBuffer<View>> viewUniforms;
    std::shared_ptr<magma::Buffer> scratchBuffer;

    std::shared_ptr<magma::PipelineCache> pipelineCache;
    std::shared_ptr<ShaderReflectionFactory> shaderReflectionFactory;

    std::unique_ptr<Timer> timer;
    VkSurfaceFormatKHR backbufferFormat;
    bool vSync;
    PresentationWait presentWait;
    uint32_t bufferIndex;
    uint32_t frameIndex;

private:
    struct SwapchainImageTable : magma::DescriptorSetTable
    {
        magma::descriptor::StorageImage output = 0;
        MAGMA_REFLECT(output)
    } swapchainImageTables[3];
};

enum VulkanRayTracingApp::Buffer : uint8_t
{
    Front, Back
};

enum class VulkanRayTracingApp::PresentationWait : uint8_t
{
    Fence, Queue, Device
};
