#include <rtdxc/rtdxc.hpp>

#define WIN32_LEAN_AND_MEAN
#include <tchar.h>
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

static rtdxc::runtime _runtime;

// ------------------------- Config -------------------------
static const uint32_t kMinImageCount = 2; // double-buffering
static const int kMaxFramesInFlight = 2; // match min image count

// ------------------- Global State (simple) ----------------
static HINSTANCE g_hInstance = nullptr;
static HWND g_hWnd = nullptr;
static bool g_appRunning = true;
static bool g_swapchainRebuild = false;
static UINT g_width = 1280, g_height = 720;
static VkInstance g_instance = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT g_debugMessenger = VK_NULL_HANDLE; // optional (validation)
static VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static uint32_t g_queueFamily = 0;
static VkQueue g_queue = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkFormat g_swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
static VkColorSpaceKHR g_colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
static VkExtent2D g_swapchainExtent = { g_width, g_height };
static std::vector<VkImage> g_swapchainImages;
static std::vector<VkImageView> g_swapchainImageViews;
static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> g_framebuffers;
static VkCommandPool g_cmdPool = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> g_cmdBuffers; // one per swapchain image

struct FrameSync {
    VkSemaphore imageAcquired = VK_NULL_HANDLE;
    VkSemaphore renderComplete = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
};
static FrameSync g_frames[kMaxFramesInFlight];
static uint32_t g_frameIndex = 0;

static VkDescriptorPool g_imguiDescPool = VK_NULL_HANDLE;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void CreateWin32Window(const wchar_t* title, uint32_t width, uint32_t height);
static void DestroyWin32Window();

static void CreateVulkan();
static void DestroyVulkan();
static void CreateSwapchainResources();
static void DestroySwapchainResources();
static void RecreateSwapchain();

static void CheckVk(VkResult err, const char* where)
{
    if (err != VK_SUCCESS) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Vulkan error %d at %s", (int)err, where);
        MessageBoxA(nullptr, buf, "Vulkan Error", MB_ICONERROR | MB_OK);
        throw std::runtime_error(buf);
    }
}

// --------------------- Win32 window -----------------------
static void CreateWin32Window(const wchar_t* title, uint32_t width, uint32_t height)
{
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandleW(NULL), NULL, NULL, NULL, NULL,
        L"ImGuiWin32VulkanClass", NULL };
    RegisterClassExW(&wc);
    g_hInstance = wc.hInstance;

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&r, style, FALSE);

    g_hWnd = CreateWindowW(wc.lpszClassName, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, wc.hInstance, NULL);
}

static void DestroyWin32Window()
{
    DestroyWindow(g_hWnd);
    UnregisterClassW(L"ImGuiWin32VulkanClass", g_hInstance);
    g_hWnd = nullptr;
}

// --------------- Vulkan: utilities / selection ------------
static std::vector<const char*> GetRequiredExtensions()
{
    std::vector<const char*> exts;
    exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    exts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#ifdef _DEBUG
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    return exts;
}
#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    OutputDebugStringA(data->pMessage);
    OutputDebugStringA("\n");
    return VK_FALSE;
}
#endif

static void CreateInstance()
{
    auto exts = GetRequiredExtensions();
    VkApplicationInfo appInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "ImGui Win32 Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    VkInstanceCreateInfo ci { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
#ifdef _DEBUG
    ci.enabledLayerCount = 1;
    ci.ppEnabledLayerNames = layers;
    VkDebugUtilsMessengerCreateInfoEXT debugInfo {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
    };
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugInfo.pfnUserCallback = DebugCallback;
    ci.pNext = &debugInfo;
#endif
    CheckVk(vkCreateInstance(&ci, nullptr, &g_instance), "vkCreateInstance");
#ifdef _DEBUG
    auto pfnCreateDebug = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(g_instance, "vkCreateDebugUtilsMessengerEXT");
    if (pfnCreateDebug) {
        VkDebugUtilsMessengerCreateInfoEXT di {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
        };
        di.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        di.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        di.pfnUserCallback = DebugCallback;
        pfnCreateDebug(g_instance, &di, nullptr, &g_debugMessenger);
    }
#endif
}

