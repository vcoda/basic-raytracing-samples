#include "vulkanRtApp.h"
#include "utilities.h"

VulkanRayTracingApp::VulkanRayTracingApp(const AppEntry& entry, const std::tstring& caption, uint32_t width, uint32_t height):
    PlatformApp(entry, caption, width, height),
    timer(std::make_unique<Timer>()),
    backbufferFormat{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    vSync(false),
    presentWait(PresentationWait::Fence),
    frameIndex(0),
    bufferIndex(0),
    frameCount(0)
{
    createInstance();
    createLogicalDevice();
    createSwapchain();
    createRenderPass();
    createFramebuffer();
    createCommandBuffers();
    createSyncPrimitives();
    createDescriptorPool();
    createDescriptorSets();
    createUniformBuffers();
    pipelineCache = std::make_unique<magma::PipelineCache>(device);
    shaderReflectionFactory = std::make_unique<ShaderReflectionFactory>(device);
}

VulkanRayTracingApp::~VulkanRayTracingApp() {}

void VulkanRayTracingApp::close()
{
    device->waitIdle();
    quit = true;
}

void VulkanRayTracingApp::onIdle()
{
    onPaint();
}

void VulkanRayTracingApp::onPaint()
{
    // Buffer index isn't guaranteed to be ordered, the sequence [0, 1, 2, 0, 2, 1...] is legal
    bufferIndex = swapchain->acquireNextImage(presentFinished[frameIndex]);
    if (PresentationWait::Fence == presentWait)
        waitFences[frameIndex]->reset();
    render(bufferIndex);
    graphicsQueue->present(swapchain, bufferIndex, renderFinished[frameIndex]);
    switch (presentWait)
    {
    case PresentationWait::Fence:
        waitFences[frameIndex]->wait();
        /*
        if (waitFence)
        {
            (*waitFence)->wait();
            graphicsQueue->onIdle();
        } */
        break;
    case PresentationWait::Queue:
        graphicsQueue->waitIdle();
        break;
    case PresentationWait::Device:
        // vkDeviceWaitIdle is equivalent to calling vkQueueWaitIdle
        // for all queues owned by device.
        device->waitIdle();
        break;
    }
    if (!vSync)
    {   // Cap fps
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // Round robin frame-in-flight
    frameIndex = (frameIndex + 1) % swapchain->getImageCount();
    ++frameCount;
}

void VulkanRayTracingApp::createInstance()
{
    std::vector<const char*> layerNames;
#ifdef MAGMA_DEBUG
    std::unique_ptr<magma::InstanceLayers> instanceLayers = std::make_unique<magma::InstanceLayers>();
    if (instanceLayers->KHRONOS_validation)
        layerNames.push_back("VK_LAYER_KHRONOS_validation");
    else if (instanceLayers->LUNARG_standard_validation)
        layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif // MAGMA_DEBUG

    magma::NullTerminatedStringArray enabledExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    #elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    #elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
    #endif // VK_USE_PLATFORM_XCB_KHR
    };
    instanceExtensions = std::make_unique<magma::InstanceExtensions>();
#ifdef VK_KHR_get_physical_device_properties2
    if (instanceExtensions->KHR_get_physical_device_properties2)
        enabledExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif // VK_KHR_get_physical_device_properties2
#ifdef VK_EXT_debug_utils
    if (instanceExtensions->EXT_debug_utils)
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    else
#endif //  VK_EXT_debug_utils
    {
    #ifdef VK_EXT_debug_report
        if (instanceExtensions->EXT_debug_report)
            enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    #endif
    }
    
    MAGMA_VLA(char, appName, caption.length() + 1);
#ifdef VK_USE_PLATFORM_WIN32_KHR
    size_t count = 0;
    wcstombs_s(&count, appName, appName.length(), caption.c_str(), appName.length());
#else
    strcpy(appName.data(), caption.c_str());
#endif
    const magma::Application appInfo(
        appName, 1,
        "Magma", 1,
        VK_API_VERSION_1_1);

#ifdef MAGMA_DEBUG
    hostAllocator = std::make_shared<magma::DebugAlignedAllocator>();
#else
    hostAllocator = std::make_shared<magma::AlignedAllocator>();
#endif

    instance = std::make_unique<magma::Instance>(layerNames, enabledExtensions, hostAllocator, &appInfo, 0,
    #ifdef VK_EXT_debug_report
        utilities::reportCallback,
    #endif
    #ifdef VK_EXT_debug_utils
        nullptr,
    #endif
        nullptr); // userData
#ifdef VK_EXT_debug_report
    if (instanceExtensions->EXT_debug_report)
    {
        debugReportCallback = std::make_unique<magma::DebugReportCallback>(
            instance.get(), utilities::reportCallback, hostAllocator);
    }
#endif // VK_EXT_debug_report

    physicalDevice = instance->getPhysicalDevice(0);
    const VkPhysicalDeviceProperties& properties = physicalDevice->getProperties();
    std::cout << "Running on " << properties.deviceName << "\n";
    extensions = std::make_unique<magma::DeviceExtensions>(physicalDevice);
}

void VulkanRayTracingApp::createLogicalDevice()
{
    const magma::DeviceQueueDescriptor graphicsQueueDesc(physicalDevice.get(), VK_QUEUE_GRAPHICS_BIT, magma::QueuePriorityHighest);
    const magma::DeviceQueueDescriptor computeQueueDesc(physicalDevice.get(), VK_QUEUE_COMPUTE_BIT, magma::QueuePriorityHighest);
    const magma::DeviceQueueDescriptor transferQueueDesc(physicalDevice.get(), VK_QUEUE_TRANSFER_BIT, magma::QueuePriorityDefault);
    std::set<magma::DeviceQueueDescriptor> queueDescriptors;
    queueDescriptors.insert(graphicsQueueDesc);
    queueDescriptors.insert(computeQueueDesc);
    queueDescriptors.insert(transferQueueDesc);

    VkPhysicalDeviceFeatures features = {};
    features.fillModeNonSolid = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    features.textureCompressionBC = VK_TRUE;
    features.shaderInt64 = VK_TRUE;

    magma::NullTerminatedStringArray enabledExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };
    if (extensions->KHR_maintenance1)
        enabledExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    else if (extensions->AMD_negative_viewport_height)
        enabledExtensions.push_back(VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME);

    magma::StructureChain extendedFeatures;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.pNext = nullptr;
    accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_FALSE;
    accelerationStructureFeatures.accelerationStructureIndirectBuild = VK_FALSE;
    accelerationStructureFeatures.accelerationStructureHostCommands = VK_FALSE;
    accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;
    extendedFeatures.linkNode(accelerationStructureFeatures);

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.pNext = nullptr;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
    rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
    rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
    rayTracingPipelineFeatures.rayTraversalPrimitiveCulling = VK_FALSE;
    extendedFeatures.linkNode(rayTracingPipelineFeatures);

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.pNext = nullptr;
    rayQueryFeatures.rayQuery = VK_TRUE;
    extendedFeatures.linkNode(rayQueryFeatures);

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures;
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
    bufferDeviceAddressFeatures.pNext = nullptr;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
    bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay = VK_FALSE;
    bufferDeviceAddressFeatures.bufferDeviceAddressMultiDevice = VK_FALSE;
    extendedFeatures.linkNode(bufferDeviceAddressFeatures);

    const std::vector<const char*> noLayers;
    device = physicalDevice->createDevice(queueDescriptors, noLayers, enabledExtensions, features, extendedFeatures);

    std::shared_ptr<magma::IDeviceMemoryAllocator> deviceAllocator = std::make_shared<magma::DeviceMemoryAllocator>(device, hostAllocator);
    allocator = std::make_shared<magma::Allocator>(hostAllocator, std::move(deviceAllocator));
}

