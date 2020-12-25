// Copyright (c) 2020, Aras Pranckevicius
// SPDX-License-Identifier: MIT

#ifndef SMOL_COMPUTE_INCLUDED
#define SMOL_COMPUTE_INCLUDED

// smol-compute: a small library to run compute shaders on several graphics APIs.
// https://github.com/aras-p/smol-compute
//
// This is a single-header library; for regular usage just include this header,
// but you also have to do a:
//   #define SMOL_COMPUTE_IMPLEMENTATION 1
//   #define SMOL_COMPUTE_<graphicsapi> 1
//   #include "smolcompute.h"
// in one of your compiled files. Current implementations are for D3D11 (SMOL_COMPUTE_D3D11)
// and Metal (SMOL_COMPUTE_METAL).


#include <stddef.h>


// Small utility to implement bitwise operators on strongly typed C++11 enums
#define SMOL_COMPUTE_ENUM_FLAGS(T) \
    inline T operator |(T a, T b) { return (T)((unsigned)a | (unsigned)b); } \
    inline T operator &(T a, T b) { return (T)((unsigned)a & (unsigned)b); } \
    inline T operator ^(T a, T b) { return (T)((unsigned)a ^ (unsigned)b); } \
    inline T operator ~(T a) { return (T)(~(unsigned)a); } \
    inline T& operator |=(T& a, T b) { return a = a | b; } \
    inline T& operator &=(T& a, T b) { return a = a & b; } \
    inline T& operator ^=(T& a, T b) { return a = a ^ b; } \
    inline bool HasFlag(T flags, T check) { return ((unsigned)flags & (unsigned)check) == (unsigned)check; } \
    inline bool HasAnyFlag(T flags, T check) { return ((unsigned)flags & (unsigned)check) != 0; }


struct SmolBuffer;
struct SmolKernel;

// Backend implementation type
enum class SmolBackend
{
    D3D11 = 0,
    Metal,
    Vulkan,
};

// Initialization flags (can be combined)
enum class SmolComputeCreateFlags
{
    None = 0,
    // Enable SmolCaptureStart/SmolCaptureFinish for capturing a computation into
    // a graphics debugger.
    // - D3D11: uses RenderDoc (assumes installed in default location),
    // - Metal: uses Xcode Metal frame capture.
    EnableCapture = 1 << 0,
    // Enable debug/validation layers when possible.
    EnableDebugLayers = 1 << 1,
    // Use software CPU device when possible.
    UseSoftwareRenderer = 1 << 2,
};
SMOL_COMPUTE_ENUM_FLAGS(SmolComputeCreateFlags);

// Data buffer type
enum class SmolBufferType
{
    Constant = 0,   // D3D11: constant buffer, Metal: does not care
    Structured,     // D3D11: structured buffer, Metal: does not care
};

// Binding "space" for buffer usage
enum class SmolBufferBinding
{
    Constant = 0,   // D3D11: constant buffer, Metal: does not care
    Input,          // D3D11: input (StructuredBuffer), Metal: does not care
    Output,         // D3D11: output (RWStructuredBuffer), Metal: does not care
};

// Kernel creation flags (can be combined)
enum class SmolKernelCreateFlags
{
	None = 0,
	DisableOptimizations = 1 << 0,  // D3D11: disable all optimizations. Metal: ignored.
    GenerateDebugInfo = 1 << 1,     // D3D11: generate debug symbols. Metal: ignored.
    EnableFastMath = 1 << 2,        // D3D11: do not pass IEEE strictness flag. Metal: sets fastMathEnabled flag.
};
SMOL_COMPUTE_ENUM_FLAGS(SmolKernelCreateFlags);


// Initialize the library. This has to be called before doing other work.
bool SmolComputeCreate(SmolComputeCreateFlags flags = SmolComputeCreateFlags::None);
// Shutdown the library.
void SmolComputeDelete();
// Get backend implementation type.
SmolBackend SmolComputeGetBackend();

// Data buffers: create, delete, set and get data.
// - All sizes are in bytes.
// - structElementSize is for structured buffers, some APIs need to know that.

SmolBuffer* SmolBufferCreate(size_t byteSize, SmolBufferType type, size_t structElementSize = 0);
void SmolBufferDelete(SmolBuffer* buffer);
void SmolBufferSetData(SmolBuffer* buffer, const void* src, size_t size, size_t dstOffset = 0);
void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset = 0);


// Computation kernels: create, delete, set them up (Set + SetBuffer), dispatch and wait
// for dispatches to complete.
// - Dispatch is number of "threads" launched, not number of "thread groups".

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint, SmolKernelCreateFlags flags = SmolKernelCreateFlags::None);
SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize);
void SmolKernelDelete(SmolKernel* kernel);
void SmolKernelSet(SmolKernel* kernel);
void SmolKernelSetBuffer(SmolBuffer* buffer, int index, SmolBufferBinding binding = SmolBufferBinding::Input);
void SmolKernelDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ);


// Starts and finishes capture into a graphics debugger.
// Initialization must be done with SmolComputeCreateFlags::EnableCapture flag.
void SmolCaptureStart();
void SmolCaptureFinish();

#endif // #ifndef SMOL_COMPUTE_INCLUDED



#if SMOL_COMPUTE_IMPLEMENTATION

#if !SMOL_COMPUTE_D3D11 && !SMOL_COMPUTE_METAL && !SMOL_COMPUTE_VULKAN
#error Define one of SMOL_COMPUTE_D3D11, SMOL_COMPUTE_METAL or SMOL_COMPUTE_VULKAN for a SMOL_COMPUTE_IMPLEMENTATION compile.
#endif

#ifndef SMOL_ASSERT
    #include <assert.h>
    #define SMOL_ASSERT(c) assert(c)
#endif


// ------------------------------------------------------------------------------------------------
//  Stripped down RenderDoc API header, from renderdoc_app.h
//  Original header is Copyright (c) 2019-2020 Baldur Karlsson, MIT license
//  Documentation for the API is available at https://renderdoc.org/docs/in_application_api.html

#define SMOL_COMPUTE_ENABLE_RENDERDOC (SMOL_COMPUTE_D3D11 || SMOL_COMPUTE_VULKAN)

#if SMOL_COMPUTE_ENABLE_RENDERDOC

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define SMOL_COMPUTE_RENDERDOC_CC __cdecl
#else
#define SMOL_COMPUTE_RENDERDOC_CC
#endif

extern "C" {
    typedef void(SMOL_COMPUTE_RENDERDOC_CC* SmolImpl_pRENDERDOC_StartFrameCapture)(void* device, void* wndHandle);
    typedef unsigned int(SMOL_COMPUTE_RENDERDOC_CC* SmolImpl_pRENDERDOC_EndFrameCapture)(void* device, void* wndHandle);
    enum SmolImpl_RENDERDOC_Version { SmolImpl_eRENDERDOC_API_Version_1_4_1 = 10401 };
    struct SmolImpl_RENDERDOC_API_1_4_1
    {
        void* GetAPIVersion;
        void* SetCaptureOptionU32;
        void* SetCaptureOptionF32;
        void* GetCaptureOptionU32;
        void* GetCaptureOptionF32;
        void* SetFocusToggleKeys;
        void* SetCaptureKeys;
        void* GetOverlayBits;
        void* MaskOverlayBits;
        void* RemoveHooks;
        void* UnloadCrashHandler;
		void* SetCaptureFilePathTemplate;
		void* GetCaptureFilePathTemplate;
        void* GetNumCaptures;
        void* GetCapture;
        void* TriggerCapture;
        void* IsTargetControlConnected;
        void* LaunchReplayUI;
        void* SetActiveWindow;
        SmolImpl_pRENDERDOC_StartFrameCapture StartFrameCapture;
        void* IsFrameCapturing;
        SmolImpl_pRENDERDOC_EndFrameCapture EndFrameCapture;
        void* TriggerMultiFrameCapture;
        void* SetCaptureFileComments;
        void* DiscardFrameCapture;
    };
    typedef int(SMOL_COMPUTE_RENDERDOC_CC* SmolImpl_pRENDERDOC_GetAPI)(SmolImpl_RENDERDOC_Version version, void** outAPIPointers);
} // extern "C"

