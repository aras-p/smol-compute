#ifndef SMOL_GFX_INCLUDED
#define SMOL_GFX_INCLUDED

#include <stddef.h>


struct SmolBuffer;
struct SmolCompute;

bool SmolGfxInitialize();
void SmolGfxShutdown();

SmolBuffer* SmolBufferCreate(size_t size);
void SmolBufferDelete(SmolBuffer* buffer);
void SmolBufferSetData(SmolBuffer* buffer, const void* src, size_t size, size_t dstOffset = 0);
void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset = 0);
void SmolBufferMakeGpuDataVisibleToCpu(SmolBuffer* buffer);

SmolCompute* SmolComputeCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint);
void SmolComputeDestroy(SmolCompute* compute);
void SmolComputeSet(SmolCompute* compute);
void SmolComputeSetBuffer(SmolBuffer* buffer, int index, size_t bufferOffset = 0);
void SmolComputeDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ);
void SmolFinishWork();

void SmolStartCapture(const char* file);
void SmolFinishCapture();

#endif // #ifndef SMOL_GFX_INCLUDED



#if SMOL_IMPLEMENTATION

#ifndef SMOL_ASSERT
    #include <assert.h>
    #define SMOL_ASSERT(c) assert(c)
#endif


#if SMOL_METAL
#if !__has_feature(objc_arc)
    #error "Enable ARC for Metal"
#endif
#include <TargetConditionals.h>
#import <Metal/Metal.h>

static id<MTLDevice> s_MetalDevice;
static id<MTLCommandQueue> s_MetalCmdQueue;
static id<MTLCommandBuffer> s_MetalCmdBuffer;
static id<MTLComputeCommandEncoder> s_MetalComputeEncoder;

bool SmolGfxInitialize()
{
    s_MetalDevice = MTLCreateSystemDefaultDevice();
    s_MetalCmdQueue = [s_MetalDevice newCommandQueue];
    return true;
}

void SmolGfxShutdown()
{
    s_MetalCmdQueue = nil;
    s_MetalDevice = nil;
}

struct SmolBuffer
{
    id<MTLBuffer> buffer;
    size_t size;
};

SmolBuffer* SmolBufferCreate(size_t size)
{
    SmolBuffer* buf = new SmolBuffer();
    buf->buffer = [s_MetalDevice newBufferWithLength:size options:/*MTLResourceStorageModeManaged*/MTLResourceStorageModeShared];
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

void SmolBufferGetData(SmolBuffer* buffer, void* dst, size_t size, size_t srcOffset)
{
    SMOL_ASSERT(buffer);
    SMOL_ASSERT(srcOffset + size <= buffer->size);
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

static void FlushActiveEncoders()
{
    if (s_MetalComputeEncoder != nil)
    {
        [s_MetalComputeEncoder endEncoding];
        s_MetalComputeEncoder = nil;
    }
}

void SmolBufferMakeGpuDataVisibleToCpu(SmolBuffer* buffer)
{
    SMOL_ASSERT(s_MetalCmdBuffer != nil);
    FlushActiveEncoders();
    id<MTLBlitCommandEncoder> blit = [s_MetalCmdBuffer blitCommandEncoder];
    [blit synchronizeResource:buffer->buffer];
    [blit endEncoding];
}

static void StartCmdBufferIfNeeded()
{
    if (s_MetalCmdBuffer == nil)
        s_MetalCmdBuffer = [s_MetalCmdQueue commandBufferWithUnretainedReferences];
}

struct SmolCompute
{
    id<MTLComputePipelineState> compute;
};

SmolCompute* SmolComputeCreate(const void* shaderCode, size_t shaderCodeSize, const char* entryPoint)
{
    MTLCompileOptions* opt = [MTLCompileOptions new];
    opt.fastMathEnabled = true; //@TODO

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
        //ReportErrorSimple(Format("Metal: Error creating compute pipeline state: %s\n%s\n", [[error localizedDescription] UTF8String], [[error localizedFailureReason] UTF8String]));
        error = nil;
    }
    if (pipe == nil)
        return nullptr;

    SmolCompute* compute = new SmolCompute();
    compute->compute = pipe;
    return compute;
}

void SmolComputeDestroy(SmolCompute* compute)
{
    compute->compute = nil;
}

void SmolComputeSet(SmolCompute* compute)
{
    StartCmdBufferIfNeeded();
    if (s_MetalComputeEncoder == nil)
    {
        FlushActiveEncoders();
        s_MetalComputeEncoder = [s_MetalCmdBuffer computeCommandEncoder];
    }
    [s_MetalComputeEncoder setComputePipelineState:compute->compute];
}

void SmolComputeSetBuffer(SmolBuffer* buffer, int index, size_t bufferOffset)
{
    SMOL_ASSERT(s_MetalComputeEncoder != nil);
    [s_MetalComputeEncoder setBuffer:buffer->buffer offset:bufferOffset atIndex:index];
}

void SmolComputeDispatch(int threadsX, int threadsY, int threadsZ, int groupSizeX, int groupSizeY, int groupSizeZ)
{
    SMOL_ASSERT(s_MetalComputeEncoder != nil);
    int groupsX = (threadsX + groupSizeX-1) / groupSizeX;
    int groupsY = (threadsY + groupSizeY-1) / groupSizeY;
    int groupsZ = (threadsZ + groupSizeZ-1) / groupSizeZ;
    [s_MetalComputeEncoder dispatchThreadgroups:MTLSizeMake(groupsX, groupsY, groupsZ) threadsPerThreadgroup:MTLSizeMake(groupSizeX,groupSizeY,groupSizeZ)];
}

void SmolFinishWork()
{
    if (s_MetalCmdBuffer == nil)
        return;
    FlushActiveEncoders();
    [s_MetalCmdBuffer commit];
    [s_MetalCmdBuffer waitUntilCompleted];
    s_MetalCmdBuffer = nil;
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

#endif // #if SMOL_METAL
#endif // #if SMOL_IMPLEMENTATION