static void CreateSurface()
{
    VkWin32SurfaceCreateInfoKHR ci { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    ci.hinstance = g_hInstance;
    ci.hwnd = g_hWnd;
    CheckVk(vkCreateWin32SurfaceKHR(g_instance, &ci, nullptr, &g_surface), "vkCreateWin32SurfaceKHR");
}

static bool IsQueueFamilySuitable(VkPhysicalDevice device, uint32_t family)
{
    VkBool32 present = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, family, g_surface, &present);

    VkQueueFamilyProperties props {};
    uint32_t count = 1;
    std::vector<VkQueueFamilyProperties> all;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    all.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, all.data());

    return (family < count) && (all[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present;
}

static void PickPhysicalDeviceAndQueue()
{
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(g_instance, &devCount, nullptr);
    if (devCount == 0)
        throw std::runtime_error("No Vulkan devices");
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(g_instance, &devCount, devs.data());
    for (auto d : devs) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qprops.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            if (IsQueueFamilySuitable(d, i)) {
                g_physicalDevice = d;
                g_queueFamily = i;
                return;
            }
        }
    }
    throw std::runtime_error("No suitable graphics+present queue family");
}

static void CreateDevice()
{
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = g_queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    const char* exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures features {};
    VkDeviceCreateInfo ci { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    ci.pQueueCreateInfos = &qci;
    ci.queueCreateInfoCount = 1;
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = exts;
    ci.pEnabledFeatures = &features;
#ifdef _DEBUG
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    ci.enabledLayerCount = 1;
    ci.ppEnabledLayerNames = layers;
#endif
    CheckVk(vkCreateDevice(g_physicalDevice, &ci, nullptr, &g_device), "vkCreateDevice");
    vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_queue);
}

static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avail)
{
    for (auto& f : avail) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return avail[0];
}

static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& avail)
{
    // FIFO is always available and avoids tearing. Use MAILBOX if present for low latency.
    for (auto m : avail)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static void CreateRenderPass()
{
    VkAttachmentDescription color {};
    color.format = g_swapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorRef { 0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub {};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    CheckVk(vkCreateRenderPass(g_device, &rpci, nullptr, &g_renderPass),
        "vkCreateRenderPass");
}

static void CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pci { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.queueFamilyIndex = g_queueFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CheckVk(vkCreateCommandPool(g_device, &pci, nullptr, &g_cmdPool),
        "vkCreateCommandPool");
    g_cmdBuffers.resize(g_swapchainImages.size());
    VkCommandBufferAllocateInfo ai {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    };
    ai.commandPool = g_cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)g_cmdBuffers.size();
    CheckVk(vkAllocateCommandBuffers(g_device, &ai, g_cmdBuffers.data()),
        "vkAllocateCommandBuffers");
}

static void CreateSyncObjects()
{
    VkSemaphoreCreateInfo sci { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // first frame shouldn't block
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        CheckVk(vkCreateSemaphore(g_device, &sci, nullptr,
                    &g_frames[i].imageAcquired),
            "vkCreateSemaphore");
        CheckVk(vkCreateSemaphore(g_device, &sci, nullptr,
                    &g_frames[i].renderComplete),
            "vkCreateSemaphore");
        CheckVk(vkCreateFence(g_device, &fci, nullptr, &g_frames[i].inFlight),
            "vkCreateFence");
    }
}

static void CreateFramebuffers()
{
    g_framebuffers.resize(g_swapchainImageViews.size());
    for (size_t i = 0; i < g_swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = { g_swapchainImageViews[i] };
        VkFramebufferCreateInfo fci {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO
        };
        fci.renderPass = g_renderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = attachments;
        fci.width = g_swapchainExtent.width;
        fci.height = g_swapchainExtent.height;
        fci.layers = 1;
        CheckVk(vkCreateFramebuffer(g_device, &fci, nullptr,
                    &g_framebuffers[i]),
            "vkCreateFramebuffer");
    }
}

static void CreateSwapchainAndImageViews()
{
    // Query caps
    VkSurfaceCapabilitiesKHR caps {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface,
        &caps);
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice,
        g_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &fmtCount,
        formats.data());
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface,
        &pmCount, nullptr);
    std::vector<VkPresentModeKHR> modes(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface,
        &pmCount, modes.data());
    VkSurfaceFormatKHR chosenFmt = ChooseSurfaceFormat(formats);
    g_swapchainFormat = chosenFmt.format;
    g_colorSpace = chosenFmt.colorSpace;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFF) {
        extent.width = g_width;
        extent.height = g_height;
    }
    g_swapchainExtent = extent;
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;
    if (imageCount < kMinImageCount)
        imageCount = kMinImageCount;
    VkSwapchainCreateInfoKHR ci { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = g_surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = g_swapchainFormat;
    ci.imageColorSpace = g_colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = ChoosePresentMode(modes);
    ci.clipped = VK_TRUE;
    CheckVk(vkCreateSwapchainKHR(g_device, &ci, nullptr, &g_swapchain),
        "vkCreateSwapchainKHR");
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &count,
        nullptr);
    g_swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &count,
        g_swapchainImages.data());
    g_swapchainImageViews.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo vi { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image = g_swapchainImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = g_swapchainFormat;
        vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY };
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount
            = 1;

        CheckVk(vkCreateImageView(g_device, &vi, nullptr,
                    &g_swapchainImageViews[i]),
            "vkCreateImageView");
    }
}