static SmolImpl_RENDERDOC_API_1_4_1* s_RenderDocApi;

#include <wtypes.h>

static void SmolImpl_LoadRenderDoc()
{
    HMODULE dll = LoadLibraryA("C:\\Program Files\\RenderDoc\\renderdoc.dll");
    if (dll)
    {
        SmolImpl_pRENDERDOC_GetAPI procGetApi = (SmolImpl_pRENDERDOC_GetAPI)GetProcAddress(dll, "RENDERDOC_GetAPI");
        if (procGetApi)
            procGetApi(SmolImpl_eRENDERDOC_API_Version_1_4_1, (void**)&s_RenderDocApi);
    }
}

#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC


// ------------------------------------------------------------------------------------------------
//  D3D11


#if SMOL_COMPUTE_D3D11
#include <d3d11.h>
#include <d3dcompiler.h>

static ID3D11Device* s_D3D11Device;
static ID3D11DeviceContext* s_D3D11Context;

#define SMOL_RELEASE(o) { if (o) (o)->Release(); (o) = nullptr; }

bool SmolComputeCreate(SmolComputeCreateFlags flags)
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
    if (HasFlag(flags, SmolComputeCreateFlags::EnableCapture))
        SmolImpl_LoadRenderDoc();
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC

    HRESULT hr;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_DRIVER_TYPE driverType = HasFlag(flags, SmolComputeCreateFlags::UseSoftwareRenderer) ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    if (HasFlag(flags, SmolComputeCreateFlags::EnableDebugLayers))
        hr = D3D11CreateDevice(NULL, driverType, NULL, D3D11_CREATE_DEVICE_DEBUG, levels, 1, D3D11_SDK_VERSION, &s_D3D11Device, NULL, &s_D3D11Context);
    if (s_D3D11Device == nullptr)
        hr = D3D11CreateDevice(NULL, driverType, NULL, 0, levels, 1, D3D11_SDK_VERSION, &s_D3D11Device, NULL, &s_D3D11Context);
    if (FAILED(hr))
        return false;
    return true;
}

void SmolComputeDelete()
{
    SMOL_RELEASE(s_D3D11Context);
    SMOL_RELEASE(s_D3D11Device);
}

SmolBackend SmolComputeGetBackend()
{
    return SmolBackend::D3D11;
}


struct SmolBuffer
{
    ID3D11Buffer* buffer = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;
    size_t size = 0;
    SmolBufferType type = SmolBufferType::Structured;
    size_t structElementSize = 0;
};

SmolBuffer* SmolBufferCreate(size_t byteSize, SmolBufferType type, size_t structElementSize)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = (UINT)byteSize;
    if (type == SmolBufferType::Constant)
    {
        SMOL_ASSERT(structElementSize == 0);
        desc.ByteWidth = (desc.ByteWidth + 15) / 16 * 16;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    }
    else if (type == SmolBufferType::Structured)
    {
        SMOL_ASSERT(structElementSize != 0 && structElementSize % 4 == 0);
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.StructureByteStride = (UINT)structElementSize;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = s_D3D11Device->CreateBuffer(&desc, NULL, &buffer);
    if (FAILED(hr))
        return nullptr;

    SmolBuffer* buf = new SmolBuffer();
    buf->buffer = buffer;
    buf->size = byteSize;
    buf->type = type;
    buf->structElementSize = structElementSize;
    return buf;
}

void SmolBufferSetData(SmolBuffer* buffer, const void* src, size_t size, size_t dstOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(dstOffset + size <= buffer->size);

    const bool fullBufferUpdate = (dstOffset == 0) && (size == buffer->size);
    if (buffer->type == SmolBufferType::Constant)
    {
        SMOL_ASSERT(fullBufferUpdate);
    }
    D3D11_BOX box = {};
    box.left = (UINT)dstOffset;
    box.right = (UINT)(dstOffset + size);
    box.bottom = box.back = 1;
    s_D3D11Context->UpdateSubresource(buffer->buffer, 0, fullBufferUpdate ? NULL : &box, src, 0, 0);
}

void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(srcOffset + size <= buffer->size);

    ID3D11Buffer* staging = nullptr;
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = (UINT)size;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = (UINT)buffer->structElementSize;
    HRESULT hr = s_D3D11Device->CreateBuffer(&desc, NULL, &staging);
    if (FAILED(hr))
        return;

    D3D11_BOX box = {};
    box.left = (UINT)srcOffset;
    box.right = (UINT)(srcOffset + size);
    box.bottom = box.back = 1;
    s_D3D11Context->CopySubresourceRegion(staging, 0, 0, 0, 0, buffer->buffer, 0, &box);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = s_D3D11Context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(dst, mapped.pData, size);
        s_D3D11Context->Unmap(staging, 0);
    }
    SMOL_RELEASE(staging);
}

void SmolBufferDelete(SmolBuffer* buffer)
{
    if (buffer == nullptr)
        return;
    SMOL_RELEASE(buffer->uav);
    SMOL_RELEASE(buffer->srv);
    SMOL_RELEASE(buffer->buffer);
    delete buffer;
}

struct SmolKernel
{
    ID3D11ComputeShader* kernel;
};

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint, SmolKernelCreateFlags flags)
{
    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    UINT d3dflags = 0;
    if (!HasFlag(flags, SmolKernelCreateFlags::EnableFastMath))
        d3dflags |= D3DCOMPILE_IEEE_STRICTNESS;
    if (HasFlag(flags, SmolKernelCreateFlags::DisableOptimizations))
        d3dflags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    if (HasFlag(flags, SmolKernelCreateFlags::GenerateDebugInfo))
        d3dflags |= D3DCOMPILE_DEBUG;
    HRESULT hr = D3DCompile(shaderCode, shaderCodeSize, "", NULL, NULL, entryPoint, "cs_5_0", d3dflags, 0, &bytecode, &errors);
    if (FAILED(hr))
    {
        const char* errMsg = (const char*)errors->GetBufferPointer();
        OutputDebugStringA(errMsg);
        SMOL_RELEASE(bytecode);
        SMOL_RELEASE(errors);
        return nullptr;
    }

    ID3D11ComputeShader* cs = nullptr;
    hr = s_D3D11Device->CreateComputeShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), NULL, &cs);
    if (FAILED(hr))
    {
        SMOL_RELEASE(bytecode);
        SMOL_RELEASE(errors);
        return nullptr;
    }

    SmolKernel* kernel = new SmolKernel();
    kernel->kernel = cs;
    return kernel;
}

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize)
{
	ID3D11ComputeShader* cs = nullptr;
	HRESULT hr = s_D3D11Device->CreateComputeShader(shaderCode, shaderCodeSize, NULL, &cs);
	if (FAILED(hr))
		return nullptr;
	SmolKernel* kernel = new SmolKernel();
	kernel->kernel = cs;
	return kernel;
}

void SmolKernelDelete(SmolKernel* kernel)
{
    if (kernel == nullptr)
        return;
    SMOL_RELEASE(kernel->kernel);
    delete kernel;
}

void SmolKernelSet(SmolKernel* kernel)
{
    s_D3D11Context->CSSetShader(kernel->kernel, NULL, 0);
}