void VulkanRayTracingApp::createSwapchain()
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    surface = std::make_unique<magma::Win32Surface>(instance.get(), hInstance, hWnd, hostAllocator);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    surface = std::make_unique<magma::XlibSurface>(instance.get(), dpy, window, hostAllocator);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    surface = std::make_unique<magma::XcbSurface>(instance.get(), connection, window, hostAllocator);
#endif // VK_USE_PLATFORM_XCB_KHR
    const magma::DeviceQueueDescriptor graphicsQueueDesc(physicalDevice.get(), VK_QUEUE_GRAPHICS_BIT, magma::QueuePriorityHighest);
    if (!physicalDevice->getSurfaceSupport(surface, graphicsQueueDesc.queueFamilyIndex))
        throw std::runtime_error("surface not supported");
    // Get surface caps
    VkSurfaceCapabilitiesKHR surfaceCaps;
    surfaceCaps = physicalDevice->getSurfaceCapabilities(surface);
    assert(surfaceCaps.currentExtent.width == width);
    assert(surfaceCaps.currentExtent.height == height);
    // Find supported transform flags
    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = surfaceCaps.currentTransform;
    // Find supported alpha composite
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (const auto alphaFlag : {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    {
        if (surfaceCaps.supportedCompositeAlpha & alphaFlag)
        {
            compositeAlpha = alphaFlag;
            break;
        }
    }
    // Try to use RGB10_A2 format if supported
    const std::vector<VkSurfaceFormatKHR> surfaceFormats = physicalDevice->getSurfaceFormats(surface);
    auto it = std::find_if(surfaceFormats.begin(), surfaceFormats.end(),
        [](const VkSurfaceFormatKHR& surfaceFormat) {
            return (VK_FORMAT_A2B10G10R10_UNORM_PACK32 == surfaceFormat.format);
        });
    if (it != surfaceFormats.end())
        backbufferFormat = *it;
    else // Fallback to first supported format
        backbufferFormat = surfaceFormats[0];
    // Choose available present mode
    const std::vector<VkPresentModeKHR> presentModes = physicalDevice->getSurfacePresentModes(surface);
    VkPresentModeKHR presentMode;
    if (vSync)
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
    else
    {   // Search for first appropriate present mode
        bool found = false;
        for (const auto mode : {
            VK_PRESENT_MODE_IMMEDIATE_KHR,  // AMD
            VK_PRESENT_MODE_MAILBOX_KHR,    // NVidia, Intel
            VK_PRESENT_MODE_FIFO_RELAXED_KHR})
        {
            if (std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end())
            {
                presentMode = mode;
                found = true;
                break;
            }
        }
        if (!found)
        {   // Must always be present
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
    }
    VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const VkSurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice->getSurfaceCapabilities(surface);
    if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
        imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT; // Can trace rays directly to back buffer
    magma::Swapchain::Initializer initializer;
    initializer.debugReportCallback = debugReportCallback.get();
    swapchain = std::make_unique<magma::Swapchain>(device, surface,
        std::max(surfaceCaps.minImageCount, 2U),
        backbufferFormat, surfaceCaps.currentExtent, 1,
        imageUsageFlags, preTransform, compositeAlpha, presentMode,
        initializer, hostAllocator);
    for (const auto& image: swapchain->getImages())
    {
        std::shared_ptr<magma::ImageView> imageView = std::make_shared<magma::SharedImageView>(std::move(image));
        swapchainImageViews.emplace_back(std::move(imageView));
    }
}

void VulkanRayTracingApp::createRenderPass()
{
    const magma::AttachmentDescription colorAttachment(backbufferFormat.format,
        1, magma::op::store, magma::op::dontCare,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    renderPass = std::make_unique<magma::RenderPass>(device, colorAttachment, hostAllocator);
}

void VulkanRayTracingApp::createFramebuffer()
{
    for (auto const& attachment: swapchainImageViews)
    {
        std::unique_ptr<magma::Framebuffer> framebuffer = std::make_unique<magma::Framebuffer>(renderPass, attachment, hostAllocator);
        framebuffers.emplace_back(std::move(framebuffer));
    }
}

void VulkanRayTracingApp::createCommandBuffers()
{
    graphicsQueue = device->getQueue(VK_QUEUE_GRAPHICS_BIT, 0);
    computeQueue = device->getQueue(VK_QUEUE_COMPUTE_BIT, 0);
    commandPools[0] = std::make_unique<magma::CommandPool>(device, graphicsQueue->getFamilyIndex(), hostAllocator);
    commandPools[1] = std::make_unique<magma::CommandPool>(device, computeQueue->getFamilyIndex(), hostAllocator);
    // Create draw command buffers
    commandBuffers = commandPools[0]->allocateCommandBuffers(VK_COMMAND_BUFFER_LEVEL_PRIMARY, magma::core::countof(framebuffers));
    // Create image copy command buffer
    cmdImageCopy = std::make_unique<magma::PrimaryCommandBuffer>(commandPools[0]);
    // Create command buffer used for build acceleration structures in compute queue
    cmdCompute = std::make_unique<magma::PrimaryCommandBuffer>(commandPools[1]);
    try
    {
        transferQueue = device->getQueue(VK_QUEUE_TRANSFER_BIT, 0);
        if (transferQueue)
        {
            commandPools[2] = std::make_unique<magma::CommandPool>(device, transferQueue->getFamilyIndex(), hostAllocator);
            // Create buffer copy command buffer
            cmdBufferCopy = std::make_unique<magma::PrimaryCommandBuffer>(commandPools[2]);
        }
    } catch (...) { std::cout << "transfer queue not present" << std::endl; }
}

void VulkanRayTracingApp::createSyncPrimitives()
{
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i)
    {
        presentFinished.push_back(std::make_unique<magma::Semaphore>(device, hostAllocator));
        renderFinished.push_back(std::make_unique<magma::Semaphore>(device, hostAllocator));
        waitFences.emplace_back(std::make_unique<magma::Fence>(device, hostAllocator, VK_FENCE_CREATE_SIGNALED_BIT));
    }
}

void VulkanRayTracingApp::createDescriptorPool()
{
    constexpr uint32_t maxDescriptorSets = 10;
    // Allocate descriptor pool with enough size for basic samples
    descriptorPool = std::make_shared<magma::DescriptorPool>(device, maxDescriptorSets,
        std::vector<magma::descriptor::DescriptorPool>{
            magma::descriptor::UniformBufferPool(4),
            magma::descriptor::DynamicUniformBufferPool(4),
            magma::descriptor::StorageBufferPool(10),
            magma::descriptor::CombinedImageSamplerPool(4),
            magma::descriptor::AccelerationStructurePool(10)
        }, hostAllocator);
}

void VulkanRayTracingApp::createDescriptorSets()
{   // General layout of storage image is required
    swapchain->layoutTransition(VK_IMAGE_LAYOUT_GENERAL, cmdImageCopy,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    uint32_t index = 0;
    for (auto& imageView: swapchainImageViews)
    {
        swapchainImageTables[index].output = imageView;
        auto descriptorSet = std::make_unique<magma::DescriptorSet>(descriptorPool,
            swapchainImageTables[index++], VK_SHADER_STAGE_RAYGEN_BIT_KHR, hostAllocator);
        swapchainDescriptorSets.emplace_back(std::move(descriptorSet));
    }
    // Make sure that recorded command buffer will perform present -> general layout transition
    swapchain->layoutTransition(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, cmdImageCopy,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}

void VulkanRayTracingApp::createUniformBuffers()
{
    viewUniforms = std::make_unique<magma::UniformBuffer<View>>(device, allocator);
}

std::unique_ptr<magma::Buffer> VulkanRayTracingApp::allocateScratchBuffer(VkDeviceSize size)
{
    const VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = physicalDevice->getAccelerationStructureProperties();
    const VkDeviceSize alignedSize = magma::core::alignUp(size, (VkDeviceSize)accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);
    return std::make_unique<magma::AccelerationStructureStorageBuffer>(device, alignedSize, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, allocator);
}

void VulkanRayTracingApp::submitCommandBuffer(uint32_t bufferIndex)
{
    const std::unique_ptr<magma::Fence>& frameFence = (PresentationWait::Fence == presentWait) ? waitFences[frameIndex] : nullFence;
    graphicsQueue->submit(commandBuffers[bufferIndex],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        presentFinished[frameIndex], // Wait for swapchain
        renderFinished[frameIndex], // Semaphore to be signaled when command buffer completed execution
        frameFence); // Fence to be signaled when command buffer completed execution
}

void VulkanRayTracingApp::submitCopyImageCommands()
{
    waitFences[0]->reset();
    graphicsQueue->submit(cmdImageCopy, 0, nullptr, nullptr, waitFences[0]);
    waitFences[0]->wait();
}

void VulkanRayTracingApp::submitCopyBufferCommands()
{
    waitFences[1]->reset();
    transferQueue->submit(cmdBufferCopy, 0, nullptr, nullptr, waitFences[1]);
    waitFences[1]->wait();
}