static void CreateVulkan()
{
    CreateInstance();
    CreateWin32Window(L"ImGui Win32 + Vulkan", g_width, g_height);
    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    CreateSurface();
    PickPhysicalDeviceAndQueue();
    CreateDevice();
    CreateSwapchainAndImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateCommandPoolAndBuffers();
    CreateSyncObjects();

    // Descriptor pool for ImGui (large enough for typical UI)
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo dpci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = 1000 * (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    dpci.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
    dpci.pPoolSizes = poolSizes;
    CheckVk(vkCreateDescriptorPool(g_device, &dpci, nullptr, &g_imguiDescPool), "vkCreateDescriptorPool");

    // Setup Dear ImGui context + backends
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hWnd);

    ImGui_ImplVulkan_InitInfo initInfo {};
    initInfo.Instance = g_instance;
    initInfo.PhysicalDevice = g_physicalDevice;
    initInfo.Device = g_device;
    initInfo.QueueFamily = g_queueFamily;
    initInfo.Queue = g_queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = g_imguiDescPool;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = kMinImageCount;
    initInfo.ImageCount = (uint32_t)g_swapchainImages.size();
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = [](VkResult err) { CheckVk(err, "ImGui_ImplVulkan"); };
    ImGui_ImplVulkan_Init(&initInfo, g_renderPass);

    // Upload Fonts
    {
        VkCommandBufferAllocateInfo ai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        ai.commandPool = g_cmdPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VkCommandBuffer cmd;
        CheckVk(vkAllocateCommandBuffers(g_device, &ai, &cmd), "vkAllocateCommandBuffers(font)");

        VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CheckVk(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer(font)");
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
        CheckVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer(font)");

        VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        CheckVk(vkQueueSubmit(g_queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit(font)");
        CheckVk(vkQueueWaitIdle(g_queue), "vkQueueWaitIdle(font)");
        ImGui_ImplVulkan_DestroyFontUploadObjects();
        vkFreeCommandBuffers(g_device, g_cmdPool, 1, &cmd);
    }
}

static void DestroySwapchainResources()
{
    vkDeviceWaitIdle(g_device);
    for (auto fb : g_framebuffers)
        if (fb)
            vkDestroyFramebuffer(g_device, fb, nullptr);
    g_framebuffers.clear();
    if (g_renderPass) {
        vkDestroyRenderPass(g_device, g_renderPass, nullptr);
        g_renderPass = VK_NULL_HANDLE;
    }
    for (auto v : g_swapchainImageViews)
        if (v)
            vkDestroyImageView(g_device, v, nullptr);
    g_swapchainImageViews.clear();
    if (g_swapchain) {
        vkDestroySwapchainKHR(g_device, g_swapchain, nullptr);
        g_swapchain = VK_NULL_HANDLE;
    }
}

static void CreateSwapchainResources()
{
    CreateSwapchainAndImageViews();
    CreateRenderPass();
    CreateFramebuffers();

    // Inform ImGui of the new swapchain image count
    ImGui_ImplVulkan_SetMinImageCount(kMinImageCount);
}

static void RecreateSwapchain()
{
    RECT rc {};
    GetClientRect(g_hWnd, &rc);
    g_width = rc.right - rc.left;
    g_height = rc.bottom - rc.top;
    if (g_width == 0 || g_height == 0)
        return; // minimized

    vkDeviceWaitIdle(g_device);
    DestroySwapchainResources();
    CreateSwapchainResources();

    // Re-allocate command buffers for new image count
    if (g_cmdPool)
        vkDestroyCommandPool(g_device, g_cmdPool, nullptr);
    CreateCommandPoolAndBuffers();

    g_swapchainRebuild = false;
}

static void DestroyVulkan()
{
    vkDeviceWaitIdle(g_device);

    // ImGui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_imguiDescPool) {
        vkDestroyDescriptorPool(g_device, g_imguiDescPool, nullptr);
        g_imguiDescPool = VK_NULL_HANDLE;
    }

    // Sync
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (g_frames[i].imageAcquired)
            vkDestroySemaphore(g_device, g_frames[i].imageAcquired, nullptr);
        if (g_frames[i].renderComplete)
            vkDestroySemaphore(g_device, g_frames[i].renderComplete, nullptr);
        if (g_frames[i].inFlight)
            vkDestroyFence(g_device, g_frames[i].inFlight, nullptr);
    }

    // Swapchain & render pass & views
    DestroySwapchainResources();

    // Command pool
    if (g_cmdPool) {
        vkDestroyCommandPool(g_device, g_cmdPool, nullptr);
        g_cmdPool = VK_NULL_HANDLE;
    }

    // Device/Surface/Instance
    if (g_device) {
        vkDestroyDevice(g_device, nullptr);
        g_device = VK_NULL_HANDLE;
    }
    if (g_surface) {
        vkDestroySurfaceKHR(g_instance, g_surface, nullptr);
        g_surface = VK_NULL_HANDLE;
    }
#ifdef _DEBUG
    if (g_debugMessenger) {
        auto pfnDestroyDebug = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(g_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (pfnDestroyDebug)
            pfnDestroyDebug(g_instance, g_debugMessenger, nullptr);
    }
#endif
    if (g_instance) {
        vkDestroyInstance(g_instance, nullptr);
        g_instance = VK_NULL_HANDLE;
    }

    DestroyWin32Window();
}

// -------------------------- Render ------------------------
static void RecordAndSubmit(uint32_t imageIndex)
{
    VkCommandBuffer cmd = g_cmdBuffers[imageIndex];
    CheckVk(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    CheckVk(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");

    VkClearValue clear {};
    clear.color = { { 0.10f, 0.10f, 0.12f, 1.0f } };

    VkRenderPassBeginInfo rpbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = g_renderPass;
    rpbi.framebuffer = g_framebuffers[imageIndex];
    rpbi.renderArea.offset = { 0, 0 };
    rpbi.renderArea.extent = g_swapchainExtent;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Render ImGui draw data
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);
    CheckVk(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &g_frames[g_frameIndex].imageAcquired;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &g_frames[g_frameIndex].renderComplete;

    CheckVk(vkResetFences(g_device, 1, &g_frames[g_frameIndex].inFlight), "vkResetFences");
    CheckVk(vkQueueSubmit(g_queue, 1, &si, g_frames[g_frameIndex].inFlight), "vkQueueSubmit");
}

static void Present(uint32_t imageIndex)
{
    VkPresentInfoKHR pi { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    VkSwapchainKHR swp = g_swapchain;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swp;
    pi.pImageIndices = &imageIndex;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &g_frames[g_frameIndex].renderComplete;
    VkResult res = vkQueuePresentKHR(g_queue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        g_swapchainRebuild = true;
        return;
    }
    CheckVk(res, "vkQueuePresentKHR");
}

// -------------------------- WinProc -----------------------
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_width = LOWORD(lParam);
            g_height = HIWORD(lParam);
            g_swapchainRebuild = true;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

int APIENTRY wmain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    try {
        CreateVulkan();

        bool showDemo = true;

        // Main loop
        MSG msg {};
        while (g_appRunning) {
            while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    g_appRunning = false;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (!g_appRunning)
                break;

            if (g_swapchainRebuild) {
                RecreateSwapchain();
            }

            // Start new ImGui frame
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            {
                static std::filesystem::path _ableton_path = "C:/ProgramData/Ableton/Live 9 Standard/Program/Ableton Live 9 Standard.exe";

                // Example UI (empty + optional demo)
                ImGui::Begin("dawxchange window");

                if (ImGui::Button("Import")) {
                }
                ImGui::SameLine();

                if (ImGui::Button("Export")) {
                }
                ImGui::SameLine();

                if (ImGui::Button("New")) {
                }
                ImGui::SameLine();
                
                static std::filesystem::path _container_path = "C:/Users/adri/Desktop/untitled.dxcc";
                if (ImGui::Button("Open")) {
                    _runtime.open_linked_session(_ableton_path, _container_path);
                }

                ImGui::End();
            }

            ImGui::Render();

            // Acquire next image
            CheckVk(vkWaitForFences(g_device, 1, &g_frames[g_frameIndex].inFlight, VK_TRUE, UINT64_MAX), "vkWaitForFences");

            uint32_t imageIndex = 0;
            VkResult acquireRes = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, g_frames[g_frameIndex].imageAcquired, VK_NULL_HANDLE, &imageIndex);
            if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
                g_swapchainRebuild = true;
                continue;
            }
            CheckVk(acquireRes, "vkAcquireNextImageKHR");

            RecordAndSubmit(imageIndex);
            Present(imageIndex);

            g_frameIndex = (g_frameIndex + 1) % kMaxFramesInFlight;
        }

        DestroyVulkan();
        return 0;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Fatal error", MB_ICONERROR | MB_OK);
        return -1;
    }
}