void SmolKernelSetBuffer(SmolBuffer* buffer, int index, SmolBufferBinding binding)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(buffer->buffer);
    switch (binding)
    {
    case SmolBufferBinding::Constant:
        SMOL_ASSERT(buffer->type == SmolBufferType::Constant);
        s_D3D11Context->CSSetConstantBuffers(index, 1, &buffer->buffer);
        break;
    case SmolBufferBinding::Input:
        SMOL_ASSERT(buffer->type == SmolBufferType::Structured);
        if (buffer->srv == nullptr)
        {
            SMOL_ASSERT(buffer->structElementSize != 0);
            D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = (UINT)(buffer->size / buffer->structElementSize);
            HRESULT hr = s_D3D11Device->CreateShaderResourceView(buffer->buffer, &desc, &buffer->srv);
            SMOL_ASSERT(SUCCEEDED(hr));
        }
        s_D3D11Context->CSSetShaderResources(index, 1, &buffer->srv);
        break;
    case SmolBufferBinding::Output:
        SMOL_ASSERT(buffer->type == SmolBufferType::Structured);
        if (buffer->uav == nullptr)
        {
            SMOL_ASSERT(buffer->structElementSize != 0);
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = (UINT)(buffer->size / buffer->structElementSize);
            HRESULT hr = s_D3D11Device->CreateUnorderedAccessView(buffer->buffer, &desc, &buffer->uav);
            SMOL_ASSERT(SUCCEEDED(hr));
        }
        s_D3D11Context->CSSetUnorderedAccessViews(index, 1, &buffer->uav, NULL);
        break;
    default:
        SMOL_ASSERT(false);
    }
}

void SmolKernelDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ)
{
    int groupsX = (threadsX + groupSizeX - 1) / groupSizeX;
    int groupsY = (threadsY + groupSizeY - 1) / groupSizeY;
    int groupsZ = (threadsZ + groupSizeZ - 1) / groupSizeZ;
    s_D3D11Context->Dispatch(groupsX, groupsY, groupsZ);
}

void SmolCaptureStart()
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
    if (!s_RenderDocApi)
        return;
    s_RenderDocApi->StartFrameCapture(NULL, NULL);
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC
}

void SmolCaptureFinish()
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
	if (!s_RenderDocApi)
		return;
	s_RenderDocApi->EndFrameCapture(NULL, NULL);
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC
}


#endif // #if SMOL_COMPUTE_D3D11


// ------------------------------------------------------------------------------------------------
//  Vulkan

#if SMOL_COMPUTE_VULKAN

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
typedef unsigned long DWORD;
typedef const wchar_t* LPCWSTR;
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HWND__* HWND;
typedef struct HMONITOR__* HMONITOR;
typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;
#include "vulkan/vulkan_win32.h"

// Manually loaded minimal set of Vulkan function pointers that we need, so that we don't
// have to link with the loader.
extern "C" {
    // Vulkan 1.0
    static PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    static PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    static PFN_vkAllocateMemory vkAllocateMemory;
    static PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    static PFN_vkBindBufferMemory vkBindBufferMemory;
    static PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    static PFN_vkCmdBindPipeline vkCmdBindPipeline;
    static PFN_vkCmdDispatch vkCmdDispatch;
    static PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
    static PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
    static PFN_vkCreateBuffer vkCreateBuffer;
    static PFN_vkCreateCommandPool vkCreateCommandPool;
    static PFN_vkCreateComputePipelines vkCreateComputePipelines;
    static PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    static PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    static PFN_vkCreateDevice vkCreateDevice;
    static PFN_vkCreateInstance vkCreateInstance;
    static PFN_vkCreatePipelineCache vkCreatePipelineCache;
    static PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    static PFN_vkCreateShaderModule vkCreateShaderModule;
    static PFN_vkDestroyBuffer vkDestroyBuffer;
    static PFN_vkDestroyCommandPool vkDestroyCommandPool;
    static PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    static PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    static PFN_vkDestroyDevice vkDestroyDevice;
    static PFN_vkDestroyInstance vkDestroyInstance;
    static PFN_vkDestroyPipeline vkDestroyPipeline;
    static PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
    static PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    static PFN_vkDestroyShaderModule vkDestroyShaderModule;
    static PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    static PFN_vkEndCommandBuffer vkEndCommandBuffer;
    static PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    static PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    static PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    static PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    static PFN_vkFreeMemory vkFreeMemory;
    static PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    static PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    static PFN_vkGetDeviceQueue vkGetDeviceQueue;
    static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    static PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
    static PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    static PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    static PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    static PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
    static PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
    static PFN_vkMapMemory vkMapMemory;
    static PFN_vkQueueSubmit vkQueueSubmit;
    static PFN_vkQueueWaitIdle vkQueueWaitIdle;
    static PFN_vkResetCommandBuffer vkResetCommandBuffer;
    static PFN_vkResetCommandPool vkResetCommandPool;
    static PFN_vkResetDescriptorPool vkResetDescriptorPool;
    static PFN_vkUnmapMemory vkUnmapMemory;
    static PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    // VK_EXT_debug_report
    static PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    static PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
} // extern "C"

static VkResult SmolImpl_VkInitialize()
{
    HMODULE dll = LoadLibraryA("vulkan-1.dll");
    if (!dll)
        return VK_ERROR_INITIALIZATION_FAILED;
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(dll, "vkGetInstanceProcAddr");
    vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(0, "vkCreateInstance");
    return VK_SUCCESS;
}

