#ifndef SMOL_COMPUTE_INCLUDED
#define SMOL_COMPUTE_INCLUDED

// smol-compute: a small library to run compute shaders on several graphics APIs.
//
// This is a single-header library; for regular usage just include this header,
// but you also have to do a:
//   #define SMOL_COMPUTE_IMPLEMENTATION 1
//   #define SMOL_COMPUTE_<graphicsapi> 1
//   #include "smolcompute.h"
// in one of your compiled files. Current implementations are for D3D11 (SMOL_COMPUTE_D3D11)
// and Metal (SMOL_COMPUTE_METAL).


#include <stddef.h>


struct SmolBuffer;
struct SmolKernel;


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


// Initialize the library. This has to be called before doing other work.
bool SmolComputeCreate();
// Shutdown the library.
void SmolComputeDelete();


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

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint);
void SmolKernelDelete(SmolKernel* kernel);
void SmolKernelSet(SmolKernel* kernel);
void SmolKernelSetBuffer(SmolBuffer* buffer, int index, SmolBufferBinding binding = SmolBufferBinding::Input);
void SmolKernelDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ);

void SmolStartCapture();
void SmolFinishCapture();

#endif // #ifndef SMOL_COMPUTE_INCLUDED



#if SMOL_COMPUTE_IMPLEMENTATION

#if !SMOL_COMPUTE_D3D11 && !SMOL_COMPUTE_METAL
#error Define one of SMOL_COMPUTE_D3D11 or SMOL_COMPUTE_METAL for a SMOL_COMPUTE_IMPLEMENTATION compile.
#endif

#ifndef SMOL_ASSERT
    #include <assert.h>
    #define SMOL_ASSERT(c) assert(c)
#endif


// ------------------------------------------------------------------------------------------------
//  D3D11


#if SMOL_COMPUTE_D3D11
#include <d3d11.h>
#include <d3dcompiler.h>

static ID3D11Device* s_D3D11Device;
static ID3D11DeviceContext* s_D3D11Context;

#define SMOL_RELEASE(o) { if (o) (o)->Release(); (o) = nullptr; }

bool SmolComputeCreate()
{
    HRESULT hr;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
#ifdef _DEBUG
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, levels, 1, D3D11_SDK_VERSION, &s_D3D11Device, NULL, &s_D3D11Context);
#endif
    if (s_D3D11Device == nullptr)
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, D3D11_SDK_VERSION, &s_D3D11Device, NULL, &s_D3D11Context);
    if (FAILED(hr))
        return false;
    return true;
}

void SmolComputeDelete()
{
    SMOL_RELEASE(s_D3D11Context);
    SMOL_RELEASE(s_D3D11Device);
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

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint)
{
    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompile(shaderCode, shaderCodeSize, "", NULL, NULL, entryPoint, "cs_5_0", D3DCOMPILE_IEEE_STRICTNESS, 0, &bytecode, &errors);
    if (FAILED(hr))
    {
        const char* errMsg = (const char*)errors->GetBufferPointer();
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

#endif // #if SMOL_COMPUTE_D3D11


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

bool SmolComputeCreate()
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

SmolKernel* SmolKernelCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint)
{
    MTLCompileOptions* opt = [MTLCompileOptions new];
    opt.fastMathEnabled = false;

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

void SmolStartCapture()
{
    MTLCaptureManager* capture = [MTLCaptureManager sharedCaptureManager];
    [capture startCaptureWithDevice: s_MetalDevice];
}
void SmolFinishCapture()
{
    MTLCaptureManager* capture = [MTLCaptureManager sharedCaptureManager];
    if ([capture isCapturing])
        [capture stopCapture];
}

#endif // #if SMOL_COMPUTE_METAL

#endif // #if SMOL_COMPUTE_IMPLEMENTATION