static void SmolImpl_VkLoadInstanceFunctions(VkInstance instance)
{
    vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)vkGetInstanceProcAddr(instance, "vkAllocateCommandBuffers");
    vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)vkGetInstanceProcAddr(instance, "vkAllocateDescriptorSets");
    vkAllocateMemory = (PFN_vkAllocateMemory)vkGetInstanceProcAddr(instance, "vkAllocateMemory");
    vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)vkGetInstanceProcAddr(instance, "vkBeginCommandBuffer");
    vkBindBufferMemory = (PFN_vkBindBufferMemory)vkGetInstanceProcAddr(instance, "vkBindBufferMemory");
    vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)vkGetInstanceProcAddr(instance, "vkCmdBindDescriptorSets");
    vkCmdBindPipeline = (PFN_vkCmdBindPipeline)vkGetInstanceProcAddr(instance, "vkCmdBindPipeline");
    vkCmdDispatch = (PFN_vkCmdDispatch)vkGetInstanceProcAddr(instance, "vkCmdDispatch");
    vkCmdExecuteCommands = (PFN_vkCmdExecuteCommands)vkGetInstanceProcAddr(instance, "vkCmdExecuteCommands");
    vkCmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)vkGetInstanceProcAddr(instance, "vkCmdUpdateBuffer");
    vkCreateBuffer = (PFN_vkCreateBuffer)vkGetInstanceProcAddr(instance, "vkCreateBuffer");
    vkCreateCommandPool = (PFN_vkCreateCommandPool)vkGetInstanceProcAddr(instance, "vkCreateCommandPool");
    vkCreateComputePipelines = (PFN_vkCreateComputePipelines)vkGetInstanceProcAddr(instance, "vkCreateComputePipelines");
    vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)vkGetInstanceProcAddr(instance, "vkCreateDescriptorPool");
    vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)vkGetInstanceProcAddr(instance, "vkCreateDescriptorSetLayout");
    vkCreateDevice = (PFN_vkCreateDevice)vkGetInstanceProcAddr(instance, "vkCreateDevice");
    vkCreatePipelineCache = (PFN_vkCreatePipelineCache)vkGetInstanceProcAddr(instance, "vkCreatePipelineCache");
    vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)vkGetInstanceProcAddr(instance, "vkCreatePipelineLayout");
    vkCreateShaderModule = (PFN_vkCreateShaderModule)vkGetInstanceProcAddr(instance, "vkCreateShaderModule");
    vkDestroyBuffer = (PFN_vkDestroyBuffer)vkGetInstanceProcAddr(instance, "vkDestroyBuffer");
    vkDestroyCommandPool = (PFN_vkDestroyCommandPool)vkGetInstanceProcAddr(instance, "vkDestroyCommandPool");
    vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)vkGetInstanceProcAddr(instance, "vkDestroyDescriptorPool");
    vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)vkGetInstanceProcAddr(instance, "vkDestroyDescriptorSetLayout");
    vkDestroyDevice = (PFN_vkDestroyDevice)vkGetInstanceProcAddr(instance, "vkDestroyDevice");
    vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(instance, "vkDestroyInstance");
    vkDestroyPipeline = (PFN_vkDestroyPipeline)vkGetInstanceProcAddr(instance, "vkDestroyPipeline");
    vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache)vkGetInstanceProcAddr(instance, "vkDestroyPipelineCache");
    vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)vkGetInstanceProcAddr(instance, "vkDestroyPipelineLayout");
    vkDestroyShaderModule = (PFN_vkDestroyShaderModule)vkGetInstanceProcAddr(instance, "vkDestroyShaderModule");
    vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetInstanceProcAddr(instance, "vkDeviceWaitIdle");
    vkEndCommandBuffer = (PFN_vkEndCommandBuffer)vkGetInstanceProcAddr(instance, "vkEndCommandBuffer");
    vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkGetInstanceProcAddr(instance, "vkFlushMappedMemoryRanges");
    vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers)vkGetInstanceProcAddr(instance, "vkFreeCommandBuffers");
    vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)vkGetInstanceProcAddr(instance, "vkFreeDescriptorSets");
    vkFreeMemory = (PFN_vkFreeMemory)vkGetInstanceProcAddr(instance, "vkFreeMemory");
    vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetInstanceProcAddr(instance, "vkGetBufferMemoryRequirements");
    vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    vkGetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetInstanceProcAddr(instance, "vkGetDeviceQueue");
    vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures");
    vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
    vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPipelineCacheData = (PFN_vkGetPipelineCacheData)vkGetInstanceProcAddr(instance, "vkGetPipelineCacheData");
    vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkGetInstanceProcAddr(instance, "vkInvalidateMappedMemoryRanges");
    vkMapMemory = (PFN_vkMapMemory)vkGetInstanceProcAddr(instance, "vkMapMemory");
    vkQueueSubmit = (PFN_vkQueueSubmit)vkGetInstanceProcAddr(instance, "vkQueueSubmit");
    vkQueueWaitIdle = (PFN_vkQueueWaitIdle)vkGetInstanceProcAddr(instance, "vkQueueWaitIdle");
    vkResetCommandBuffer = (PFN_vkResetCommandBuffer)vkGetInstanceProcAddr(instance, "vkResetCommandBuffer");
    vkResetCommandPool = (PFN_vkResetCommandPool)vkGetInstanceProcAddr(instance, "vkResetCommandPool");
    vkResetDescriptorPool = (PFN_vkResetDescriptorPool)vkGetInstanceProcAddr(instance, "vkResetDescriptorPool");
    vkUnmapMemory = (PFN_vkUnmapMemory)vkGetInstanceProcAddr(instance, "vkUnmapMemory");
    vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)vkGetInstanceProcAddr(instance, "vkUpdateDescriptorSets");

    vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
}

#include <malloc.h>
#include <memory>

static VkInstance s_VkInstance;
static VkDevice s_VkDevice;
static uint32_t s_VkComputeQueueIndex;
static VkQueue s_VkComputeQueue;
static uint32_t s_VkMemoryTypeHostVisibleNonCoherent;
static uint32_t s_VkMemoryTypeHostVisibleCoherent;
static uint32_t s_VkMemoryTypeDeviceLocal;
static VkDescriptorPool s_VkDescriptorPool;
static VkCommandPool s_VkCommandPool;
static VkCommandBuffer s_VkCommandBuffer;
static VkDebugReportCallbackEXT s_VkDebugReportCallback;

static VkResult SmolImpl_GetBestComputeQueue(VkPhysicalDevice device, uint32_t* outQueueFamilyIndex)
{
    uint32_t propsCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &propsCount, 0);
    VkQueueFamilyProperties* props = (VkQueueFamilyProperties*)_alloca(sizeof(VkQueueFamilyProperties) * propsCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &propsCount, props);

    // try to find a queue that only has the compute bit
    for (uint32_t i = 0; i < propsCount; ++i)
    {
        auto flags = props[i].queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT))
        {
            *outQueueFamilyIndex = i;
            return VK_SUCCESS;
        }
    }
    // otherwise get any suitable queue
    for (uint32_t i = 0; i < propsCount; ++i)
    {
        auto flags = props[i].queueFlags;
        if (flags & VK_QUEUE_COMPUTE_BIT)
        {
            *outQueueFamilyIndex = i;
            return VK_SUCCESS;
        }
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

static VkBool32 VKAPI_CALL SmolImpl_VkDebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    // Silence performance warnings
    if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        return VK_FALSE;

    const char* type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR" : (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "WARNING" : "INFO";
    char message[4096];
    snprintf(message, sizeof(message), "Vulkan %s: %s\n", type, pMessage);
    printf("%s", message);
#ifdef _WIN32
    OutputDebugStringA(message);
#endif
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        assert(!"Vulkan validation error encountered!");

    return VK_FALSE;
}


bool SmolComputeCreate(SmolComputeCreateFlags flags)
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
    if (HasFlag(flags, SmolComputeCreateFlags::EnableCapture))
        SmolImpl_LoadRenderDoc();
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC

    if (SmolImpl_VkInitialize() != VK_SUCCESS)
        return false;

    // instance
    const VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "smol_compute", 0, "smol_compute", 0, VK_MAKE_VERSION(1, 1, 0) };
    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    const char* debugLayers[] = { "VK_LAYER_KHRONOS_validation" };
    const char* debugExtensions[] = { "VK_EXT_debug_report" };
    bool useDebugLayer = HasFlag(flags, SmolComputeCreateFlags::EnableDebugLayers);
    if (useDebugLayer)
    {
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = debugLayers;
        instanceCreateInfo.enabledExtensionCount = 1;
        instanceCreateInfo.ppEnabledExtensionNames = debugExtensions;
    }
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    VkResult res;
    res = vkCreateInstance(&instanceCreateInfo, 0, &s_VkInstance);
    if (res == VK_ERROR_LAYER_NOT_PRESENT || res == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        // no debug layer support; run without
        useDebugLayer = false;
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.ppEnabledLayerNames = 0;
        instanceCreateInfo.enabledExtensionCount = 0;
        instanceCreateInfo.ppEnabledExtensionNames = 0;
        res = vkCreateInstance(&instanceCreateInfo, 0, &s_VkInstance);
    }
    if (res != VK_SUCCESS)
        return false;
    SmolImpl_VkLoadInstanceFunctions(s_VkInstance);

    if (useDebugLayer)
    {
        VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
        createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
        createInfo.pfnCallback = SmolImpl_VkDebugReportCallback;
        res = vkCreateDebugReportCallbackEXT(s_VkInstance, &createInfo, 0, &s_VkDebugReportCallback);
    }

    // physical devices
    uint32_t physicalDeviceCount = 0;
    res = vkEnumeratePhysicalDevices(s_VkInstance, &physicalDeviceCount, 0);
    if (res != VK_SUCCESS || physicalDeviceCount == 0)
        return false;
    VkPhysicalDevice* physicalDevices = (VkPhysicalDevice*)_alloca(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    res = vkEnumeratePhysicalDevices(s_VkInstance, &physicalDeviceCount, physicalDevices);
    if (res != VK_SUCCESS)
        return false;
    uint32_t pdi = 0;
    for (; pdi < physicalDeviceCount; ++pdi)
    {
        res = SmolImpl_GetBestComputeQueue(physicalDevices[pdi], &s_VkComputeQueueIndex);
        if (res == VK_SUCCESS)
            break;
    }
    if (pdi == physicalDeviceCount)
        return false; // no devices with compute queue found

    // device
    const float queuePrioritory = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, 0, 0, s_VkComputeQueueIndex, 1, &queuePrioritory};
    const VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, 0, 0, 1, &deviceQueueCreateInfo, 0, 0, 0, 0, 0 };
    res = vkCreateDevice(physicalDevices[pdi], &deviceCreateInfo, 0, &s_VkDevice);
    if (res != VK_SUCCESS)
        return false;
    vkGetDeviceQueue(s_VkDevice, s_VkComputeQueueIndex, 0, &s_VkComputeQueue);

    // memory properties
    VkPhysicalDeviceMemoryProperties properties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevices[pdi], &properties);
    s_VkMemoryTypeHostVisibleNonCoherent = VK_MAX_MEMORY_TYPES;
    s_VkMemoryTypeHostVisibleCoherent = VK_MAX_MEMORY_TYPES;
    s_VkMemoryTypeDeviceLocal = VK_MAX_MEMORY_TYPES;
    for (uint32_t mt = 0; mt < properties.memoryTypeCount; ++mt)
    {
        const auto& mem = properties.memoryTypes[mt];
        if ((s_VkMemoryTypeHostVisibleNonCoherent == VK_MAX_MEMORY_TYPES) && (mem.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && !(mem.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            s_VkMemoryTypeHostVisibleNonCoherent = mt;
        if ((s_VkMemoryTypeHostVisibleCoherent == VK_MAX_MEMORY_TYPES) && (mem.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (mem.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            s_VkMemoryTypeHostVisibleCoherent = mt;
        if ((s_VkMemoryTypeDeviceLocal == VK_MAX_MEMORY_TYPES) && (mem.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            s_VkMemoryTypeDeviceLocal = mt;
    }

    // descriptor pool
    uint32_t kPoolDescriptorCount = 1024;
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kPoolDescriptorCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kPoolDescriptorCount },
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolCreateInfo.maxSets = kPoolDescriptorCount;
    poolCreateInfo.poolSizeCount = sizeof(poolSizes)/sizeof(poolSizes[0]);
    poolCreateInfo.pPoolSizes = poolSizes;
    res = vkCreateDescriptorPool(s_VkDevice, &poolCreateInfo, 0, &s_VkDescriptorPool);
    if (res != VK_SUCCESS)
        return false;

    // command pool
    VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    commandPoolCreateInfo.queueFamilyIndex = s_VkComputeQueueIndex;
    res = vkCreateCommandPool(s_VkDevice, &commandPoolCreateInfo, 0, &s_VkCommandPool);
    if (res != VK_SUCCESS)
        return false;

    return true;
}

static void SmolImpl_VkFinishWork()
{
    if (!s_VkCommandBuffer)
        return;
    VkResult res = vkEndCommandBuffer(s_VkCommandBuffer);
    SMOL_ASSERT(res == VK_SUCCESS);

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &s_VkCommandBuffer;
    res = vkQueueSubmit(s_VkComputeQueue, 1, &submitInfo, 0);
    SMOL_ASSERT(res == VK_SUCCESS);
    vkQueueWaitIdle(s_VkComputeQueue);

    vkFreeCommandBuffers(s_VkDevice, s_VkCommandPool, 1, &s_VkCommandBuffer);
    s_VkCommandBuffer = 0;

    vkResetDescriptorPool(s_VkDevice, s_VkDescriptorPool, 0);
}

void SmolComputeDelete()
{
    SmolImpl_VkFinishWork();
    if (s_VkCommandPool) vkDestroyCommandPool(s_VkDevice, s_VkCommandPool, 0); s_VkCommandPool = 0;
    if (s_VkDescriptorPool) vkDestroyDescriptorPool(s_VkDevice, s_VkDescriptorPool, 0); s_VkDescriptorPool = 0;
    if (s_VkDevice) vkDestroyDevice(s_VkDevice, 0); s_VkDevice = 0;
    if (s_VkDebugReportCallback) vkDestroyDebugReportCallbackEXT(s_VkInstance, s_VkDebugReportCallback, 0); s_VkDebugReportCallback = 0;
    if (s_VkInstance) vkDestroyInstance(s_VkInstance, 0); s_VkInstance = 0;
}

SmolBackend SmolComputeGetBackend()
{
    return SmolBackend::Vulkan;
}

struct SmolBuffer
{
    VkBuffer buffer = nullptr;
    VkDeviceMemory memory = nullptr;
    size_t size = 0;
    SmolBufferType type = SmolBufferType::Structured;
    size_t structElementSize = 0;
    bool writtenByGpuSinceLastRead = false;
};

SmolBuffer* SmolBufferCreate(size_t byteSize, SmolBufferType type, size_t structElementSize)
{
    VkBufferUsageFlags usage = type == SmolBufferType::Constant ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, 0, 0, byteSize, usage, VK_SHARING_MODE_EXCLUSIVE, 1, &s_VkComputeQueueIndex };
    VkBuffer buffer = 0;
    VkResult res = vkCreateBuffer(s_VkDevice, &bufferCreateInfo, 0, &buffer);
    if (res != VK_SUCCESS)
        return nullptr;
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(s_VkDevice, buffer, &requirements);

    uint32_t memType = s_VkMemoryTypeHostVisibleNonCoherent != VK_MAX_MEMORY_TYPES ? s_VkMemoryTypeHostVisibleNonCoherent : s_VkMemoryTypeHostVisibleCoherent;
    const VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, 0, requirements.size, memType };

    VkDeviceMemory memory = 0;
    res = vkAllocateMemory(s_VkDevice, &memoryAllocateInfo, 0, &memory);
    if (res != VK_SUCCESS)
    {
        vkDestroyBuffer(s_VkDevice, buffer, 0);
        return nullptr;
    }
    res = vkBindBufferMemory(s_VkDevice, buffer, memory, 0);
    if (res != VK_SUCCESS)
    {
        vkFreeMemory(s_VkDevice, memory, 0);
        vkDestroyBuffer(s_VkDevice, buffer, 0);
        return nullptr;
    }

    SmolBuffer* buf = new SmolBuffer();
    buf->buffer = buffer;
    buf->memory = memory;
    buf->size = byteSize;
    buf->type = type;
    buf->structElementSize = structElementSize;
    return buf;
}

void SmolBufferSetData(SmolBuffer* buffer, const void* src, size_t size, size_t dstOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(dstOffset + size <= buffer->size);

    void* dst = 0;
    VkResult res = vkMapMemory(s_VkDevice, buffer->memory, dstOffset, size, 0, &dst);
    if (res != VK_SUCCESS)
    {
        SMOL_ASSERT(!"failed to map Vulkan buffer memory for writing");
        return;
    }
    memcpy(dst, src, size);
    vkUnmapMemory(s_VkDevice, buffer->memory);
}

void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(srcOffset + size <= buffer->size);

    if (buffer->writtenByGpuSinceLastRead)
    {
        SmolImpl_VkFinishWork();
        buffer->writtenByGpuSinceLastRead = false;
    }

    void* src = 0;
    VkResult res = vkMapMemory(s_VkDevice, buffer->memory, srcOffset, size, 0, &src);
    if (res != VK_SUCCESS)
    {
        SMOL_ASSERT(!"failed to map Vulkan buffer memory for reading");
        return;
    }
    memcpy(dst, src, size);
    vkUnmapMemory(s_VkDevice, buffer->memory);
}

void SmolBufferDelete(SmolBuffer* buffer)
{
    if (buffer == nullptr)
        return;
    if (buffer->buffer != 0)
        vkDestroyBuffer(s_VkDevice, buffer->buffer, 0);
    if (buffer->memory != 0)
        vkFreeMemory(s_VkDevice, buffer->memory, 0);
    delete buffer;
}

static const int SmolImpl_VkMaxResources = 32;

struct SmolKernel
{
    VkShaderModule kernel = nullptr;
    VkDescriptorSetLayout dsLayout = nullptr;
    VkPipelineLayout pipeLayout = nullptr;
    VkPipeline pipeline = nullptr;
    int localSize[3] = { 0, 0, 0 };
    VkDescriptorType resourceTypes[SmolImpl_VkMaxResources] = {};
    uint32_t resourceMask = 0;
    uint32_t resourceCount = 0;
};

static const uint32_t SmolImpl_SpvMagicNumber = 0x07230203;
static const uint32_t SmolImpl_SpvExecutionModelGLCompute = 5;
static const uint32_t SmolImpl_SpvExecutionModeLocalSize = 17;
static const uint32_t SmolImpl_SpvDecorationBlock = 2;
static const uint32_t SmolImpl_SpvDecorationBufferBlock = 3;
static const uint32_t SmolImpl_SpvDecorationBinding = 33;
static const uint32_t SmolImpl_SpvDecorationDescriptorSet = 34;
static const uint32_t SmolImpl_SpvStorageClassUniformConstant = 0;
static const uint32_t SmolImpl_SpvStorageClassUniform = 2;
static const uint32_t SmolImpl_SpvStorageClassStorageBuffer = 12;

enum SmolImpl_SpvOp
{
    kSmolImpl_SpvOpEntryPoint = 15,
    kSmolImpl_SpvOpExecutionMode = 16,
    kSmolImpl_SpvOpTypeImage = 25,
    kSmolImpl_SpvOpTypeSampler = 26,
    kSmolImpl_SpvOpTypeSampledImage = 27,
    kSmolImpl_SpvOpTypeStruct = 30,
    kSmolImpl_SpvOpTypePointer = 32,
    kSmolImpl_SpvOpVariable = 59,
    kSmolImpl_SpvOpDecorate = 71,
};

static bool SmolImpl_VkParseShaderResources(const uint32_t* code, uint32_t codeSizeInWords, SmolKernel& kernel)
{
    if (codeSizeInWords < 5) // SPIR-V header is 5 words
        return false;
    if (code[0] != SmolImpl_SpvMagicNumber)
        return false;

    // Parse code and figure out information about Ids
    struct Id
    {
        uint32_t op = 0;
        uint32_t typeId = 0;
        uint32_t storageClass = 0;
        uint32_t binding = 0;
        uint32_t set = 0;
        bool bufferBlock = false;
    };
    const uint32_t boundIdCount = code[3];
    const auto ids = std::unique_ptr<Id[]>(new Id[boundIdCount]);

    const uint32_t* instr = code + 5;
    while (instr < code + codeSizeInWords)
    {
        uint16_t op = uint16_t(instr[0]);
        uint16_t instrLen = uint16_t(instr[0] >> 16);
        switch (op)
        {
        case kSmolImpl_SpvOpEntryPoint:
        {
            if (instrLen < 2) return false;
            if (instr[1] != SmolImpl_SpvExecutionModelGLCompute)
                return false;
        }
            break;
        case kSmolImpl_SpvOpExecutionMode:
        {
            if (instrLen < 3) return false;
            uint32_t mode = instr[2];
            if (mode == SmolImpl_SpvExecutionModeLocalSize)
            {
                if (instrLen != 6) return false;
                kernel.localSize[0] = instr[3];
                kernel.localSize[1] = instr[4];
                kernel.localSize[2] = instr[5];
            }
        }
            break;
        case kSmolImpl_SpvOpDecorate:
        {
            if (instrLen < 3) return false;
            uint32_t id = instr[1];
            if (id >= boundIdCount)
                return false;
            if (instr[2] == SmolImpl_SpvDecorationDescriptorSet)
            {
                if (instrLen != 4) return false;
                ids[id].set = instr[3];
            }
            if (instr[2] == SmolImpl_SpvDecorationBinding)
            {
                if (instrLen != 4) return false;
                ids[id].binding = instr[3];
            }
            if (instr[2] == SmolImpl_SpvDecorationBlock)
            {
                ids[id].bufferBlock = false;
            }
            if (instr[2] == SmolImpl_SpvDecorationBufferBlock)
            {
                ids[id].bufferBlock = true;
            }
        }
            break;
        case kSmolImpl_SpvOpTypeStruct:
        case kSmolImpl_SpvOpTypeImage:
        case kSmolImpl_SpvOpTypeSampler:
        case kSmolImpl_SpvOpTypeSampledImage:
        {
            if (instrLen < 2) return false;
            uint32_t id = instr[1];
            if (id >= boundIdCount) return false;
            if (ids[id].op != 0) return false;
            ids[id].op = op;
        }
            break;
        case kSmolImpl_SpvOpTypePointer:
        {
            if (instrLen != 4) return false;
            uint32_t id = instr[1];
            if (id >= boundIdCount) return false;
            if (ids[id].op != 0) return false;
            ids[id].op = op;
            ids[id].typeId = instr[3];
            ids[id].storageClass = instr[2];
        }
            break;
        case kSmolImpl_SpvOpVariable:
        {
            if (instrLen < 4) return false;
            uint32_t id = instr[2];
            if (id >= boundIdCount) return false;
            if (ids[id].op != 0) return false;
            ids[id].op = op;
            ids[id].typeId = instr[1];
            ids[id].storageClass = instr[3];
        }
            break;
        }
        instr += instrLen;
    }

    // Now find which ones we're interested in (basically "buffers") and record that info into kernel
    for (uint32_t i = 0; i < boundIdCount; ++i)
    {
        Id& id = ids[i];
        if (id.op == kSmolImpl_SpvOpVariable && (id.storageClass == SmolImpl_SpvStorageClassUniform || id.storageClass == SmolImpl_SpvStorageClassUniformConstant || id.storageClass == SmolImpl_SpvStorageClassStorageBuffer))
        {
            if (id.set != 0)
                return false;
            if (id.binding >= SmolImpl_VkMaxResources)
                return false;
            // type of the variable should be a pointer
            if (ids[id.typeId].op != kSmolImpl_SpvOpTypePointer)
                return false;

            // get type of the pointer
            const Id& ptrTypeId = ids[ids[id.typeId].typeId];
            uint32_t typeKind = ptrTypeId.op;

            switch (typeKind)
            {
            case kSmolImpl_SpvOpTypeStruct:
                kernel.resourceTypes[id.binding] = ptrTypeId.bufferBlock ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                kernel.resourceMask |= 1 << id.binding;
                ++kernel.resourceCount;
                break;
            default:
                SMOL_ASSERT(!"Unsupported Vulkan resource type");
            }
        }
    }
    return true;
}


SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint, SmolKernelCreateFlags flags)
{
    // create shader module
    VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0, 0, shaderCodeSize, (const uint32_t*)shaderCode };
    VkShaderModule sm = 0;
    VkResult res = vkCreateShaderModule(s_VkDevice, &shaderModuleCreateInfo, 0, &sm);
    if (res != VK_SUCCESS)
        return nullptr;
    SmolKernel* kernel = new SmolKernel();
    kernel->kernel = sm;

    // parse SPIR-V to get resource bindings
    if (!SmolImpl_VkParseShaderResources((const uint32_t*)shaderCode, (uint32_t)(shaderCodeSize / 4), *kernel))
    {
        SmolKernelDelete(kernel);
        return nullptr;
    }

    // create descriptor set layout
    auto setBindings = std::unique_ptr<VkDescriptorSetLayoutBinding[]>(new VkDescriptorSetLayoutBinding[kernel->resourceCount]);
    uint32_t bindingIdx = 0;
    for (uint32_t i = 0; i < SmolImpl_VkMaxResources; ++i)
    {
        if (!(kernel->resourceMask & (1 << i)))
            continue;
        VkDescriptorSetLayoutBinding b = {};
        b.binding = i;
        b.descriptorType = kernel->resourceTypes[i];
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        setBindings[bindingIdx++] = b;
    }
    VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setCreateInfo.bindingCount = kernel->resourceCount;
    setCreateInfo.pBindings = setBindings.get();
    res = vkCreateDescriptorSetLayout(s_VkDevice, &setCreateInfo, 0, &kernel->dsLayout);
    if (res != VK_SUCCESS)
    {
        SmolKernelDelete(kernel);
        return nullptr;
    }

    // create pipeline layout
    VkPipelineLayoutCreateInfo pipeLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutCreateInfo.setLayoutCount = 1;
    pipeLayoutCreateInfo.pSetLayouts = &kernel->dsLayout;
    res = vkCreatePipelineLayout(s_VkDevice, &pipeLayoutCreateInfo, 0, &kernel->pipeLayout);
    if (res != VK_SUCCESS)
    {
        SmolKernelDelete(kernel);
        return nullptr;
    }

    // create pipeline
    VkComputePipelineCreateInfo pipeCreateInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = kernel->kernel;
    stage.pName = entryPoint;
    pipeCreateInfo.stage = stage;
    pipeCreateInfo.layout = kernel->pipeLayout;
    res = vkCreateComputePipelines(s_VkDevice, 0, 1, &pipeCreateInfo, 0, &kernel->pipeline);
    if (res != VK_SUCCESS)
    {
        SmolKernelDelete(kernel);
        return nullptr;
    }

    return kernel;
}

void SmolKernelDelete(SmolKernel* kernel)
{
    if (kernel == nullptr)
        return;
    if (kernel->kernel != nullptr)
        vkDestroyShaderModule(s_VkDevice, kernel->kernel, 0);
    if (kernel->dsLayout != nullptr)
        vkDestroyDescriptorSetLayout(s_VkDevice, kernel->dsLayout, 0);
    if (kernel->pipeLayout != nullptr)
        vkDestroyPipelineLayout(s_VkDevice, kernel->pipeLayout, 0);
    if (kernel->pipeline != nullptr)
        vkDestroyPipeline(s_VkDevice, kernel->pipeline, 0);
    delete kernel;
}

struct SmolImpl_VulkanState
{
    SmolKernel* kernel = nullptr;
    SmolBuffer* buffers[SmolImpl_VkMaxResources] = {};
};

static SmolImpl_VulkanState s_VkState;

void SmolKernelSet(SmolKernel* kernel)
{
    memset(s_VkState.buffers, 0, sizeof(s_VkState.buffers));
    s_VkState.kernel = kernel;
}

void SmolKernelSetBuffer(SmolBuffer* buffer, int index, SmolBufferBinding binding)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(buffer->buffer);
    SMOL_ASSERT(index >= 0 && index < SmolImpl_VkMaxResources);
    if (binding == SmolBufferBinding::Output)
        buffer->writtenByGpuSinceLastRead = true;
    s_VkState.buffers[index] = buffer;
}

static void SmolImpl_VkStartCmdBufferIfNeeded()
{
    if (s_VkCommandBuffer != nullptr)
        return;
    VkCommandBufferAllocateInfo cbAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAllocInfo.commandPool = s_VkCommandPool;
    cbAllocInfo.commandBufferCount = 1;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkResult res = vkAllocateCommandBuffers(s_VkDevice, &cbAllocInfo, &s_VkCommandBuffer);
    if (res != VK_SUCCESS)
        return;

    VkCommandBufferBeginInfo cbBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cbBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = vkBeginCommandBuffer(s_VkCommandBuffer, &cbBeginInfo);
    SMOL_ASSERT(res == VK_SUCCESS);
}


void SmolKernelDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ)
{
    SmolKernel* kernel = s_VkState.kernel;
    SMOL_ASSERT(kernel != nullptr && kernel->pipeline != nullptr);
    SMOL_ASSERT(kernel->localSize[0] == groupSizeX && kernel->localSize[1] == groupSizeY && kernel->localSize[2] == groupSizeZ);

    // allocate a descriptor set
    VkDescriptorSetAllocateInfo dsAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsAllocInfo.descriptorPool = s_VkDescriptorPool;
    dsAllocInfo.descriptorSetCount = 1;
    dsAllocInfo.pSetLayouts = &kernel->dsLayout;
    VkDescriptorSet ds = 0;
    VkResult res = vkAllocateDescriptorSets(s_VkDevice, &dsAllocInfo, &ds);
    if (res != VK_SUCCESS)
        return;

    // fill descriptor set with binding data
    VkDescriptorBufferInfo binfos[SmolImpl_VkMaxResources];
    VkWriteDescriptorSet wds[SmolImpl_VkMaxResources];
    memset(binfos, 0, sizeof(binfos[0]) * kernel->resourceCount);
    memset(wds, 0, sizeof(wds[0]) * kernel->resourceCount);
    uint32_t idx = 0;
    for (uint32_t i = 0; i < SmolImpl_VkMaxResources; ++i)
    {
        if (!(kernel->resourceMask & (1 << i)))
            continue;
        binfos[idx].buffer = s_VkState.buffers[i] ? s_VkState.buffers[i]->buffer : nullptr;
        binfos[idx].offset = 0;
        binfos[idx].range = VK_WHOLE_SIZE;
        wds[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[idx].dstSet = ds;
        wds[idx].dstBinding = i;
        wds[idx].descriptorCount = 1;
        wds[idx].descriptorType = kernel->resourceTypes[i];
        wds[idx].pBufferInfo = &binfos[idx];
        ++idx;
    }
    vkUpdateDescriptorSets(s_VkDevice, kernel->resourceCount, wds, 0, 0);

    // dispatch
    int groupsX = (threadsX + groupSizeX - 1) / groupSizeX;
    int groupsY = (threadsY + groupSizeY - 1) / groupSizeY;
    int groupsZ = (threadsZ + groupSizeZ - 1) / groupSizeZ;
    SmolImpl_VkStartCmdBufferIfNeeded();
    SMOL_ASSERT(s_VkCommandBuffer);
    vkCmdBindPipeline(s_VkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, kernel->pipeline);
    vkCmdBindDescriptorSets(s_VkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, kernel->pipeLayout, 0, 1, &ds, 0, 0);
    vkCmdDispatch(s_VkCommandBuffer, groupsX, groupsY, groupsZ);
}

void SmolCaptureStart()
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
    if (!s_RenderDocApi)
        return;
    s_RenderDocApi->StartFrameCapture(NULL, NULL);
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC
}

void SmolCaptureFinish()
{
#if SMOL_COMPUTE_ENABLE_RENDERDOC
    if (!s_RenderDocApi)
        return;
    s_RenderDocApi->EndFrameCapture(NULL, NULL);
#endif // #if SMOL_COMPUTE_ENABLE_RENDERDOC
}


#endif // #if SMOL_COMPUTE_VULKAN



// ------------------------------------------------------------------------------------------------
//  Metal

#if SMOL_COMPUTE_METAL
#if !__has_feature(objc_arc)
    #error Enable ARC for Metal
#endif
#include <TargetConditionals.h>
#import <Metal/Metal.h>

static id<MTLDevice> s_MetalDevice;
static id<MTLCommandQueue> s_MetalCmdQueue;
static id<MTLCommandBuffer> s_MetalCmdBuffer;
static id<MTLComputeCommandEncoder> s_MetalComputeEncoder;

static void MetalFlushActiveEncoders()
{
    if (s_MetalComputeEncoder != nil)
    {
        [s_MetalComputeEncoder endEncoding];
        s_MetalComputeEncoder = nil;
    }
}

static void MetalFinishWork()
{
    if (s_MetalCmdBuffer == nil)
        return;
    MetalFlushActiveEncoders();
    [s_MetalCmdBuffer commit];
    [s_MetalCmdBuffer waitUntilCompleted];
    s_MetalCmdBuffer = nil;
}

bool SmolComputeCreate(SmolComputeCreateFlags flags)
{
    s_MetalDevice = MTLCreateSystemDefaultDevice();
    s_MetalCmdQueue = [s_MetalDevice newCommandQueue];
    return true;
}

void SmolComputeDelete()
{
    MetalFinishWork();
    s_MetalCmdQueue = nil;
    s_MetalDevice = nil;
}

SmolBackend SmolComputeGetBackend()
{
    return SmolBackend::Metal;
}

struct SmolBuffer
{
    id<MTLBuffer> buffer;
    size_t size;
    bool writtenByGpuSinceLastRead = false;
};

SmolBuffer* SmolBufferCreate(size_t size, SmolBufferType type, size_t structElementSize)
{
    SmolBuffer* buf = new SmolBuffer();
    buf->buffer = [s_MetalDevice newBufferWithLength:size options:MTLResourceStorageModeManaged];
    buf->size = size;
    return buf;
}

void SmolBufferSetData(SmolBuffer* buffer, const void* src, size_t size, size_t dstOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(dstOffset + size <= buffer->size);
    uint8_t* dst = (uint8_t*)[buffer->buffer contents];
    memcpy(dst + dstOffset, src, size);
    [buffer->buffer didModifyRange: NSMakeRange(dstOffset, size)];
}

static void MetalBufferMakeGpuDataVisibleToCpu(SmolBuffer* buffer)
{
    SMOL_ASSERT(s_MetalCmdBuffer != nil);
    MetalFlushActiveEncoders();
    id<MTLBlitCommandEncoder> blit = [s_MetalCmdBuffer blitCommandEncoder];
    [blit synchronizeResource:buffer->buffer];
    [blit endEncoding];
}


void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(srcOffset + size <= buffer->size);
    if (buffer->writtenByGpuSinceLastRead)
    {
        MetalBufferMakeGpuDataVisibleToCpu(buffer);
        MetalFinishWork();
        buffer->writtenByGpuSinceLastRead = false;
    }
    const uint8_t* src = (const uint8_t*)[buffer->buffer contents];
    memcpy(dst, src + srcOffset, size);
}

void SmolBufferDelete(SmolBuffer* buffer)
{
    if (buffer == nullptr)
        return;
    SMOL_ASSERT(buffer->buffer != nil);
    buffer->buffer = nil;
    delete buffer;
}

static void StartCmdBufferIfNeeded()
{
    if (s_MetalCmdBuffer == nil)
        s_MetalCmdBuffer = [s_MetalCmdQueue commandBufferWithUnretainedReferences];
}

struct SmolKernel
{
    id<MTLComputePipelineState> kernel;
};

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint, SmolKernelCreateFlags flags)
{
    MTLCompileOptions* opt = [MTLCompileOptions new];
    opt.fastMathEnabled = HasFlag(flags, SmolKernelCreateFlags::EnableFastMath);

    NSString* srcStr = [[NSString alloc] initWithBytes: shaderCode length: shaderCodeSize encoding: NSASCIIStringEncoding];

    NSError* error = nil;
    id<MTLLibrary> lib = [s_MetalDevice newLibraryWithSource:srcStr options:opt error:&error];
    if (error != nil)
    {
        //@TODO
        //NSString* desc = [error localizedDescription];
        //NSString* reason = [error localizedFailureReason];
        //::printf_console("%s\n%s\n\n", desc ? [desc UTF8String] : "<unknown>", reason ? [reason UTF8String] : "");
        error = nil;
    }
    if (lib == nil)
        return nullptr;

    id<MTLFunction> func = [lib newFunctionWithName: [NSString stringWithUTF8String: entryPoint]];
    if (func == nil)
        return nullptr;

    MTLComputePipelineDescriptor* desc = [[MTLComputePipelineDescriptor alloc] init];
    desc.computeFunction = func;
    id<MTLComputePipelineState> pipe = [s_MetalDevice newComputePipelineStateWithDescriptor: desc options: MTLPipelineOptionNone reflection: nil error: &error];

    if (error != nil)
    {
        //@TODO
        //ReportErrorSimple(Format("Metal: Error creating kernel pipeline state: %s\n%s\n", [[error localizedDescription] UTF8String], [[error localizedFailureReason] UTF8String]));
        error = nil;
    }
    if (pipe == nil)
        return nullptr;

    SmolKernel* kernel = new SmolKernel();
    kernel->kernel = pipe;
    return kernel;
}

void SmolKernelDelete(SmolKernel* kernel)
{
    if (kernel == nullptr)
        return;
    kernel->kernel = nil;
    delete kernel;
}

void SmolKernelSet(SmolKernel* kernel)
{
    StartCmdBufferIfNeeded();
    if (s_MetalComputeEncoder == nil)
    {
        MetalFlushActiveEncoders();
        s_MetalComputeEncoder = [s_MetalCmdBuffer computeCommandEncoder];
    }
    [s_MetalComputeEncoder setComputePipelineState:kernel->kernel];
}

void SmolKernelSetBuffer(SmolBuffer* buffer, int index, SmolBufferBinding binding)
{
    SMOL_ASSERT(s_MetalComputeEncoder != nil);
    if (binding == SmolBufferBinding::Output)
        buffer->writtenByGpuSinceLastRead = true;
    [s_MetalComputeEncoder setBuffer:buffer->buffer offset:0 atIndex:index];
}

void SmolKernelDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ)
{
    SMOL_ASSERT(s_MetalComputeEncoder != nil);
    int groupsX = (threadsX + groupSizeX-1) / groupSizeX;
    int groupsY = (threadsY + groupSizeY-1) / groupSizeY;
    int groupsZ = (threadsZ + groupSizeZ-1) / groupSizeZ;
    [s_MetalComputeEncoder dispatchThreadgroups:MTLSizeMake(groupsX, groupsY, groupsZ) threadsPerThreadgroup:MTLSizeMake(groupSizeX,groupSizeY,groupSizeZ)];
}

void SmolCaptureStart()
{
    MTLCaptureManager* capture = [MTLCaptureManager sharedCaptureManager];
    [capture startCaptureWithDevice: s_MetalDevice];
}
void SmolCaptureFinish()
{
    MTLCaptureManager* capture = [MTLCaptureManager sharedCaptureManager];
    if ([capture isCapturing])
        [capture stopCapture];
}

#endif // #if SMOL_COMPUTE_METAL

#endif // #if SMOL_COMPUTE_IMPLEMENTATION
